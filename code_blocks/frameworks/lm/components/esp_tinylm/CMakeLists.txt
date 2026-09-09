# esp_tinylm — tiny LM inference for ESP32 (PLE + per-target accelerated matvec).

set(srcs "src/esp_tinylm.c" "src/esp_tinylm_runtime.c")

if(CONFIG_IDF_TARGET_ESP32S31 OR CONFIG_IDF_TARGET_ESP32P4)
    # RISC-V xespv (PIE) vector kernel -- S31 and P4 share the PIE ISA
    list(APPEND srcs "src/matvec_riscv_pie.c")
elseif(CONFIG_IDF_TARGET_ESP32S3)
    # Espressif SIMD/Vector Extension (EE.* 128-bit) vector kernel
    list(APPEND srcs "src/matvec_s3.c")
else()
    # portable scalar fallback (other targets)
    list(APPEND srcs "src/matvec_scalar.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "priv_include"
                       PRIV_REQUIRES esp_psram)
