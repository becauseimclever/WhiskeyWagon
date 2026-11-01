#pragma once

#include <array>

#include "bench_logic.hpp"

namespace whiskey_wagon {
struct EncoderSnapshot {
    EncoderCounter counts;
    uint64_t last_edge_us = 0;
};

void initialize_encoder_capture(bool capture_left = true);
std::array<EncoderSnapshot, 2> encoder_snapshot();
} // namespace whiskey_wagon
