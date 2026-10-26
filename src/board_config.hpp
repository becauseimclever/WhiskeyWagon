#pragma once

#include <array>

namespace whiskey_wagon {
inline constexpr unsigned int kStandbyPin = 14;
inline constexpr std::array<unsigned int, 4> kMotorInputPins = {6, 7, 8, 9};
inline constexpr std::array<unsigned int, 4> kEncoderPins = {10, 11, 12, 13};
inline constexpr unsigned int kPairingButtonPin = 15;
inline constexpr bool kMotorOutputsEnabled = false;
// Candidate positional mappings only, to be checked against physical Micro labels.
inline constexpr unsigned int kForwardButtonMask = 1u << 0;
inline constexpr unsigned int kReverseButtonMask = 1u << 3;
inline constexpr unsigned int kCoastButtonMask = 1u << 2;
} // namespace whiskey_wagon
