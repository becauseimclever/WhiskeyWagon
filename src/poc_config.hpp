#pragma once

#include <array>
#include <cstdint>

namespace whiskey_wagon {
struct DriveMotorProfile {
    unsigned int gearbox_reduction;
    unsigned int rated_output_rpm;
    unsigned int supply_mv;
    unsigned int running_ma;
    unsigned int stall_ma;
};
inline constexpr std::array<DriveMotorProfile, 2> kDriveMotorProfiles = {{
    // Reduction from 2026-11-01 tachometer readings (disc/output ~162-166), not the 171 label.
    {164, 35, 6000, 150, 500}, {164, 35, 6000, 150, 500},
}};
// x4 quadrature counts per motor-shaft revolution (11-pulse Hall), tachometer-confirmed.
inline constexpr uint32_t kEncoderCountsPerMotorRev = 44;

// Signed rpm in tenths from a count delta; reduction 1 gives motor-shaft rpm.
constexpr int32_t encoder_rpm_tenths(int64_t delta_counts, uint32_t elapsed_ms,
                                     unsigned int reduction = 1)
{
    if (elapsed_ms == 0 || reduction == 0) return 0;
    return static_cast<int32_t>(delta_counts * 600000 /
                                (static_cast<int64_t>(elapsed_ms) *
                                 kEncoderCountsPerMotorRev * reduction));
}
inline constexpr uint16_t kPocMaxDuty = WHISKEY_POC_MAX_DUTY_PERMILLE;
inline constexpr uint32_t kPocAccelerationMs = WHISKEY_POC_ACCELERATION_MS;
inline constexpr uint32_t kPocReportTimeoutMs = WHISKEY_POC_REPORT_TIMEOUT_MS;
// A controller that sends nothing this long after connecting is treated as at rest.
inline constexpr uint32_t kPocQuietNeutralMs = 500;
inline constexpr uint16_t kServoLeftUs = WHISKEY_POC_SERVO_LEFT_US;
inline constexpr uint16_t kServoCenterUs = WHISKEY_POC_SERVO_CENTER_US;
inline constexpr uint16_t kServoRightUs = WHISKEY_POC_SERVO_RIGHT_US;
inline constexpr std::array<bool, 2> kMotorInverted = {
    WHISKEY_POC_LEFT_INVERTED != 0, WHISKEY_POC_RIGHT_INVERTED != 0};
inline constexpr uint32_t kReversalDwellMs = 250;
inline constexpr uint32_t kReversalTimeoutMs = 2000;
inline constexpr uint32_t kHallSilenceMs = 500;
inline constexpr uint16_t kHallMonitoringDuty = 100;
inline constexpr uint32_t kControlOverrunMs = 100;
static_assert(kPocMaxDuty >= kHallMonitoringDuty && kPocMaxDuty <= 1000);
static_assert(kPocAccelerationMs >= 100 && kPocAccelerationMs <= 1000);
static_assert(kPocReportTimeoutMs >= 100 && kPocReportTimeoutMs <= 2000);
static_assert(kServoLeftUs >= 1000 && kServoLeftUs <= 2000);
static_assert(kServoCenterUs >= 1000 && kServoCenterUs <= 2000);
static_assert(kServoRightUs >= 1000 && kServoRightUs <= 2000);
static_assert((kServoLeftUs < kServoCenterUs && kServoCenterUs < kServoRightUs) ||
              (kServoRightUs < kServoCenterUs && kServoCenterUs < kServoLeftUs));
} // namespace whiskey_wagon
