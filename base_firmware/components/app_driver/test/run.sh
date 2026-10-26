#!/usr/bin/env bash
# Host-compile + run the app_driver regression tests, one binary per test_*.cpp
# (each needs the bus's static state fresh).
# No IDF/Unity: shims/ stand in for esp_err/esp_log/esp_timer/freertos (real
# locks, std::thread tasks), app_driver_types.h
# here stands in for the generated header. Compiles the REAL app_driver_cb.cpp.
set -euo pipefail
cd "$(dirname "$0")"
CXX="${CXX:-c++}"
dir="$(mktemp -d)"
for t in test_*.cpp; do
  out="$dir/${t%.cpp}"
  $CXX -std=c++17 -Wall -Wextra -O1 -pthread \
    -I shims -I . -I ../include \
    "$t" ../app_driver_cb.cpp \
    -o "$out"
  "$out"
done
