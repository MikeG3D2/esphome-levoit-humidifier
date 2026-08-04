#include "levoit_protocol.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using esphome::levoit_classic_300s::protocol::Frame;
using esphome::levoit_classic_300s::protocol::FrameParser;
using esphome::levoit_classic_300s::protocol::MAX_PAYLOAD_LENGTH;
using esphome::levoit_classic_300s::protocol::OperatingMode;
using esphome::levoit_classic_300s::protocol::ParseResult;
using esphome::levoit_classic_300s::protocol::Status;
using esphome::levoit_classic_300s::protocol::build_frame;
using esphome::levoit_classic_300s::protocol::decode_status;
using esphome::levoit_classic_300s::protocol::auto_mode_payload;
using esphome::levoit_classic_300s::protocol::is_valid_target_humidity;
using esphome::levoit_classic_300s::protocol::manual_mist_payload;
using esphome::levoit_classic_300s::protocol::night_light_payload;
using esphome::levoit_classic_300s::protocol::power_payload;
using esphome::levoit_classic_300s::protocol::status_request_payload;

static void test_known_power_frame() {
  const std::vector<uint8_t> expected{0xA5, 0x22, 0x06, 0x05, 0x00, 0x8B, 0x01, 0x00, 0xA0, 0x00, 0x01};
  assert(build_frame(0x22, 0x06, power_payload(true)) == expected);
}

static void test_power_payloads() {
  assert(power_payload(false) == std::vector<uint8_t>({0x01, 0x00, 0xA0, 0x00, 0x00}));
  assert(power_payload(true) == std::vector<uint8_t>({0x01, 0x00, 0xA0, 0x00, 0x01}));
}

static void test_night_light_payloads() {
  assert(night_light_payload(0) == std::vector<uint8_t>({0x01, 0x03, 0xA0, 0x00, 0x01, 0x00}));
  assert(night_light_payload(50) == std::vector<uint8_t>({0x01, 0x03, 0xA0, 0x00, 0x01, 0x32}));
  assert(night_light_payload(100) == std::vector<uint8_t>({0x01, 0x03, 0xA0, 0x00, 0x01, 0x64}));
}

static void test_manual_mist_payloads() {
  assert(manual_mist_payload(1) == std::vector<uint8_t>({0x01, 0x60, 0xA2, 0x00, 0x00, 0x01, 0x01}));
  assert(manual_mist_payload(4) == std::vector<uint8_t>({0x01, 0x60, 0xA2, 0x00, 0x00, 0x01, 0x04}));
  assert(manual_mist_payload(9) == std::vector<uint8_t>({0x01, 0x60, 0xA2, 0x00, 0x00, 0x01, 0x09}));
}

static void test_captured_auto_mode_payloads() {
  assert(auto_mode_payload(63) ==
         std::vector<uint8_t>({0x01, 0x80, 0x40, 0x00, 0x3F, 0x3A, 0x44, 0x09, 0x05, 0x01}));
  assert(auto_mode_payload(37) ==
         std::vector<uint8_t>({0x01, 0x80, 0x40, 0x00, 0x25, 0x20, 0x2A, 0x09, 0x05, 0x01}));
}

static void test_status_request_payload() {
  assert(status_request_payload() == std::vector<uint8_t>({0x01, 0x84, 0x40, 0x00}));
}

static void test_auto_target_is_normalized_to_supported_range() {
  const std::vector<uint8_t> minimum{0x01, 0x80, 0x40, 0x00, 30, 25, 35, 0x09, 0x05, 0x01};
  const std::vector<uint8_t> maximum{0x01, 0x80, 0x40, 0x00, 80, 75, 85, 0x09, 0x05, 0x01};

  assert(auto_mode_payload(0) == minimum);
  assert(auto_mode_payload(29) == minimum);
  assert(auto_mode_payload(30) == minimum);
  assert(auto_mode_payload(80) == maximum);
  assert(auto_mode_payload(81) == maximum);
  assert(auto_mode_payload(255) == maximum);
}

static void test_target_humidity_validation_uses_supported_boundaries() {
  assert(!is_valid_target_humidity(0));
  assert(!is_valid_target_humidity(29));
  assert(is_valid_target_humidity(30));
  assert(is_valid_target_humidity(80));
  assert(!is_valid_target_humidity(81));
  assert(!is_valid_target_humidity(255));
}

