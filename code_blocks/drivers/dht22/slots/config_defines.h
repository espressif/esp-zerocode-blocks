#define {{prefix}}_DHT_GPIO      {{cfg.gpio}}
#define {{prefix}}_DHT_POLL_MS   {{cfg.poll_interval_ms}}
#define {{prefix}}_DHT_IS_DHT11  {{cfg.is_dht11}}
/* Clamped to the DHT max sample rate (~0.5 Hz). */
#define {{prefix}}_DHT_PERIOD_MS ((uint32_t)({{prefix}}_DHT_POLL_MS < 2000 ? 2000 : {{prefix}}_DHT_POLL_MS))
