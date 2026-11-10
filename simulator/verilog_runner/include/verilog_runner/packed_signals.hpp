#pragma once

#include <cstdint>
#include <type_traits>

namespace simulator::signals {

// SystemVerilog packed structs place the last declared member in the low bits.
// ConfigData and ElasticConfigData keep op/constant in their low 36 bits.
// ElasticWire remains an internal record with scalar directional port bindings.
template <class Packed>
void SetBits(Packed& signal, unsigned offset, unsigned width, uint32_t value) {
  for (unsigned bit = 0; bit < width; ++bit) {
    const unsigned position = offset + bit;
    if constexpr (std::is_integral_v<Packed>) {
      const uint64_t mask = uint64_t{1} << position;
      signal = (signal & ~mask) | (uint64_t((value >> bit) & 1) << position);
    } else {
      const uint32_t mask = uint32_t{1} << (position % 32);
      signal[position / 32] = (signal[position / 32] & ~mask) |
                              (((value >> bit) & 1) << (position % 32));
    }
  }
}

template <class Packed>
uint32_t GetBits(const Packed& signal, unsigned offset, unsigned width) {
  uint32_t value = 0;
  for (unsigned bit = 0; bit < width; ++bit) {
    const unsigned position = offset + bit;
    if constexpr (std::is_integral_v<Packed>) {
      value |= ((uint64_t(signal) >> position) & 1) << bit;
    } else {
      value |= ((signal[position / 32] >> (position % 32)) & 1) << bit;
    }
  }
  return value;
}

}  // namespace simulator::signals
