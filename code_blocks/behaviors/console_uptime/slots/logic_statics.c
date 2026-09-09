static int {{prefix_lc}}_cmd(int argc, char **argv)
{
    uint64_t us = esp_timer_get_time();
    uint64_t s  = us / 1000000ULL;
    uint32_t d  = (uint32_t)(s / 86400);
    uint32_t h  = (uint32_t)((s % 86400) / 3600);
    uint32_t m  = (uint32_t)((s % 3600) / 60);
    uint32_t sec = (uint32_t)(s % 60);
    /* NEWLIB NANO printf (the default) has no 64-bit support — PRIu64 prints
     * a literal "lu". Total seconds fits u32 for 136 years; cast it. */
    printf("uptime: %" PRIu32 "d %02" PRIu32 "h %02" PRIu32 "m %02" PRIu32 "s  (%" PRIu32 " s)\n",
           d, h, m, sec, (uint32_t)s);
    return 0;
}
