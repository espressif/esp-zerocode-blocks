# coreMQTT — fetched from espressif/esp-aws-iot, not vendored.
#
# WHY THIS FILE EXISTS AT ALL. Upstream's own libraries/coreMQTT/CMakeLists.txt
# builds its include list with
#
#     ${CMAKE_CURRENT_LIST_DIR}/../common/logging/
#
# which reaches OUTSIDE its own component directory. In an upstream checkout
# libraries/common/logging is a sibling and that resolves. Fetched by the IDF
# component manager it becomes managed_components/common/logging, and then:
#
#   - declaring `common` as a dependency makes the manager register
#     managed_components/common as a component, but upstream's libraries/common
#     has no CMakeLists.txt, so IDF answers "Directory ... does not contain a
#     component" and the build dies at CONFIGURE time;
#   - `require: no` does not avoid it — the manager writes the entry into
#     managed_components_list.temp.cmake regardless;
#   - omitting `common` fails earlier still, with "Include directory
#     .../managed_components/common/logging is not a directory".
#
# All three were tested against ESP-IDF 6.x, not reasoned about. Registering the
# sources ourselves sidesteps the lot: we fetch a real checkout, where
# libraries/common/logging IS a sibling, and point at it directly.
#
# The fix belongs upstream. If coreMQTT's CMakeLists stopped reaching for
# ../common/logging, this file could be deleted and coreMQTT git-pinned beside
# coreMQTT-Agent in ../../idf_component.yml.
set(ZC_AWS_IOT_PIN "9c879feedb699611e7c220b93aed84c84322ff84")
set(ZC_AWS_IOT_URL "https://github.com/espressif/esp-aws-iot.git")

# WHERE THE SDK LIVES. Honour, in order: an existing ZC_AWS_IOT_SRC variable,
# the ENVIRONMENT variable of the same name, then the default sibling location.
#
# The environment is the supported override, NOT -DZC_AWS_IOT_SRC=... . IDF
# processes component CMakeLists in two passes and the early one does not see
# -D, so a -D override still fetches on the first pass and only takes effect on
# the second: the build succeeds while silently re-cloning every time. An
# environment variable reaches both passes.
if(NOT DEFINED ZC_AWS_IOT_SRC OR ZC_AWS_IOT_SRC STREQUAL "")
    if(DEFINED ENV{ZC_AWS_IOT_SRC} AND NOT "$ENV{ZC_AWS_IOT_SRC}" STREQUAL "")
        set(ZC_AWS_IOT_SRC "$ENV{ZC_AWS_IOT_SRC}")
    else()
        # A SIBLING of the firmware tree on purpose: apply_template deletes and
        # recreates the firmware dir, so a checkout inside it would be destroyed
        # on every re-scaffold. base_firmware/CMakeLists.txt already treats
        # ../third_party as the home for fetched source.
        set(ZC_AWS_IOT_SRC "${CMAKE_CURRENT_LIST_DIR}/../../../third_party/esp-aws-iot")
    endif()
endif()
get_filename_component(ZC_AWS_IOT_SRC "${ZC_AWS_IOT_SRC}" ABSOLUTE)

# coreMQTT's sources live in a git SUBMODULE of esp-aws-iot, so this file — not
# the repository root — is what proves the checkout is usable.
set(_zc_mqtt_paths "${ZC_AWS_IOT_SRC}/libraries/coreMQTT/coreMQTT/mqttFilePaths.cmake")

if(NOT EXISTS "${_zc_mqtt_paths}")
    message(STATUS "coreMQTT: fetching esp-aws-iot@${ZC_AWS_IOT_PIN} into ${ZC_AWS_IOT_SRC}")
    file(MAKE_DIRECTORY "${ZC_AWS_IOT_SRC}")
    # init + fetch + checkout rather than `git clone --branch`, because the pin
    # is a commit and a shallow clone cannot name one.
    execute_process(COMMAND git init -q "${ZC_AWS_IOT_SRC}" RESULT_VARIABLE _zc_rc)
    if(_zc_rc)
        message(FATAL_ERROR "coreMQTT: git init failed in ${ZC_AWS_IOT_SRC}")
    endif()
    execute_process(COMMAND git -C "${ZC_AWS_IOT_SRC}" remote add origin "${ZC_AWS_IOT_URL}"
                    OUTPUT_QUIET ERROR_QUIET)
    execute_process(COMMAND git -C "${ZC_AWS_IOT_SRC}" fetch -q --depth 1 origin "${ZC_AWS_IOT_PIN}"
                    RESULT_VARIABLE _zc_rc)
    if(_zc_rc)
        message(FATAL_ERROR
            "coreMQTT: could not fetch esp-aws-iot@${ZC_AWS_IOT_PIN}.\n"
            "  Offline or behind a proxy? Clone it once and export ZC_AWS_IOT_SRC=<path>.\n"
            "  (The environment variable, not -DZC_AWS_IOT_SRC — see the note above.)")
    endif()
    execute_process(COMMAND git -C "${ZC_AWS_IOT_SRC}" checkout -q FETCH_HEAD RESULT_VARIABLE _zc_rc)
    if(_zc_rc)
        message(FATAL_ERROR "coreMQTT: checkout of ${ZC_AWS_IOT_PIN} failed")
    endif()
    execute_process(COMMAND git -C "${ZC_AWS_IOT_SRC}" submodule update -q --init --depth 1
                            --recursive libraries/coreMQTT/coreMQTT
                    RESULT_VARIABLE _zc_rc)
    if(_zc_rc OR NOT EXISTS "${_zc_mqtt_paths}")
        message(FATAL_ERROR "coreMQTT: the coreMQTT submodule did not materialise")
    endif()
endif()

# Gives MQTT_INCLUDE_PUBLIC_DIRS, MQTT_SOURCES and MQTT_SERIALIZER_SOURCES.
include("${_zc_mqtt_paths}")

idf_component_register(
    SRCS ${MQTT_SOURCES}
         ${MQTT_SERIALIZER_SOURCES}
         "${ZC_AWS_IOT_SRC}/libraries/coreMQTT/port/network_transport/network_transport.c"
    INCLUDE_DIRS ${MQTT_INCLUDE_PUBLIC_DIRS}
                 "${ZC_AWS_IOT_SRC}/libraries/coreMQTT/config"
                 "${ZC_AWS_IOT_SRC}/libraries/common/logging"
                 "${ZC_AWS_IOT_SRC}/libraries/coreMQTT/port/network_transport"
    REQUIRES esp-tls
)

# Upstream sets the same flag: core_mqtt.c trips -Wformat on a 32-bit size_t.
set_source_files_properties(
    "${ZC_AWS_IOT_SRC}/libraries/coreMQTT/coreMQTT/source/core_mqtt.c"
    PROPERTIES COMPILE_FLAGS -Wno-format)
