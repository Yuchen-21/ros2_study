#pragma once

#include "linux_ipc_lab/system.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace linux_ipc_lab
{

struct SensorSample
{
  std::uint32_t sequence{};
  std::uint64_t timestamp_ns{};
  float x{};
  float y{};
  float z{};
};

constexpr std::uint32_t kWireMagic = 0x52495043U;  // ASCII "RIPC".
constexpr std::uint16_t kWireVersion = 1U;
constexpr std::size_t kWireMessageSize = 32U;
using WireBuffer = std::array<std::uint8_t, kWireMessageSize>;

inline void put_u16(WireBuffer & bytes, const std::size_t offset, const std::uint16_t value)
{
  bytes.at(offset) = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
  bytes.at(offset + 1U) = static_cast<std::uint8_t>(value & 0xffU);
}

inline void put_u32(WireBuffer & bytes, const std::size_t offset, const std::uint32_t value)
{
  for (std::size_t index = 0U; index < 4U; ++index) {
    const unsigned int shift = static_cast<unsigned int>((3U - index) * 8U);
    bytes.at(offset + index) = static_cast<std::uint8_t>((value >> shift) & 0xffU);
  }
}

inline void put_u64(WireBuffer & bytes, const std::size_t offset, const std::uint64_t value)
{
  for (std::size_t index = 0U; index < 8U; ++index) {
    const unsigned int shift = static_cast<unsigned int>((7U - index) * 8U);
    bytes.at(offset + index) = static_cast<std::uint8_t>((value >> shift) & 0xffULL);
  }
}

[[nodiscard]] inline std::uint16_t get_u16(
  const WireBuffer & bytes, const std::size_t offset)
{
  return static_cast<std::uint16_t>(
    (static_cast<std::uint16_t>(bytes.at(offset)) << 8U) |
    static_cast<std::uint16_t>(bytes.at(offset + 1U)));
}

[[nodiscard]] inline std::uint32_t get_u32(
  const WireBuffer & bytes, const std::size_t offset)
{
  std::uint32_t value = 0U;
  for (std::size_t index = 0U; index < 4U; ++index) {
    value = static_cast<std::uint32_t>(
      (value << 8U) | static_cast<std::uint32_t>(bytes.at(offset + index)));
  }
  return value;
}

[[nodiscard]] inline std::uint64_t get_u64(
  const WireBuffer & bytes, const std::size_t offset)
{
  std::uint64_t value = 0U;
  for (std::size_t index = 0U; index < 8U; ++index) {
    value = (value << 8U) | static_cast<std::uint64_t>(bytes.at(offset + index));
  }
  return value;
}

inline void put_float(WireBuffer & bytes, const std::size_t offset, const float value)
{
  static_assert(sizeof(float) == sizeof(std::uint32_t), "lab protocol requires 32-bit float");
  static_assert(
    std::numeric_limits<float>::is_iec559,
    "lab protocol requires IEEE-754 float representation");
  std::uint32_t bits = 0U;
  std::memcpy(&bits, &value, sizeof(bits));
  put_u32(bytes, offset, bits);
}

[[nodiscard]] inline float get_float(const WireBuffer & bytes, const std::size_t offset)
{
  const std::uint32_t bits = get_u32(bytes, offset);
  float value = 0.0F;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

[[nodiscard]] inline WireBuffer encode_sample(const SensorSample & sample)
{
  WireBuffer bytes{};
  put_u32(bytes, 0U, kWireMagic);
  put_u16(bytes, 4U, kWireVersion);
  put_u16(bytes, 6U, 0U);  // Reserved flags keep protocol evolution explicit.
  put_u32(bytes, 8U, sample.sequence);
  put_u64(bytes, 12U, sample.timestamp_ns);
  put_float(bytes, 20U, sample.x);
  put_float(bytes, 24U, sample.y);
  put_float(bytes, 28U, sample.z);
  return bytes;
}

[[nodiscard]] inline SensorSample decode_sample(const WireBuffer & bytes)
{
  if (get_u32(bytes, 0U) != kWireMagic) {
    throw std::runtime_error("wire message has an invalid magic value");
  }
  if (get_u16(bytes, 4U) != kWireVersion) {
    throw std::runtime_error("wire message has an unsupported version");
  }

  SensorSample sample{};
  sample.sequence = get_u32(bytes, 8U);
  sample.timestamp_ns = get_u64(bytes, 12U);
  sample.x = get_float(bytes, 20U);
  sample.y = get_float(bytes, 24U);
  sample.z = get_float(bytes, 28U);
  return sample;
}

[[nodiscard]] inline SensorSample make_sample(const std::uint32_t sequence)
{
  const float base = static_cast<float>(sequence);
  return SensorSample{sequence, monotonic_now_ns(), base, base + 0.25F, -base - 0.5F};
}

[[nodiscard]] inline std::uint64_t sample_age_us(const SensorSample & sample)
{
  const std::uint64_t now = monotonic_now_ns();
  // Both endpoints are on one Linux host in Stage 01. Guarding underflow makes
  // a clock-domain bug visible as zero instead of a huge unsigned duration.
  return now >= sample.timestamp_ns ? (now - sample.timestamp_ns) / 1000U : 0U;
}

[[nodiscard]] inline std::string describe_sample(
  const std::string & prefix, const SensorSample & sample)
{
  std::ostringstream output;
  output << prefix << " seq=" << sample.sequence
         << " xyz=(" << std::fixed << std::setprecision(2)
         << sample.x << ',' << sample.y << ',' << sample.z << ')'
         << " latency_us=" << sample_age_us(sample);
  return output.str();
}

class SequenceTracker
{
public:
  void observe(const std::uint32_t sequence)
  {
    if (!initialized_) {
      initialized_ = true;
      if (sequence > 0U) {
        gaps_ += sequence;
      }
      expected_ = sequence + 1U;
      return;
    }

    if (sequence > expected_) {
      gaps_ += sequence - expected_;
      expected_ = sequence + 1U;
    } else if (sequence == expected_) {
      ++expected_;
    } else {
      ++out_of_order_or_duplicate_;
    }
  }

  [[nodiscard]] std::uint64_t gaps() const noexcept {return gaps_;}
  [[nodiscard]] std::uint64_t reordered_or_duplicate() const noexcept
  {
    return out_of_order_or_duplicate_;
  }

private:
  bool initialized_{false};
  std::uint32_t expected_{0U};
  std::uint64_t gaps_{0U};
  std::uint64_t out_of_order_or_duplicate_{0U};
};

}  // namespace linux_ipc_lab