static void test_stream_parser_ignores_noise_and_accepts_fragments() {
  const auto raw = build_frame(0x22, 0x07, power_payload(false));
  FrameParser parser;
  Frame frame;
  assert(parser.push(0x00, frame) == ParseResult::NONE);
  assert(parser.push(0xFF, frame) == ParseResult::NONE);

  for (size_t index = 0; index + 1 < raw.size(); index++) {
    assert(parser.push(raw[index], frame) == ParseResult::NONE);
  }
  assert(parser.push(raw.back(), frame) == ParseResult::FRAME);
  assert(frame.type == 0x22);
  assert(frame.sequence == 0x07);
  assert(frame.payload == power_payload(false));
}

static void test_bad_checksum_is_rejected_then_parser_recovers() {
  auto bad = build_frame(0x22, 0x08, power_payload(true));
  bad[5] ^= 0x01;
  FrameParser parser;
  Frame frame;
  for (size_t index = 0; index + 1 < bad.size(); index++) {
    assert(parser.push(bad[index], frame) == ParseResult::NONE);
  }
  assert(parser.push(bad.back(), frame) == ParseResult::CHECKSUM_ERROR);

  const auto good = build_frame(0x22, 0x09, power_payload(false));
  for (size_t index = 0; index + 1 < good.size(); index++) {
    assert(parser.push(good[index], frame) == ParseResult::NONE);
  }
  assert(parser.push(good.back(), frame) == ParseResult::FRAME);
  assert(frame.sequence == 0x09);
}

static void test_truncated_frame_followed_by_valid_frame_recovers_without_gap() {
  auto truncated = build_frame(0x22, 0x20, power_payload(true));
  truncated.resize(truncated.size() - 2);
  const auto good = build_frame(0x22, 0x21, power_payload(false));
  truncated.insert(truncated.end(), good.begin(), good.end());

  FrameParser parser;
  Frame frame;
  size_t decoded_frames = 0;
  for (const auto byte : truncated) {
    if (parser.push(byte, frame) == ParseResult::FRAME) {
      decoded_frames++;
    }
  }

  assert(decoded_frames == 1);
  assert(frame.sequence == 0x21);
  assert(frame.payload == power_payload(false));
}

static void test_wrong_plausible_length_does_not_hide_complete_valid_frame() {
  std::vector<uint8_t> stream{0xA5, 0x22, 0x30, 0x40, 0x00, 0x00, 0x11, 0x22};
  const auto good = build_frame(0x02, 0x31, {0xA5, 0x01});
  stream.insert(stream.end(), good.begin(), good.end());

  FrameParser parser;
  Frame frame;
  ParseResult last_result = ParseResult::NONE;
  for (const auto byte : stream) {
    last_result = parser.push(byte, frame);
  }

  assert(last_result == ParseResult::FRAME);
  assert(frame.sequence == 0x31);
  assert(frame.payload == std::vector<uint8_t>({0xA5, 0x01}));
}

static void test_oversized_header_followed_by_valid_frame_recovers_without_gap() {
  std::vector<uint8_t> stream{0xA5, 0x22, 0x40, 0x41, 0x00};
  const auto good = build_frame(0x22, 0x41, power_payload(true));
  stream.insert(stream.end(), good.begin(), good.end());

  FrameParser parser;
  Frame frame;
  size_t decoded_frames = 0;
  for (const auto byte : stream) {
    if (parser.push(byte, frame) == ParseResult::FRAME) {
      decoded_frames++;
    }
  }

  assert(decoded_frames == 1);
  assert(frame.sequence == 0x41);
}

static void test_preamble_byte_inside_valid_payload_is_preserved() {
  const std::vector<uint8_t> payload{0x01, 0xA5, 0x03, 0x04, 0x05, 0x06};
  const auto raw = build_frame(0x12, 0x42, payload);
  FrameParser parser;
  Frame frame;
  ParseResult result = ParseResult::NONE;
  for (const auto byte : raw) {
    result = parser.push(byte, frame);
  }

  assert(result == ParseResult::FRAME);
  assert(frame.payload == payload);
}

static void test_stream_buffer_remains_bounded_under_malformed_input() {
  FrameParser parser;
  Frame frame;
  const std::vector<uint8_t> malformed_header{0xA5, 0x22, 0x50, 0x40, 0x00};

  for (size_t cycle = 0; cycle < 200; cycle++) {
    for (const auto byte : malformed_header) {
      parser.push(byte, frame);
      assert(parser.buffered_size() <= MAX_PAYLOAD_LENGTH + 6);
    }
    for (size_t index = 0; index < MAX_PAYLOAD_LENGTH; index++) {
      parser.push(static_cast<uint8_t>(index), frame);
      assert(parser.buffered_size() <= MAX_PAYLOAD_LENGTH + 6);
    }
  }
}

