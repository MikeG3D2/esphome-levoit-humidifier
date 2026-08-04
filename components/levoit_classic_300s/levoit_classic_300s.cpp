#include "levoit_classic_300s.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace esphome::levoit_classic_300s {

static const char *const TAG = "levoit_classic_300s";

fan::FanTraits LevoitHumidifierFan::get_traits() {
  fan::FanTraits traits;
  traits.set_speed(true);
  traits.set_supported_speed_count(9);
  return traits;
}

void LevoitHumidifierFan::control(const fan::FanCall &call) {
  if (call.get_state().has_value()) {
    this->parent_->set_power(call.get_state().value());
  }
  if (call.get_speed().has_value()) {
    this->parent_->set_manual_mist_level(static_cast<uint8_t>(call.get_speed().value()));
  }
}

void LevoitModeSelect::control(size_t index) {
  if (index == 0) {
    this->parent_->set_auto_mode(0);
  } else if (index == 1) {
    this->parent_->set_manual_mist_level(0);
  } else {
    ESP_LOGW(TAG, "Unknown is a reported-only mode and cannot be selected");
  }
}

void LevoitTargetHumidityNumber::control(float value) {
  const auto target = static_cast<uint8_t>(std::lround(value));
  this->parent_->set_auto_mode(target);
}

light::LightTraits LevoitNightLight::get_traits() {
  light::LightTraits traits;
  traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  return traits;
}

void LevoitNightLight::write_state(light::LightState *state) {
  uint8_t percent = 0;
  if (state->remote_values.is_on()) {
    percent = state->remote_values.get_brightness() < 0.75f ? 50 : 100;
  }
  // LightState writes its restore/default value during boot. The MCU is the
  // source of truth, so do not change the physical light before first status.
  if (this->last_commanded_percent_ < 0) {
    this->last_commanded_percent_ = percent;
    return;
  }
  if (this->last_commanded_percent_ == percent) {
    return;
  }
  this->last_commanded_percent_ = percent;
  this->parent_->set_night_light_brightness(percent);
}

void LevoitNightLight::sync_brightness(uint8_t percent) {
  percent = protocol::normalize_night_light_percent(percent);
  this->last_commanded_percent_ = percent;
  if (this->state_ == nullptr) {
    return;
  }
  auto call = this->state_->make_call();
  call.set_state(percent != 0);
  if (percent != 0) {
    call.set_brightness(percent / 100.0f);
  }
  call.set_transition_length(0);
  call.perform();
}

void LevoitClassic300S::setup() {
  if (this->communication_problem_sensor_ != nullptr) {
    this->communication_problem_sensor_->publish_initial_state(false);
  }
  this->set_timeout("initial_status", 1000, [this]() { this->request_status_(); });
}

void LevoitClassic300S::loop() {
  while (this->available()) {
    uint8_t byte;
    if (!this->read_byte(&byte)) {
      break;
    }

    protocol::Frame frame;
    const auto result = this->parser_.push(byte, frame);
    if (result == protocol::ParseResult::FRAME) {
      this->process_frame_(frame);
    } else if (result == protocol::ParseResult::CHECKSUM_ERROR) {
      ESP_LOGW(TAG, "Discarded UART frame with invalid checksum");
    } else if (result == protocol::ParseResult::LENGTH_ERROR) {
      ESP_LOGW(TAG, "Discarded UART frame with oversized payload");
    }
  }

  const uint32_t now = millis();
  if (this->communication_state_.check_timeout(now)) {
    ESP_LOGW(TAG, "MCU did not return a valid status before the response timeout");
    this->status_set_warning("MCU status response timeout");
    if (this->communication_problem_sensor_ != nullptr) {
      this->communication_problem_sensor_->publish_state(true);
    }
  }

  if (this->command_queue_.empty()) {
    return;
  }
  if (this->has_sent_command_ && now - this->last_command_at_ < this->command_interval_) {
    return;
  }

  const auto &command = this->command_queue_.front();
  ESP_LOGVV(TAG, "Sending frame: %s", format_hex_pretty(command.frame).c_str());
  this->write_array(command.frame);
  if (command.status_request) {
    this->communication_state_.status_request_transmitted(now);
  }
  this->command_queue_.pop_front();
  this->last_command_at_ = now;
  this->has_sent_command_ = true;

  if (this->command_queue_.empty() && this->refresh_after_commands_) {
    this->refresh_after_commands_ = false;
    this->request_status_(true);
  }
}

void LevoitClassic300S::update() { this->request_status_(); }

