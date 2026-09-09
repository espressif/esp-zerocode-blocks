case {{cfg.param_id}}: {
    int level = val.b ? {{prefix}}_RELAY_ACTIVE_LEVEL : !{{prefix}}_RELAY_ACTIVE_LEVEL;
    return gpio_set_level((gpio_num_t){{prefix}}_RELAY_GPIO, level);
}
