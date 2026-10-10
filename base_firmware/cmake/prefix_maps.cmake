# Fixed prefix maps that hide build-machine paths, shared by the app and the
# bootloader. The set must not depend on the component list.
function(zc_add_prefix_maps project_dir)
  get_filename_component(_tp "${project_dir}/../third_party" ABSOLUTE)
  idf_build_set_property(COMPILE_OPTIONS "-ffile-prefix-map=${project_dir}=/IDF_PROJECT" APPEND)
  idf_build_set_property(COMPILE_OPTIONS "-ffile-prefix-map=${_tp}=/THIRD_PARTY" APPEND)
  idf_build_set_property(COMPILE_OPTIONS "-ffile-prefix-map=$ENV{IDF_PATH}=/IDF" APPEND)
  idf_build_set_property(COMPILE_OPTIONS "-ffile-prefix-map=$ENV{IDF_TOOLS_PATH}=/TOOLCHAIN" APPEND)
endfunction()
