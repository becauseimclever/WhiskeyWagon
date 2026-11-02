#include "bluetooth_adapter.h"

#include <stdio.h>
#include <string.h>

#include "btstack_tlv.h"
#include "pico/cyw43_arch.h"
#include "uni.h"

#define CONTROLLER_TAG 0x57574331u // WWC1, separate from SDK bonding/property tags.

static const char* status = "initializing";
static bool ready;
static bool remembered;
static bool sleeping;
static BluetoothMode mode = BLUETOOTH_RECONNECT;
static bd_addr_t last_address;
static uni_hid_device_t* active;
static const btstack_tlv_t* storage;
static void* storage_context;
static BluetoothInput input;
static BluetoothInputHandler input_handler;
static btstack_packet_callback_registration_t hci_events;

static bool address_allowed(bd_addr_t address)
{
    return !sleeping && (mode == BLUETOOTH_PAIRING ||
                        (remembered && bd_addr_cmp(address, last_address) == 0));
}

static void on_hci_event(uint8_t packet_type, uint16_t channel, uint8_t* packet,
                        uint16_t size)
{
    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }
    if (hci_event_packet_get_type(packet) == BTSTACK_EVENT_POWERON_FAILED) {
        status = "failed";
        printf("error: Bluetooth HCI power-on failed; control shutdown required\n");
    } else if (hci_event_packet_get_type(packet) == BTSTACK_EVENT_STATE &&
               btstack_event_state_get_state(packet) == HCI_STATE_OFF) {
        status = sleeping ? "sleeping" : "failed";
        if (sleeping) {
            printf("Bluetooth: HCI off confirmed; software sleep\n");
        } else {
            printf("error: unexpected Bluetooth HCI off; control shutdown required\n");
        }
    }
}

static void platform_init(int argc, const char** argv)
{
    (void)argc;
    (void)argv;
    uni_bt_allow_incoming_connections(false);
    uni_bt_service_set_enabled(false);
    printf("Bluetooth: initializing wagon platform; fresh input required\n");
}

static void on_init_complete(void)
{
    btstack_tlv_get_instance(&storage, &storage_context);
    if (!storage) {
        status = "failed";
        printf("error: Bluetooth persistent storage unavailable\n");
        return;
    }
    uint8_t record[7] = {0};
    const int length = storage->get_tag(storage_context, CONTROLLER_TAG, record, sizeof(record));
    if (length != 0 && (length != sizeof(record) || record[0] != 1)) {
        status = "failed";
        printf("error: invalid remembered-controller record (length=%d)\n", length);
        return;
    }
    remembered = length == sizeof(record);
    if (remembered) {
        memcpy(last_address, &record[1], sizeof(last_address));
        const bd_addr_t zero_address = {0};
        if (bd_addr_cmp(last_address, zero_address) == 0) {
            status = "failed";
            printf("error: invalid zero remembered-controller address\n");
            return;
        }
        printf("Bluetooth: remembered controller %s\n", bd_addr_to_str(last_address));
    }
    uni_bt_stop_scanning_unsafe();
    gap_connectable_control(0);
    gap_discoverable_control(0);
    ready = true;
    status = "ready";
    printf("Bluetooth: ready; remembered=%u\n", remembered);
}

static uni_error_t on_device_discovered(bd_addr_t address, const char* name,
                                       uint16_t device_class, uint8_t rssi)
{
    (void)device_class;
    (void)rssi;
    if (!active && address_allowed(address)) {
        printf("Bluetooth: candidate address=%s name=%s\n",
               bd_addr_to_str(address), name ? name : "(unknown)");
        return UNI_ERROR_SUCCESS;
    }
    return UNI_ERROR_IGNORE_DEVICE;
}

static void on_device_connected(uni_hid_device_t* device)
{
    printf("Bluetooth: transport connected %s; awaiting HID readiness\n",
           bd_addr_to_str(device->conn.btaddr));
}

static void on_device_disconnected(uni_hid_device_t* device)
{
    if (device == active) {
        active = NULL;
        input_handler(NULL);
        memset(&input, 0, sizeof(input));
        printf("Bluetooth: controller lost; release-ramp handling on next control tick\n");
    }
}

static uni_error_t on_device_ready(uni_hid_device_t* device)
{
    if (active || !uni_hid_device_is_gamepad(device) ||
        !address_allowed(device->conn.btaddr)) {
        printf("Bluetooth: rejecting controller %s\n", bd_addr_to_str(device->conn.btaddr));
        return UNI_ERROR_IGNORE_DEVICE;
    }
    // HID readiness defines successful replacement; keep the previous record until this point.
    if (!remembered || bd_addr_cmp(last_address, device->conn.btaddr) != 0) {
        uint8_t record[7] = {1};
        uint8_t check[7] = {0};
        memcpy(&record[1], device->conn.btaddr, sizeof(bd_addr_t));
        if (storage->store_tag(storage_context, CONTROLLER_TAG, record, sizeof(record)) != 0 ||
            storage->get_tag(storage_context, CONTROLLER_TAG, check, sizeof(check)) != sizeof(check) ||
            memcmp(record, check, sizeof(record)) != 0) {
            status = "failed";
            printf("error: failed to persist controller; connection rejected\n");
            return UNI_ERROR_IGNORE_DEVICE;
        }
        memcpy(last_address, device->conn.btaddr, sizeof(last_address));
        remembered = true;
    }
    active = device;
    input_handler(NULL);
    mode = BLUETOOTH_CONNECTED;
    memset(&input, 0, sizeof(input));
    uni_bt_stop_scanning_unsafe();
    uni_bt_allow_incoming_connections(false);
    gap_connectable_control(0);
    printf("Bluetooth: controller ready address=%s name=%s vid=%04x pid=%04x; "
#if WHISKEY_WAGON_POC
           "first press drives once idle 500 ms; POC mapping must be verified\n",
#else
           "release B/X/Y then press to preview; mapping UNVERIFIED\n",
#endif
           bd_addr_to_str(device->conn.btaddr), device->name,
           device->vendor_id, device->product_id);
    return UNI_ERROR_SUCCESS;
}

