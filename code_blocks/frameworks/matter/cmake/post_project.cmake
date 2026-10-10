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
