/* Register for one device+location. The framework holds the model and the
 * worker; this only says which tool-calls belong to this instance.
 *
 * Not fatal on failure: the listener table is a fixed size and a product that
 * outgrows it should still boot and still work by every other means.
 * app_lm logs which instance was dropped. */
{
    esp_err_t {{prefix_lc}}_err = app_lm_register("{{cfg.device}}", "{{cfg.location}}",
                                                 {{prefix_lc}}_on_action, NULL);
    if ({{prefix_lc}}_err != ESP_OK) {
        ESP_LOGE(TAG, "{{prefix_lc}}: could not register {{cfg.device}}/{{cfg.location}} (%s)",
                 esp_err_to_name({{prefix_lc}}_err));
    } else {
        ESP_LOGI(TAG, "{{prefix_lc}}: listening for {{cfg.device}} in {{cfg.location}}");
    }
}
