#include "levoit_protocol.h"

#include <algorithm>

namespace esphome::levoit_classic_300s::protocol {

ParseResult FrameParser::push(uint8_t byte, Frame &frame) {
  if (this->buffer_.empty() && byte != PREAMBLE) {
    return ParseResult::NONE;
  }

  this->buffer_.push_back(byte);
  if (this->buffer_.size() == 5) {
    const size_t payload_length = this->buffer_[3] | (static_cast<size_t>(this->buffer_[4]) << 8U);
    if (payload_length > MAX_PAYLOAD_LENGTH) {
      this->reset();
      return ParseResult::LENGTH_ERROR;
    }
    this->expected_length_ = 6 + payload_length;
  }

  if (this->expected_length_ == 0 || this->buffer_.size() < this->expected_length_) {
    return ParseResult::NONE;
  }

  if (!has_valid_checksum(this->buffer_)) {
    this->reset();
    return ParseResult::CHECKSUM_ERROR;
  }

  frame.type = this->buffer_[1];
  frame.sequence = this->buffer_[2];
  frame.payload.assign(this->buffer_.begin() + 6, this->buffer_.end());
  this->reset();
  return ParseResult::FRAME;
}

void FrameParser::reset() {
  this->buffer_.clear();
  this->expected_length_ = 0;
}

uint8_t checksum(const std::vector<uint8_t> &bytes) {
  uint8_t sum = 0;
  for (const uint8_t byte : bytes) {
    sum = static_cast<uint8_t>(sum + byte);
  }
  return static_cast<uint8_t>(0xFFU - sum);
}

bool has_valid_checksum(const std::vector<uint8_t> &frame) {
  uint8_t sum = 0;
  for (const uint8_t byte : frame) {
    sum = static_cast<uint8_t>(sum + byte);
  }
  return sum == 0xFF;
}

std::vector<uint8_t> build_frame(uint8_t type, uint8_t sequence, const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> frame{PREAMBLE, type, sequence, static_cast<uint8_t>(payload.size() & 0xFFU),
                             static_cast<uint8_t>((payload.size() >> 8U) & 0xFFU)};

  std::vector<uint8_t> checksum_bytes = frame;
  checksum_bytes.insert(checksum_bytes.end(), payload.begin(), payload.end());
  frame.push_back(checksum(checksum_bytes));
  frame.insert(frame.end(), payload.begin(), payload.end());
  return frame;
}

std::vector<uint8_t> power_payload(bool on) { return {0x01, 0x00, 0xA0, 0x00, static_cast<uint8_t>(on)}; }

std::vector<uint8_t> night_light_payload(uint8_t percent) {
  const uint8_t confirmed_percent = percent == 0 ? 0 : (percent <= 50 ? 50 : 100);
  return {0x01, 0x03, 0xA0, 0x00, 0x01, confirmed_percent};
}

std::vector<uint8_t> manual_mist_payload(uint8_t level) {
  level = std::max<uint8_t>(1, std::min<uint8_t>(9, level));
  return {0x01, 0x60, 0xA2, 0x00, 0x00, 0x01, level};
}

std::vector<uint8_t> auto_mode_payload(uint8_t target_humidity) {
  target_humidity = std::max<uint8_t>(5, std::min<uint8_t>(250, target_humidity));
  return {0x01, 0x80, 0x40, 0x00, target_humidity, static_cast<uint8_t>(target_humidity - 5),
          static_cast<uint8_t>(target_humidity + 5), 0x09, 0x05, 0x01};
}

std::vector<uint8_t> status_request_payload() { return {0x01, 0x84, 0x40, 0x00}; }

bool decode_status(const Frame &frame, Status &status) {
  if (frame.type != STATUS_FRAME_TYPE || frame.payload.size() != 20 || frame.payload[0] != 0x01 ||
      frame.payload[1] != 0x85 || frame.payload[2] != 0x40) {
    return false;
  }

  std::copy_n(frame.payload.begin() + 3, status.raw.size(), status.raw.begin());
  status.power = status.raw[4] != 0;
  status.tank_lifted = status.raw[5] != 0;
  status.target_humidity = status.raw[10];
  status.current_humidity = status.raw[11];
  status.temperature_celsius = status.raw[12];
  switch (status.raw[13]) {
    case 0:
      status.mode = OperatingMode::AUTO;
      break;
    case 1:
      status.mode = OperatingMode::MANUAL;
      break;
    default:
      status.mode = OperatingMode::UNKNOWN;
      break;
  }
  status.manual_mist_level = status.raw[14];
  status.night_light_percent = status.raw[15];
  return true;
}

}  // namespace esphome::levoit_classic_300s::protocol
