case {{cfg.position_param}}:
    s_{{prefix_lc}}_target = (int32_t)val.i16;
    /* Already running answers ESP_ERR_INVALID_STATE — it will reach the new
       target on its own. See step_cb for the race at arrival. */
    if (s_{{prefix_lc}}_step_timer) {
        esp_timer_start_periodic(s_{{prefix_lc}}_step_timer, (uint64_t){{prefix}}_ST_DELAY_US);
    }
    return ESP_OK;
