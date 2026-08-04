#!/usr/bin/env bash
set -euo pipefail

build_dir="${TMPDIR:-/tmp}/levoit-classic-300s-sanitizers"
mkdir -p "$build_dir"

common_flags=(
  -std=c++17
  -Wall
  -Wextra
  -Werror
  -fsanitize=address,undefined
  -fno-omit-frame-pointer
  -Icomponents/levoit_classic_300s
)

g++ \
  "${common_flags[@]}" \
  components/levoit_classic_300s/levoit_protocol.cpp \
  tests/test_levoit_protocol.cpp \
  -o "$build_dir/test_levoit_protocol"

g++ \
  "${common_flags[@]}" \
  components/levoit_classic_300s/communication_state.cpp \
  tests/test_communication_state.cpp \
  -o "$build_dir/test_communication_state"

ASAN_OPTIONS=detect_leaks=0 "$build_dir/test_levoit_protocol"
ASAN_OPTIONS=detect_leaks=0 "$build_dir/test_communication_state"
