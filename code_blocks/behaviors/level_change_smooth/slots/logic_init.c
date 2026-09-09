{
    app_driver_register_solution("{{prefix_lc}}_smooth", {{prefix_lc}}_smooth_cb, NULL);
    xTaskCreate({{prefix_lc}}_smooth_task, "{{prefix_lc}}_sm", 2048, NULL, 4, NULL);
}
