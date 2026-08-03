#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome::levoit_classic_300s::protocol {

constexpr uint8_t PREAMBLE = 0xA5;
constexpr uint8_t COMMAND_FRAME_TYPE = 0x22;
constexpr uint8_t STATUS_FRAME_TYPE = 0x02;
constexpr uint8_t RESPONSE_FRAME_TYPE = 0x12;
constexpr size_t MAX_PAYLOAD_LENGTH = 64;

struct Frame {
  uint8_t type{0};
  uint8_t sequence{0};
  std::vector<uint8_t> payload;
};

enum class ParseResult : uint8_t {
  NONE,
  FRAME,
  CHECKSUM_ERROR,
  LENGTH_ERROR,
};

class FrameParser {
 public:
  ParseResult push(uint8_t byte, Frame &frame);
  void reset();

 protected:
  std::vector<uint8_t> buffer_;
  size_t expected_length_{0};
};

enum class OperatingMode : uint8_t {
  AUTO = 0,
  MANUAL = 1,
  UNKNOWN = 0xFF,
};

struct Status {
  bool power{false};
  bool tank_lifted{false};
  uint8_t target_humidity{0};
  uint8_t current_humidity{0};
  uint8_t temperature_celsius{0};
  OperatingMode mode{OperatingMode::UNKNOWN};
  uint8_t manual_mist_level{0};
  uint8_t night_light_percent{0};
  std::array<uint8_t, 17> raw{};
};

uint8_t checksum(const std::vector<uint8_t> &bytes);
bool has_valid_checksum(const std::vector<uint8_t> &frame);
std::vector<uint8_t> build_frame(uint8_t type, uint8_t sequence, const std::vector<uint8_t> &payload);

std::vector<uint8_t> power_payload(bool on);
std::vector<uint8_t> night_light_payload(uint8_t percent);
std::vector<uint8_t> manual_mist_payload(uint8_t level);
std::vector<uint8_t> auto_mode_payload(uint8_t target_humidity);
std::vector<uint8_t> status_request_payload();

bool decode_status(const Frame &frame, Status &status);

}  // namespace esphome::levoit_classic_300s::protocol
