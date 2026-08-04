#include "communication_state.h"

namespace esphome::levoit_classic_300s {

bool CommunicationState::request_status(bool require_fresh_response) {
  if (this->status_request_pending()) {
    this->follow_up_requested_ = this->follow_up_requested_ || require_fresh_response;
    return false;
  }
  this->follow_up_requested_ = false;
  this->request_queued_ = true;
  return true;
}

void CommunicationState::status_request_transmitted(uint32_t now) {
  this->request_queued_ = false;
  this->awaiting_response_ = true;
  this->request_transmitted_at_ = now;
}

bool CommunicationState::status_received() {
  const bool request_follow_up = this->awaiting_response_ && this->follow_up_requested_;
  this->awaiting_response_ = false;
  this->follow_up_requested_ = false;
  this->communication_problem_ = false;
  return request_follow_up;
}

bool CommunicationState::check_timeout(uint32_t now) {
  if (!this->awaiting_response_ || now - this->request_transmitted_at_ < this->response_timeout_ms_) {
    return false;
  }
  this->awaiting_response_ = false;
  this->communication_problem_ = true;
  return true;
}

}  // namespace esphome::levoit_classic_300s
