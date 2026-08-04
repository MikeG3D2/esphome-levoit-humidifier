#include "communication_state.h"

#include <cassert>
#include <cstdint>
#include <iostream>

using esphome::levoit_classic_300s::CommunicationState;

static void test_missing_status_response_times_out_from_transmission() {
  CommunicationState state(5000);
  assert(state.request_status());
  assert(!state.check_timeout(100000));

  state.status_request_transmitted(100000);
  assert(!state.check_timeout(104999));
  assert(state.check_timeout(105000));
  assert(state.communication_problem());
  assert(!state.status_request_pending());
}

static void test_valid_status_clears_problem_and_prevents_false_timeout() {
  CommunicationState state(5000);
  assert(state.request_status());
  state.status_request_transmitted(100);
  assert(!state.status_received());
  assert(!state.check_timeout(5100));
  assert(!state.communication_problem());

  assert(state.request_status());
  state.status_request_transmitted(10000);
  assert(state.check_timeout(15000));
  assert(state.communication_problem());
  assert(!state.status_received());
  assert(!state.communication_problem());
}

static void test_duplicate_status_requests_are_coalesced_until_response() {
  CommunicationState state(5000);
  assert(state.request_status());
  assert(!state.request_status());
  state.status_request_transmitted(200);
  assert(!state.request_status());
  assert(state.status_request_pending());

  assert(!state.status_received());
  assert(!state.status_request_pending());
  assert(state.request_status());
}

static void test_command_refresh_waits_for_in_flight_status_then_requests_fresh_status() {
  CommunicationState state(5000);
  assert(state.request_status());
  state.status_request_transmitted(300);
  assert(!state.request_status(true));

  assert(state.status_received());
  assert(state.request_status());
  assert(state.status_request_pending());
}

int main() {
  test_missing_status_response_times_out_from_transmission();
  test_valid_status_clears_problem_and_prevents_false_timeout();
  test_duplicate_status_requests_are_coalesced_until_response();
  test_command_refresh_waits_for_in_flight_status_then_requests_fresh_status();
  std::cout << "All communication state tests passed\n";
  return 0;
}
