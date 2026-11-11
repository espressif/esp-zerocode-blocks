/* Degrades rather than aborts: no PSRAM (or no `model` partition) leaves
 * voice unable to start, and a device whose other functions still work is
 * better than one that reset-loops. app_audio_init logs the reason. */
{
    esp_err_t audio_err = app_audio_init();
    if (audio_err != ESP_OK) {
        ESP_LOGE(TAG, "voice disabled: %s", esp_err_to_name(audio_err));
    }
}
