idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES app_driver app_logic app_config app_console app_utils
)
# Protocol components (app_matter / app_rainmaker / app_zigbee / app_ble_mesh)
# are added per-product via cmake_priv_requires.main from each protocol block.
