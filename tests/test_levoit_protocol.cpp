#include "levoit_protocol.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using esphome::levoit_classic_300s::protocol::Frame;
using esphome::levoit_classic_300s::protocol::FrameParser;
using esphome::levoit_classic_300s::protocol::OperatingMode;
using esphome::levoit_classic_300s::protocol::ParseResult;
using esphome::levoit_classic_300s::protocol::Status;
using esphome::levoit_classic_300s::protocol::build_frame;
using esphome::levoit_classic_300s::protocol::decode_status;
using esphome::levoit_classic_300s::protocol::power_payload;

static void test_known_power_frame() {
  const std::vector<uint8_t> expected{0xA5, 0x22, 0x06, 0x05, 0x00, 0x8B, 0x01, 0x00, 0xA0, 0x00, 0x01};
  assert(build_frame(0x22, 0x06, power_payload(true)) == expected);
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

int main() {
  test_known_power_frame();
  test_stream_parser_ignores_noise_and_accepts_fragments();
  test_bad_checksum_is_rejected_then_parser_recovers();
  test_status_decode();
  test_unknown_mode_is_not_reported_as_auto();
  std::cout << "All Levoit protocol tests passed\n";
  return 0;
}