static void test_status_decode() {
  const std::vector<uint8_t> payload{
      0x01, 0x85, 0x40,
      0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01,
      0x23, 0x35, 0x18, 0x01, 0x08, 0x32, 0x00,
  };
  const auto raw = build_frame(0x02, 0x11, payload);
  FrameParser parser;
  Frame frame;
  for (const auto byte : raw) {
    parser.push(byte, frame);
  }

  Status status;
  assert(decode_status(frame, status));
  assert(status.power);
  assert(!status.tank_lifted);
  assert(status.target_humidity == 35);
  assert(status.current_humidity == 53);
  assert(status.temperature_celsius == 24);
  assert(status.mode == OperatingMode::MANUAL);
  assert(status.manual_mist_level == 8);
  assert(status.night_light_percent == 50);
}

static void test_captured_no_water_status_is_separate_from_tank_lifted() {
  const std::vector<uint8_t> payload{
      0x01, 0x85, 0x40,
      0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
      0x25, 0x2F, 0x16, 0x00, 0x00, 0x64, 0x00,
  };
  Status status;
  assert(decode_status(Frame{0x02, 0x14, payload}, status));
  assert(!status.tank_lifted);
  assert(status.no_water);
}

static void test_captured_refill_clears_no_water_with_tank_seated() {
  const std::vector<uint8_t> payload{
      0x01, 0x85, 0x40,
      0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x25, 0x32, 0x16, 0x00, 0x00, 0x00, 0x00,
  };
  Status status;
  assert(decode_status(Frame{0x02, 0x14, payload}, status));
  assert(!status.tank_lifted);
  assert(!status.no_water);
}

static void test_status_decode_rejects_unrecognized_frames() {
  const std::vector<uint8_t> valid_payload{
      0x01, 0x85, 0x40,
      0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01,
      0x23, 0x35, 0x18, 0x01, 0x08, 0x32, 0x00,
  };
  Status status;
  assert(!decode_status(Frame{0x12, 0x60, valid_payload}, status));

  auto bad_prefix = valid_payload;
  bad_prefix[1] = 0x84;
  assert(!decode_status(Frame{0x02, 0x61, bad_prefix}, status));

  auto short_payload = valid_payload;
  short_payload.pop_back();
  assert(!decode_status(Frame{0x02, 0x62, short_payload}, status));
}

static void test_unknown_mode_is_not_reported_as_auto() {
  const std::vector<uint8_t> payload{
      0x01, 0x85, 0x40,
      0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01,
      0x23, 0x35, 0x18, 0x02, 0x08, 0x32, 0x00,
  };
  Frame frame{0x02, 0x12, payload};
  Status status;
  assert(decode_status(frame, status));
  assert(status.mode == OperatingMode::UNKNOWN);
  assert(status.raw[13] == 0x02);
}

static void test_status_preserves_invalid_target_for_diagnostics() {
  std::vector<uint8_t> payload{
      0x01, 0x85, 0x40,
      0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01,
      0x00, 0x35, 0x18, 0x01, 0x08, 0x32, 0x00,
  };
  Frame frame{0x02, 0x13, payload};
  Status status;
  assert(decode_status(frame, status));
  assert(status.target_humidity == 0);
  assert(status.raw[10] == 0);
  assert(!is_valid_target_humidity(status.target_humidity));
}

int main() {
  test_known_power_frame();
  test_power_payloads();
  test_night_light_payloads();
  test_manual_mist_payloads();
  test_captured_auto_mode_payloads();
  test_status_request_payload();
  test_auto_target_is_normalized_to_supported_range();
  test_target_humidity_validation_uses_supported_boundaries();
  test_stream_parser_ignores_noise_and_accepts_fragments();
  test_bad_checksum_is_rejected_then_parser_recovers();
  test_truncated_frame_followed_by_valid_frame_recovers_without_gap();
  test_wrong_plausible_length_does_not_hide_complete_valid_frame();
  test_oversized_header_followed_by_valid_frame_recovers_without_gap();
  test_preamble_byte_inside_valid_payload_is_preserved();
  test_stream_buffer_remains_bounded_under_malformed_input();
  test_status_decode();
  test_captured_no_water_status_is_separate_from_tank_lifted();
  test_captured_refill_clears_no_water_with_tank_seated();
  test_status_decode_rejects_unrecognized_frames();
  test_unknown_mode_is_not_reported_as_auto();
  test_status_preserves_invalid_target_for_diagnostics();
  std::cout << "All Levoit protocol tests passed\n";
  return 0;
}
