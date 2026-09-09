#!/usr/bin/env bash
# Host-compile + run the app_driver change-detection regression test.
# No IDF/Unity: shims/ stand in for esp_err/esp_log/freertos, app_driver_types.h
# here stands in for the generated header. Compiles the REAL app_driver_cb.cpp.
set -euo pipefail
cd "$(dirname "$0")"
CXX="${CXX:-c++}"
out="$(mktemp -d)/test_set_param_dedup"
$CXX -std=c++17 -Wall -Wextra -O1 \
  -I shims -I . -I ../include \
  test_set_param_dedup.cpp ../app_driver_cb.cpp \
  -o "$out"
"$out"