void LevoitClassic300S::dump_config() {
  ESP_LOGCONFIG(TAG, "Levoit Classic 300S:");
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Command interval: %lu ms", static_cast<unsigned long>(this->command_interval_));
  ESP_LOGCONFIG(TAG, "  Status response timeout: %lu ms",
                static_cast<unsigned long>(this->communication_state_.response_timeout_ms()));
  ESP_LOGCONFIG(TAG, "  Humidifier entity: %s", YESNO(this->humidifier_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Mode entity: %s", YESNO(this->mode_select_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Target humidity entity: %s", YESNO(this->target_humidity_number_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Night light entity: %s", YESNO(this->night_light_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Current humidity sensor: %s", YESNO(this->current_humidity_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Temperature sensor: %s", YESNO(this->temperature_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Tank lifted sensor: %s", YESNO(this->tank_lifted_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Communication problem sensor: %s", YESNO(this->communication_problem_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Raw status sensor: %s", YESNO(this->raw_status_sensor_ != nullptr));
}

void LevoitClassic300S::set_power(bool on) { this->enqueue_command_(protocol::power_payload(on)); }

void LevoitClassic300S::set_manual_mist_level(uint8_t level) {
  if (level != 0) {
    this->last_manual_mist_level_ = protocol::normalize_mist_level(level);
  }
  this->enqueue_command_(protocol::manual_mist_payload(this->last_manual_mist_level_));
}

void LevoitClassic300S::set_auto_mode(uint8_t target_humidity) {
  if (target_humidity != 0) {
    this->last_target_humidity_ = protocol::normalize_target_humidity(target_humidity);
  }
  this->enqueue_command_(protocol::auto_mode_payload(this->last_target_humidity_));
}

void LevoitClassic300S::set_night_light_brightness(uint8_t percent) {
  this->enqueue_command_(protocol::night_light_payload(percent));
}

void LevoitClassic300S::enqueue_command_(const std::vector<uint8_t> &payload, bool refresh_after,
                                         bool status_request) {
  this->command_queue_.push_back(
      {protocol::build_frame(protocol::COMMAND_FRAME_TYPE, this->next_sequence_++, payload), status_request});
  this->refresh_after_commands_ = this->refresh_after_commands_ || refresh_after;
}

void LevoitClassic300S::request_status_(bool require_fresh_response) {
  if (!this->communication_state_.request_status(require_fresh_response)) {
    return;
  }
  this->enqueue_command_(protocol::status_request_payload(), false, true);
}

void LevoitClassic300S::process_frame_(const protocol::Frame &frame) {
  ESP_LOGVV(TAG, "Received type 0x%02X sequence %u payload: %s", frame.type, frame.sequence,
            format_hex_pretty(frame.payload).c_str());
  protocol::Status status;
  if (protocol::decode_status(frame, status)) {
    const bool request_follow_up = this->communication_state_.status_received();
    this->status_clear_warning();
    if (this->communication_problem_sensor_ != nullptr) {
      this->communication_problem_sensor_->publish_state(false);
    }
    this->publish_status_(status);
    if (request_follow_up) {
      this->request_status_();
    }
  }
}

void LevoitClassic300S::publish_status_(const protocol::Status &status) {
  const bool target_humidity_valid = protocol::is_valid_target_humidity(status.target_humidity);
  if (target_humidity_valid) {
    this->last_target_humidity_ = status.target_humidity;
  } else {
    ESP_LOGW(TAG, "Ignoring out-of-range target humidity byte: %u (expected %u-%u)", status.target_humidity,
             protocol::MIN_TARGET_HUMIDITY, protocol::MAX_TARGET_HUMIDITY);
  }
  if (status.manual_mist_level >= 1 && status.manual_mist_level <= 9) {
    this->last_manual_mist_level_ = status.manual_mist_level;
  }

  if (this->humidifier_ != nullptr) {
    this->humidifier_->state = status.power;
    this->humidifier_->speed = std::max<int>(1, std::min<int>(9, status.manual_mist_level));
    this->humidifier_->publish_state();
  }
  if (this->mode_select_ != nullptr) {
    const char *mode = "Unknown";
    if (status.mode == protocol::OperatingMode::AUTO) {
      mode = "Auto";
    } else if (status.mode == protocol::OperatingMode::MANUAL) {
      mode = "Manual";
    } else {
      ESP_LOGW(TAG, "Unmapped operating mode byte: 0x%02X", status.raw[13]);
    }
    this->mode_select_->publish_state(mode);
  }
  if (this->target_humidity_number_ != nullptr && target_humidity_valid) {
    this->target_humidity_number_->publish_state(status.target_humidity);
  }
  if (this->night_light_ != nullptr) {
    this->night_light_->sync_brightness(status.night_light_percent);
  }
  if (this->current_humidity_sensor_ != nullptr) {
    this->current_humidity_sensor_->publish_state(status.current_humidity);
  }
  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(status.temperature_celsius);
  }
  if (this->tank_lifted_sensor_ != nullptr) {
    this->tank_lifted_sensor_->publish_state(status.tank_lifted);
  }
  if (this->raw_status_sensor_ != nullptr) {
    this->raw_status_sensor_->publish_state(hex_(status.raw));
  }
}

std::string LevoitClassic300S::hex_(const std::array<uint8_t, 17> &bytes) {
  std::string output;
  output.reserve(bytes.size() * 3 - 1);
  char hex[4];
  for (size_t index = 0; index < bytes.size(); index++) {
    std::snprintf(hex, sizeof(hex), index == 0 ? "%02X" : " %02X", bytes[index]);
    output += hex;
  }
  return output;
}

}  // namespace esphome::levoit_classic_300s
