#!/usr/bin/env bash
set -euo pipefail

build_dir="${TMPDIR:-/tmp}/levoit-classic-300s-tests"
mkdir -p "$build_dir"

g++ \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -Icomponents/levoit_classic_300s \
  components/levoit_classic_300s/levoit_protocol.cpp \
  tests/test_levoit_protocol.cpp \
  -o "$build_dir/test_levoit_protocol"

"$build_dir/test_levoit_protocol"

g++ \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -Icomponents/levoit_classic_300s \
  components/levoit_classic_300s/communication_state.cpp \
  tests/test_communication_state.cpp \
  -o "$build_dir/test_communication_state"

"$build_dir/test_communication_state"
