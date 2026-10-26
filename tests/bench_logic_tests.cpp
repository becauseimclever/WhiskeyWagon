#include "bench_logic.hpp"

using namespace whiskey_wagon;

constexpr bool lifecycle_tests()
{
    LinkLifecycle link;
    link.start(false, 100);
    link.pair(1000);
    link.update(60099);
    if (link.mode() != LinkMode::kPairing) return false;
    link.update(60100);
    if (link.mode() != LinkMode::kSleeping) return false;
    link.update(999999);
    if (link.mode() != LinkMode::kSleeping) return false;
    link.start(true, 200000);
    link.update(230000);
    if (link.mode() != LinkMode::kReconnect) return false;
    link.update(230001);
    if (link.mode() != LinkMode::kPairing) return false;
    link.connected();
    link.update(999999);
    if (link.mode() != LinkMode::kConnected) return false;
    link.disconnected(1000000);
    return link.mode() == LinkMode::kReconnect;
}
static_assert(lifecycle_tests());

constexpr bool button_tests()
{
    PairButton button;
    button.update(true, 0);
    button.update(false, 10);
    if (button.update(false, 50) != ButtonEvent::kNone) return false;
    button.update(true, 100);
    button.update(true, 130);
    if (button.update(true, 2129) != ButtonEvent::kNone) return false;
    if (button.update(true, 2130) != ButtonEvent::kHold) return false;
    if (button.update(true, 5000) != ButtonEvent::kNone) return false;
    button.update(false, 5010);
    if (button.update(false, 5040) != ButtonEvent::kNone) return false;
    button.update(true, 6000);
    button.update(true, 6030);
    button.update(false, 6100);
    return button.update(false, 6130) == ButtonEvent::kShortPress;
}
static_assert(button_tests());

constexpr bool control_tests()
{
    BenchControl control;
    control.report({true});
    control.tick(1000);
    if (control.drive() != 0 || control.qualified()) return false;
    control.report({});
    control.report({true});
    control.tick(500);
    if (control.drive() != 0.5f) return false;
    control.tick(500);
    if (control.drive() != 1) return false;
    control.report({});
    control.tick(350);
    if (control.drive() != 0.5f) return false;
    control.tick(350);
    if (control.drive() != 0) return false;
    control.report({true, true});
    control.tick(1000);
    if (control.drive() != 1) return false;
    control.report({false, true});
    control.tick(700);
    if (control.drive() != 0) return false; // No fallback to older X.
    control.report({});
    control.report({false, true});
    control.tick(1000);
    if (control.drive() != -1) return false;
    control.report({true, true});
    control.tick(700);
    if (control.drive() != 0) return false; // Must pass through zero.
    control.tick(1000);
    if (control.drive() != 1) return false;
    control.report({true, true, true});
    if (control.drive() != 0 || !control.coast()) return false;
    control.report({true, true, false});
    control.tick(1000);
    if (control.drive() != 0) return false;
    control.report({});
    control.report({false, false, false, true});
    control.tick(500);
    if (control.steering() != -1) return false;
    control.report({false, false, false, true, true});
    control.tick(500);
    if (control.steering() != 0) return false;
    control.report({true, false, false, false, true});
    control.tick(1000);
    control.disconnect();
    control.tick(700);
    if (control.drive() != 0 || control.steering() != 0) return false;
    control.report({true});
    control.tick(1000);
    if (control.drive() != 0) return false;
    control.sleep();
    return control.drive() == 0 && control.steering() == 0;
}
static_assert(control_tests());

constexpr bool tie_and_reset_tests()
{
    BenchControl control;
    control.report({false, false, false, true});
    control.tick(500);
    if (control.steering() != -1) return false;
    control.report({true, true, true});
    control.tick(1000);
    if (!control.coast() || control.drive() != 0) return false;
    control.report({});
    control.report({true});
    control.tick(1000);
    if (control.drive() != 1) return false;
    control.connection_reset();
    control.report({false, true});
    control.tick(700);
    if (control.drive() != 0 || control.qualified()) return false;
    control.report({});
    control.report({false, true});
    control.tick(500);
    if (control.drive() != -0.5f) return false;
    control.report({false, true, true});
    return control.drive() == 0 && control.coast();
}
static_assert(tie_and_reset_tests());

constexpr bool independent_steering_tests()
{
    BenchControl control;
    control.report({true, false, false, false, true});
    control.tick(500);
    if (control.qualified() || control.drive() != 0 || control.steering() != 1) return false;
    control.report({});
    control.tick(500);
    control.report({true, false, false, true});
    control.tick(1000);
    control.report({true, false, true, true});
    if (control.drive() != 0 || control.steering() != -1) return false;
    control.tick(1000);
    return control.drive() == 0 && control.steering() == -1 && control.coast();
}
static_assert(independent_steering_tests());

constexpr bool encoder_tests()
{
    EncoderCounter counter;
    counter.sample(2);
    counter.sample(3);
    counter.sample(1);
    counter.sample(0);
    if (counter.tentative_position != 4 || counter.a_edges != 2 ||
        counter.b_edges != 2 || counter.invalid_transitions != 0) return false;
    counter.sample(1);
    counter.sample(3);
    counter.sample(2);
    counter.sample(0);
    if (counter.tentative_position != 0) return false;
    counter.sample(3);
    return counter.invalid_transitions == 1 && counter.tentative_position == 0;
}
static_assert(encoder_tests());
