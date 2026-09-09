# Self-contained framework component: the engine copies this subtree verbatim
# and, unlike app_matter / app_audio / app_vision, never regenerates either
# app_tflite.cpp or this file — nothing here has a per-device-type slot family.
#
# esp-tflite-micro is NOT listed below on purpose; idf_component.yml beside this
# file declares it and the component manager turns that into a requirement.
# esp_timer is different — it is a core IDF component, absent from IDF's
# common requires (tools/cmake/build.cmake), so nothing but this line supplies
# it and app_tflite.cpp's esp_timer_get_time() fails to find its header.
idf_component_register(
    SRCS "app_tflite.cpp" "model_reference.cc"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_timer
)

# ── Catalogued models, compiled out of esp-tflite-micro ──────────────────
# Each is guarded by its own Kconfig (see Kconfig beside this file) so the
# catalogue can grow without every product paying the compile time. A block
# that consumes a model turns its switch on from its own sdkconfig.defaults —
# the same shape every esp-dl model component uses.
#
# Sources come from the component rather than being vendored here:
# person_detect_model_data.cc alone is 1.85 MB of hex, seven times this whole
# catalogue, and the licence (Apache-2.0, (c) The TensorFlow Authors) stays
# with the component that owns it.
idf_component_get_property(zc_tflm_dir espressif__esp-tflite-micro COMPONENT_DIR)

# model-kconfig-suffix : path within esp-tflite-micro
set(zc_ml_models
    "PERSON_DETECT:examples/person_detection/main/person_detect_model_data.cc"
    "MICRO_SPEECH:examples/micro_speech/main/model.cc"
)

foreach(zc_entry IN LISTS zc_ml_models)
    string(REPLACE ":" ";" zc_parts "${zc_entry}")
    list(GET zc_parts 0 zc_name)
    list(GET zc_parts 1 zc_rel)
    if(CONFIG_ZC_ML_MODEL_${zc_name})
        set(zc_src "${zc_tflm_dir}/${zc_rel}")
        if(EXISTS "${zc_src}")
            target_sources(${COMPONENT_LIB} PRIVATE "${zc_src}")
        else()
            # Say it here rather than let it surface as an undefined reference
            # to the model symbol three steps later.
            message(WARNING "ZC_ML_MODEL_${zc_name} is on but esp-tflite-micro "
                            "no longer ships ${zc_src} — the block consuming it "
                            "will not link until models/<id>.yml is updated.")
        endif()
    endif()
endforeach()
