static void {{prefix_lc}}_uptime_log_cb(void *arg)
{
    int64_t up_us = esp_timer_get_time();
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    size_t min_heap  = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "{{prefix_lc}}: up=%llds free_heap=%u min_heap=%u",
             (long long)(up_us / 1000000LL), (unsigned)free_heap, (unsigned)min_heap);
}
