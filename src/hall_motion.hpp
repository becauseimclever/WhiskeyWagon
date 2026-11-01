#pragma once

#include "bench_logic.hpp"

namespace whiskey_wagon {
enum class HallMotion { kNoRecentEdges, kPositive, kNegative, kUncertain };

class HallMotionTracker {
public:
    constexpr HallMotion update(const EncoderCounter& counts, uint64_t last_edge_us,
                                uint64_t now_us)
    {
        const auto positive = counts.positive_steps - previous_.positive_steps;
        const auto negative = counts.negative_steps - previous_.negative_steps;
        const auto invalid = counts.invalid_transitions - previous_.invalid_transitions;
        previous_ = counts;
        if (invalid || (positive && negative)) state_ = HallMotion::kUncertain;
        else if (positive) state_ = HallMotion::kPositive;
        else if (negative) state_ = HallMotion::kNegative;
        if (!last_edge_us || now_us < last_edge_us || now_us - last_edge_us >= 250000)
            state_ = HallMotion::kNoRecentEdges;
        return state_;
    }

private:
    EncoderCounter previous_;
    HallMotion state_ = HallMotion::kNoRecentEdges;
};
} // namespace whiskey_wagon
