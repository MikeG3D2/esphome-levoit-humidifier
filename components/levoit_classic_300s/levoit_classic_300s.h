#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/fan/fan.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

#include "levoit_protocol.h"
#include "communication_state.h"

#include <cstdint>
#include <deque>
#include <vector>

namespace esphome::levoit_classic_300s {

class LevoitClassic300S;

class LevoitHumidifierFan final : public fan::Fan {
 public:
  explicit LevoitHumidifierFan(LevoitClassic300S *parent) : parent_(parent) {}
  fan::FanTraits get_traits() override;

 protected:
  void control(const fan::FanCall &call) override;
  LevoitClassic300S *parent_;
};

class LevoitModeSelect final : public select::Select {
 public:
  explicit LevoitModeSelect(LevoitClassic300S *parent) : parent_(parent) {}

 protected:
  void control(size_t index) override;
  LevoitClassic300S *parent_;
};

class LevoitTargetHumidityNumber final : public number::Number {
 public:
  explicit LevoitTargetHumidityNumber(LevoitClassic300S *parent) : parent_(parent) {}

 protected:
  void control(float value) override;
  LevoitClassic300S *parent_;
};

class LevoitManualMistLevelNumber final : public number::Number {
 public:
  explicit LevoitManualMistLevelNumber(LevoitClassic300S *parent) : parent_(parent) {}

 protected:
  void control(float value) override;
  LevoitClassic300S *parent_;
};

class LevoitNightLight final : public light::LightOutput {
 public:
  explicit LevoitNightLight(LevoitClassic300S *parent) : parent_(parent) {}
  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override { this->state_ = state; }
  void write_state(light::LightState *state) override;
  void sync_brightness(uint8_t percent);

 protected:
  LevoitClassic300S *parent_;
  light::LightState *state_{nullptr};
  int16_t last_commanded_percent_{-1};
};

class LevoitDisplaySwitch final : public switch_::Switch {
 public:
  explicit LevoitDisplaySwitch(LevoitClassic300S *parent) : parent_(parent) {}
  bool assumed_state() override { return true; }

 protected:
  void write_state(bool state) override;
  LevoitClassic300S *parent_;
};

class LevoitAutoStopSwitch final : public switch_::Switch {
 public:
  explicit LevoitAutoStopSwitch(LevoitClassic300S *parent) : parent_(parent) {}
  bool assumed_state() override { return true; }

 protected:
  void write_state(bool state) override;
  LevoitClassic300S *parent_;
};

class LevoitClassic300S final : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_command_interval(uint32_t command_interval) { this->command_interval_ = command_interval; }
  void set_status_response_timeout(uint32_t timeout) { this->communication_state_.set_response_timeout(timeout); }
  void set_humidifier(LevoitHumidifierFan *humidifier) { this->humidifier_ = humidifier; }
  void set_mode_select(LevoitModeSelect *mode_select) { this->mode_select_ = mode_select; }
  void set_target_humidity_number(LevoitTargetHumidityNumber *number) { this->target_humidity_number_ = number; }
  void set_manual_mist_level_number(LevoitManualMistLevelNumber *number) {
    this->manual_mist_level_number_ = number;
  }
  void set_night_light(LevoitNightLight *night_light) { this->night_light_ = night_light; }
  void set_display_switch(LevoitDisplaySwitch *display_switch) { this->display_switch_ = display_switch; }
  void set_auto_stop_switch(LevoitAutoStopSwitch *auto_stop_switch) { this->auto_stop_switch_ = auto_stop_switch; }
  void set_current_humidity_sensor(sensor::Sensor *sensor) { this->current_humidity_sensor_ = sensor; }
  void set_temperature_sensor(sensor::Sensor *sensor) { this->temperature_sensor_ = sensor; }
  void set_tank_lifted_binary_sensor(binary_sensor::BinarySensor *sensor) { this->tank_lifted_sensor_ = sensor; }
  void set_no_water_binary_sensor(binary_sensor::BinarySensor *sensor) { this->no_water_sensor_ = sensor; }
  void set_communication_problem_binary_sensor(binary_sensor::BinarySensor *sensor) {
    this->communication_problem_sensor_ = sensor;
  }
  void set_raw_status_text_sensor(text_sensor::TextSensor *sensor) { this->raw_status_sensor_ = sensor; }

  void set_power(bool on);
  void set_manual_mist_level(uint8_t level);
  void set_auto_mode(uint8_t target_humidity);
  void set_sleep_mode(uint8_t target_humidity);
  void set_target_humidity(uint8_t target_humidity);
  void set_night_light_brightness(uint8_t percent);
  void set_display(bool on);
  void set_auto_stop(bool enabled);

 protected:
  struct QueuedCommand {
    std::vector<uint8_t> frame;
    bool status_request{false};
  };

  void enqueue_command_(const std::vector<uint8_t> &payload, bool refresh_after = true,
                        bool status_request = false);
  void request_status_(bool require_fresh_response = false);
  void process_frame_(const protocol::Frame &frame);
  void publish_status_(const protocol::Status &status);
  static std::string hex_(const std::array<uint8_t, 17> &bytes);

  protocol::FrameParser parser_;
  CommunicationState communication_state_{5000};
  std::deque<QueuedCommand> command_queue_;
  uint8_t next_sequence_{0};
  uint32_t command_interval_{100};
  uint32_t last_command_at_{0};
  bool has_sent_command_{false};
  bool refresh_after_commands_{false};
  uint8_t last_target_humidity_{50};
  uint8_t last_manual_mist_level_{1};

  LevoitHumidifierFan *humidifier_{nullptr};
  LevoitModeSelect *mode_select_{nullptr};
  LevoitTargetHumidityNumber *target_humidity_number_{nullptr};
  LevoitManualMistLevelNumber *manual_mist_level_number_{nullptr};
  LevoitNightLight *night_light_{nullptr};
  LevoitDisplaySwitch *display_switch_{nullptr};
  LevoitAutoStopSwitch *auto_stop_switch_{nullptr};
  sensor::Sensor *current_humidity_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  binary_sensor::BinarySensor *tank_lifted_sensor_{nullptr};
  binary_sensor::BinarySensor *no_water_sensor_{nullptr};
  binary_sensor::BinarySensor *communication_problem_sensor_{nullptr};
  text_sensor::TextSensor *raw_status_sensor_{nullptr};
};

}  // namespace esphome::levoit_classic_300s
