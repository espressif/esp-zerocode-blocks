{
    s_{{prefix_lc}}_ct_lock = xSemaphoreCreateMutex();
    zc_ct_add(&s_{{prefix_lc}}_ct);
    /* Its own low-priority task: a window blocks for 10 mains cycles. */
    xTaskCreate({{prefix_lc}}_ct_task, "{{prefix_lc}}_ct", 3072, NULL, 1, NULL);
}
