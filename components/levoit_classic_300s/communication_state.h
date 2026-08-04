#pragma once

#include <cstdint>

namespace esphome::levoit_classic_300s {

class CommunicationState {
 public:
  explicit CommunicationState(uint32_t response_timeout_ms) : response_timeout_ms_(response_timeout_ms) {}

  void set_response_timeout(uint32_t response_timeout_ms) { this->response_timeout_ms_ = response_timeout_ms; }
  bool request_status(bool require_fresh_response = false);
  void status_request_transmitted(uint32_t now);
  bool status_received();
  bool check_timeout(uint32_t now);

  bool status_request_pending() const { return this->request_queued_ || this->awaiting_response_; }
  bool communication_problem() const { return this->communication_problem_; }
  uint32_t response_timeout_ms() const { return this->response_timeout_ms_; }

 protected:
  uint32_t response_timeout_ms_;
  uint32_t request_transmitted_at_{0};
  bool request_queued_{false};
  bool awaiting_response_{false};
  bool follow_up_requested_{false};
  bool communication_problem_{false};
};

}  // namespace esphome::levoit_classic_300s
