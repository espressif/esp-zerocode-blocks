/* Vision event 0 = motion. Motion IS occupancy for a camera sensor. */
if (event_id == 0) {
    app_driver_param_val_t v = { .b = (value != 0) };
    app_driver_set_param({{cfg.occupied_param}}, v, s_handle);
}
