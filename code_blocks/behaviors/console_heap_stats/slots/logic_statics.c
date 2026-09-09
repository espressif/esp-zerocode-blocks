static int {{prefix_lc}}_cmd(int argc, char **argv)
{
    const struct { uint32_t caps; const char *name; } regions[] = {
        { MALLOC_CAP_8BIT,    "8BIT (general)" },
        { MALLOC_CAP_INTERNAL,"INTERNAL (DRAM)" },
        { MALLOC_CAP_DMA,     "DMA-capable" },
        /* ESP-IDF 6 gates MALLOC_CAP_EXEC behind CONFIG_HEAP_HAS_EXEC_HEAP, so
           on a part with no executable heap (the C3, for one) the macro does
           not exist at all rather than naming an empty region. Reporting a row
           that cannot exist was never useful, so it is simply omitted there. */
#ifdef MALLOC_CAP_EXEC
        { MALLOC_CAP_EXEC,    "EXEC (IRAM)" },
#endif
    };
    printf("%-22s  %12s  %12s  %12s\n", "region", "free", "largest", "min_free");
    for (size_t i = 0; i < sizeof(regions) / sizeof(regions[0]); i++) {
        size_t f   = heap_caps_get_free_size(regions[i].caps);
        size_t lg  = heap_caps_get_largest_free_block(regions[i].caps);
        size_t mn  = heap_caps_get_minimum_free_size(regions[i].caps);
        printf("%-22s  %12u  %12u  %12u\n", regions[i].name,
               (unsigned)f, (unsigned)lg, (unsigned)mn);
    }
    return 0;
}
