# Keep this in step with any app_aws_iot CMakeLists the engine might emit. It
# does not emit one today — aws_iot is a pure copy-in framework, like ml and lm —
# so this file is what compiles the component.
#
# REQUIRES vs PRIV_REQUIRES matters here. include/app_aws_iot.h is a PUBLIC
# header and it includes esp_event.h and core_mqtt_agent.h, so those components
# must be public: app_logic includes this header from a behaviour slot (see the
# block's requires.cmake) and would not otherwise see their include paths.
idf_component_register(
    SRCS "app_aws_iot.cpp" "zc_aws_agent.cpp" "zc_aws_subs.cpp" "zc_aws_console.cpp"
    INCLUDE_DIRS "include"
    REQUIRES coreMQTT coreMQTT-Agent esp_event
    PRIV_REQUIRES backoffAlgorithm posix_compat esp-tls mbedtls
                  esp_wifi esp_netif esp_hw_support nvs_flash app_console console
)
