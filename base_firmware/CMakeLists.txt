# ZeroCode AI - Matter Firmware
cmake_minimum_required(VERSION 3.16)

# Safety net: esp_matter has various C++ standard / GCC compatibility issues.
# IDF 6 raised the dialect to gnu++26 and the toolchain to GCC 16, so this is
# doing more work than it was, not less.
add_compile_options(-fpermissive)

# IDF builds with `-Wall -Werror -Wextra -Wno-error=extra`. That last flag does
# NOT cover -Wmissing-field-initializers: the diagnostic has its own name, so
# the blanket -Werror still promotes it, and under GCC 16 it fires on partially
# designated-initialized aggregates that GCC 14 let through.
#
# Demoted to a warning rather than relied on alone, because this is not a
# defect class: C and C++ both zero-initialize the members you leave out, and
# `esp_timer_create_args_t{.callback = cb, .name = "x"}` is the idiomatic way to
# fill one in. More to the point, the agents WRITE C at runtime — a build that
# hard-fails on idiomatic partial initialization would fail on code that is
# correct, which is the worst possible signal to hand a code-writing agent.
#
# This does NOT replace fixing our own sources: the blocks were separately
# rewritten to zero-init-then-assign (`gpio_config_t c = {}; c.mode = ...`),
# which is future-proof by construction — IDF 6.2 added hys_ctrl_mode to
# gpio_config_t and will add more. The two do different jobs: the rewrite keeps
# OUR code exemplary (blocks are what agents read as examples), the flag keeps
# an agent's own idiomatic code from hard-failing a build.
add_compile_options(-Wno-error=missing-field-initializers)

# GCC 16 added -Wnon-c-typedef-for-linkage, and esp-dl (pulled in by esp-sr for
# the voice framework) trips it in dl_image_bmp.cpp. It is a MANAGED component,
# so there is nothing of ours to fix and no patch we could carry that the next
# esp-sr bump would not undo. Demoting the diagnostic is the only lever on our
# side; drop this line once esp-dl builds clean under GCC 16.
add_compile_options(-Wno-error=non-c-typedef-for-linkage)

# Third-party source fetched by git_clone (and any repo the platform pre-installs)
# lives in ../third_party/<name>, a SIBLING of this firmware tree. It is a
# sibling on purpose: apply_template deletes and recreates the firmware dir, so
# anything vendored inside it would be destroyed by a re-scaffold.
#
# Each subdirectory is offered to IDF as a component search path. IDF ignores a
# directory with no CMakeLists.txt, so a repo that isn't a component is simply
# not built — while still being readable/searchable by the agent, and that is a
# legitimate use (read the real header instead of recalling the API).
#
# Being in the component path is also what makes check_code work on it: clangd
# resolves through compile_commands.json, so a repo that isn't compiled has no
# include paths and every header in it reports "not in the build index".
set(_zc_third_party "${CMAKE_CURRENT_LIST_DIR}/../third_party")
if(EXISTS "${_zc_third_party}")
  file(GLOB _zc_tp_dirs "${_zc_third_party}/*")
  foreach(_dir ${_zc_tp_dirs})
    if(IS_DIRECTORY "${_dir}")
      list(APPEND EXTRA_COMPONENT_DIRS "${_dir}")
      message(STATUS "third_party component path: ${_dir}")
    endif()
  endforeach()
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(zerocode_ai_firmware)

# Patch esp_matter ClosureControl: add missing self-comparison operator==.
# C++23 std::optional<T>::operator== requires T==T, but the structs only define
# operator==(const Base&). This adds operator==(const Self&) delegating to the base.
# Uses Python to avoid CMake semicolon escaping issues in C++ code.
set(_cc_header "${CMAKE_CURRENT_SOURCE_DIR}/managed_components/espressif__esp_matter/connectedhomeip/connectedhomeip/src/app/clusters/closure-control-server/closure-control-cluster-objects.h")
if(EXISTS "${_cc_header}")
  execute_process(
    COMMAND python3 -c "
import sys
p = sys.argv[1]
with open(p) as f: s = f.read()
if 'const GenericOverallCurrentState & rhs' in s:
    sys.exit(0)
s = s.replace(
    '    bool operator==(const Structs::OverallCurrentStateStruct::Type & rhs) const',
    '    bool operator==(const GenericOverallCurrentState & rhs) const { return operator==(static_cast<const Structs::OverallCurrentStateStruct::Type &>(rhs)); }\\n    bool operator==(const Structs::OverallCurrentStateStruct::Type & rhs) const')
s = s.replace(
    '    bool operator==(const Structs::OverallTargetStateStruct::Type & rhs) const',
    '    bool operator==(const GenericOverallTargetState & rhs) const { return operator==(static_cast<const Structs::OverallTargetStateStruct::Type &>(rhs)); }\\n    bool operator==(const Structs::OverallTargetStateStruct::Type & rhs) const')
with open(p, 'w') as f: f.write(s)
print('Patched')
" "${_cc_header}"
    OUTPUT_VARIABLE _patch_out
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(_patch_out)
    message(STATUS "Patched ClosureControl operator== for C++23 compatibility")
  endif()
endif()

# Patch esp_matter BLEManagerImpl: drop its ESP32-P4 ble_transport_ll_deinit stub
# when ESP-Hosted supplies the real one. P4 has no BT controller, so NimBLE's
# transport comes from esp_hosted's VHCI glue; esp_matter 1.4.2 was written
# against a hosted release that implemented three of the four ble_transport_*
# entry points and stubbed the fourth itself. Current hosted implements all
# four, so the stub is a duplicate symbol and the link fails. Narrowing the
# guard keeps the stub for anyone building without that glue. The stub's
# `#ifdef` is the only one in the file, so one line is the whole edit.
set(_ble_mgr "${CMAKE_CURRENT_SOURCE_DIR}/managed_components/espressif__esp_matter/connectedhomeip/connectedhomeip/src/platform/ESP32/nimble/BLEManagerImpl.cpp")
if(EXISTS "${_ble_mgr}")
  execute_process(
    COMMAND python3 -c "
import sys
p = sys.argv[1]
old = '#ifdef CONFIG_IDF_TARGET_ESP32P4'
new = '#if defined(CONFIG_IDF_TARGET_ESP32P4) && !defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)'
with open(p) as f: s = f.read()
if new in s or old not in s:
    sys.exit(0)
with open(p, 'w') as f: f.write(s.replace(old, new))
print('Patched')
" "${_ble_mgr}"
    OUTPUT_VARIABLE _ble_patch_out
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(_ble_patch_out)
    message(STATUS "Patched BLEManagerImpl: hosted NimBLE glue owns ble_transport_ll_deinit")
  endif()
endif()
