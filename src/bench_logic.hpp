#pragma once

#include <cstdint>

namespace whiskey_wagon {
enum class LinkMode { kReconnect, kPairing, kConnected, kSleeping };

class LinkLifecycle {
public:
    constexpr void start(bool remembered, uint64_t now)
    {
        mode_ = remembered ? LinkMode::kReconnect : LinkMode::kPairing;
        since_ = now;
    }
    constexpr void connected() { mode_ = LinkMode::kConnected; }
    constexpr void disconnected(uint64_t now) { start(true, now); }
    constexpr void pair(uint64_t now)
    {
        // A repeated hold/request does not indefinitely extend an open window.
        if (mode_ != LinkMode::kPairing && mode_ != LinkMode::kSleeping) {
            mode_ = LinkMode::kPairing;
            since_ = now;
        }
    }
    constexpr void update(uint64_t now)
    {
        if (mode_ == LinkMode::kReconnect && now - since_ > 30000) {
            mode_ = LinkMode::kPairing;
            since_ = now;
        } else if (mode_ == LinkMode::kPairing && now - since_ >= 60000) {
            mode_ = LinkMode::kSleeping;
        }
    }
    constexpr LinkMode mode() const { return mode_; }

private:
    LinkMode mode_ = LinkMode::kReconnect;
    uint64_t since_ = 0;
};

enum class ButtonEvent { kNone, kShortPress, kHold };

class PairButton {
public:
    constexpr ButtonEvent update(bool pressed, uint64_t now)
    {
        if (pressed != raw_) {
            raw_ = pressed;
            changed_ = now;
        }
        if (raw_ != stable_ && now - changed_ >= 30) {
            stable_ = raw_;
            if (stable_) {
                down_ = now;
                held_ = false;
            } else if (!held_) {
                return ButtonEvent::kShortPress;
            }
        }
        if (stable_ && !held_ && now - down_ >= 2000) {
            held_ = true;
            return ButtonEvent::kHold;
        }
        return ButtonEvent::kNone;
    }

private:
    bool raw_ = false;
    bool stable_ = false;
    bool held_ = false;
    uint64_t changed_ = 0;
    uint64_t down_ = 0;
};

struct DriveInput {
    bool forward = false;
    bool reverse = false;
    bool stop = false;
    bool left = false;
    bool right = false;
};

constexpr float magnitude(float value) { return value < 0 ? -value : value; }

class SmoothRamp {
public:
    constexpr void reset(float value = 0)
    {
        value_ = start_ = target_ = value;
        elapsed_ = duration_ = 0;
    }
    constexpr float update(float target, uint32_t elapsed_ms, uint32_t full_travel_ms)
    {
        if (target != target_) {
            start_ = value_;
            target_ = target;
            elapsed_ = 0;
            duration_ = static_cast<uint32_t>(magnitude(target_ - start_) * full_travel_ms);
        }
        if (duration_ == 0 || elapsed_ms >= duration_ - elapsed_) {
            value_ = target_;
            elapsed_ = duration_;
        } else {
            elapsed_ += elapsed_ms;
            const float t = static_cast<float>(elapsed_) / duration_;
            value_ = start_ + (target_ - start_) * t * t * (3 - 2 * t);
        }
        return value_;
    }
    constexpr float value() const { return value_; }

private:
    float value_ = 0;
    float start_ = 0;
    float target_ = 0;
    uint32_t elapsed_ = 0;
    uint32_t duration_ = 0;
};

class BenchControl {
public:
    constexpr void connection_reset()
    {
        qualified_ = false;
        previous_ = {};
        winner_ = 0;
        steering_target_ = 0;
    }
    constexpr void report(DriveInput input)
    {
        steering_target_ = input.left == input.right ? 0 : input.left ? -1 : 1;
        if (!qualified_) {
            qualified_ = !input.forward && !input.reverse && !input.stop;
            previous_ = input;
            return;
        }
        if (input.stop && !previous_.stop) {
            winner_ = 0;
            coast_ = true;
            drive_.reset();
        } else if (input.forward && !previous_.forward) {
            winner_ = 1;
            coast_ = false;
        } else if (input.reverse && !previous_.reverse) {
            winner_ = -1;
            coast_ = false;
        }
        if ((winner_ > 0 && !input.forward) || (winner_ < 0 && !input.reverse)) {
            winner_ = 0;
        }
        previous_ = input;
    }
    constexpr void disconnect() { connection_reset(); }
    constexpr void sleep()
    {
        connection_reset();
        coast_ = true;
        drive_.reset();
        steering_.reset();
    }
    constexpr void tick(uint32_t elapsed_ms)
    {
        float target = static_cast<float>(winner_);
        if (target * drive_.value() < 0) {
            target = 0;
        }
        const bool slowing = target == 0;
        drive_.update(target, elapsed_ms, slowing ? 700 : 1000);
        steering_.update(steering_target_, elapsed_ms, 500);
    }
    constexpr float drive() const { return drive_.value(); }
    constexpr float steering() const { return steering_.value(); }
    constexpr bool qualified() const { return qualified_; }
    constexpr bool coast() const { return coast_; }

private:
    DriveInput previous_ = {};
    int winner_ = 0;
    float steering_target_ = 0;
    bool qualified_ = false;
    bool coast_ = false;
    SmoothRamp drive_;
    SmoothRamp steering_;
};

struct EncoderCounter {
    uint32_t a_edges = 0;
    uint32_t b_edges = 0;
    int64_t tentative_position = 0;
    uint32_t invalid_transitions = 0;
    uint8_t previous = 0;

    constexpr void sample(uint8_t state)
    {
        const auto changed = previous ^ state;
        a_edges += (changed & 2) != 0;
        b_edges += (changed & 1) != 0;
        if (changed == 3) {
            ++invalid_transitions;
        } else if (changed != 0) {
            constexpr int8_t steps[16] = {0, -1, 1, 0, 1, 0, 0, -1,
                                         -1, 0, 0, 1, 0, 1, -1, 0};
            tentative_position += steps[(previous << 2) | state];
        }
        previous = state;
    }
};
} // namespace whiskey_wagon
