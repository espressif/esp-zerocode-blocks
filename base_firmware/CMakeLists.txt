# ZeroCode AI firmware
cmake_minimum_required(VERSION 3.16)

# Framework CMake (cmake/frameworks/<name>/), copied in for the selected
# frameworks only. Sorted so every configure includes them in the same order.
file(GLOB _zc_fw_pre LIST_DIRECTORIES false "${CMAKE_CURRENT_LIST_DIR}/cmake/frameworks/*/pre_project.cmake")
list(SORT _zc_fw_pre)
foreach(_zc_hook ${_zc_fw_pre})
  include("${_zc_hook}")
endforeach()

# Safety net for every C++ file in the tree (frameworks, managed components and
# agent-written code): IDF 6 builds gnu++26 with GCC 16, which rejects code
# older compilers accepted.
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

# The app version (esp_app_desc_t.version: what the `version` console command
# prints) is fixed. Unset, IDF takes `git describe` of the repo this tree lives
# in — the PRODUCT's repo, whose HEAD moves on every report and spec save — so
# an unchanged firmware would get a new version, a recompiled app descriptor and
# a new ELF SHA-256 on every commit. The board prints that SHA at boot, and the
# host's crash decoder refuses to decode a board whose SHA is not the build's.
set(PROJECT_VER "1.0.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(zerocode_ai_firmware)

file(GLOB _zc_fw_post LIST_DIRECTORIES false "${CMAKE_CURRENT_LIST_DIR}/cmake/frameworks/*/post_project.cmake")
list(SORT _zc_fw_post)
foreach(_zc_hook ${_zc_fw_post})
  include("${_zc_hook}")
endforeach()