static void on_controller_data(uni_hid_device_t* device, uni_controller_t* controller)
{
    if (device != active || sleeping || controller->klass != UNI_CONTROLLER_CLASS_GAMEPAD) {
        printf("error: unexpected controller report rejected\n");
        return;
    }
    const uint32_t now = btstack_run_loop_get_time_ms();
    const uint32_t gap = now - input.last_report_ms;
    if (input.reports && gap > input.max_gap_ms) {
        input.max_gap_ms = gap;
    }
    input.last_report_ms = now;
    ++input.reports;
    input.buttons = controller->gamepad.buttons;
    input.dpad = controller->gamepad.dpad;
    input.misc = controller->gamepad.misc_buttons;
    input.axes[0] = controller->gamepad.axis_x;
    input.axes[1] = controller->gamepad.axis_y;
    input.axes[2] = controller->gamepad.axis_rx;
    input.axes[3] = controller->gamepad.axis_ry;
    input_handler(&input);
}

static void on_oob_event(uni_platform_oob_event_t event, void* data)
{
    (void)data;
    if (event == UNI_PLATFORM_OOB_BLUETOOTH_ENABLED) {
        printf("Bluetooth: scanning=%u\n", uni_bt_is_scanning());
    }
}

static struct uni_platform platform = {
#if WHISKEY_WAGON_POC
    .name = "WhiskeyWagon open-loop POC",
#else
    .name = "WhiskeyWagon inhibited bench",
#endif
    .init = platform_init,
    .on_init_complete = on_init_complete,
    .on_device_discovered = on_device_discovered,
    .on_device_connected = on_device_connected,
    .on_device_disconnected = on_device_disconnected,
    .on_device_ready = on_device_ready,
    .on_controller_data = on_controller_data,
    .on_oob_event = on_oob_event,
};

bool bluetooth_initialize(BluetoothInputHandler handler)
{
    input_handler = handler;
    const int result = cyw43_arch_init();
    if (result != 0) {
        status = "failed";
        printf("error: CYW43 initialization failed (%d)\n", result);
        return false;
    }
    hci_events.callback = on_hci_event;
    hci_add_event_handler(&hci_events);
    uni_platform_set_custom(&platform);
    if (uni_init(0, NULL) != 0) {
        status = "failed";
        printf("error: Bluepad32 initialization failed\n");
        return false;
    }
    gap_connectable_control(0);
    gap_discoverable_control(0);
    return true;
}

const char* bluetooth_status(void) { return status; }
bool bluetooth_ready(void) { return ready; }
bool bluetooth_connected(void) { return active != NULL; }
bool bluetooth_remembered(void) { return remembered; }
const BluetoothInput* bluetooth_input(void) { return &input; }

void bluetooth_set_mode(BluetoothMode next)
{
    mode = next;
    if (next == BLUETOOTH_SLEEPING) {
        sleeping = true;
        uni_bt_stop_scanning_unsafe();
        uni_bt_allow_incoming_connections(false);
        gap_connectable_control(0);
        if (active) {
            uni_hid_device_disconnect(active);
            active = NULL;
            input_handler(NULL);
        }
        if (hci_power_control(HCI_POWER_OFF) != 0) {
            printf("error: HCI power-off request failed\n");
            status = "failed";
        } else {
            status = "sleep_requested";
        }
        return;
    }
    if (next == BLUETOOTH_CONNECTED) {
        return;
    }
    if (active) {
        uni_hid_device_disconnect(active);
        active = NULL;
        input_handler(NULL);
    }
    // Keep an active inquiry running while changing its allowlist: BTstack
    // cancels inquiries asynchronously and rejects an immediate restart.
    if (!uni_bt_allowlist_remove_all()) {
        status = "failed";
        printf("error: Bluetooth allowlist reset failed; control shutdown required\n");
        return;
    }
    if (remembered && !uni_bt_allowlist_add_addr(last_address)) {
        status = "failed";
        printf("error: Bluetooth remembered-controller filter failed; control shutdown required\n");
        return;
    }
    uni_bt_allowlist_set_enabled(next == BLUETOOTH_RECONNECT);
    uni_bt_allow_incoming_connections(true);
    gap_connectable_control(1);
    gap_discoverable_control(0);
    uni_bt_start_scanning_and_autoconnect_unsafe();
    printf("Bluetooth: mode=%s; window_ms=%u; first ready gamepad wins; fresh input required\n",
           next == BLUETOOTH_PAIRING ? "pairing" : "reconnect",
           next == BLUETOOTH_PAIRING ? 60000u : 30000u);
}
