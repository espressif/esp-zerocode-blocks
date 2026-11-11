#include <stddef.h>
#include <esp_err.h>

#define {{prefix}}_TG_MAX_PENDING {{cfg.max_pending}}
#define {{prefix}}_TG_POLL_S      {{cfg.poll_seconds}}
#define {{prefix}}_TG_MAX_RETRY_S  {{cfg.max_retry_s}}
#define {{prefix}}_TG_TRIGGER     "{{cfg.trigger_text}}"
/* Longest message kept and sent, in bytes. */
#define TELEGRAM_NOTIFY_MAX_TEXT  1000
/* Longest key telegram_notify_send_keyed takes, in bytes. */
#define TELEGRAM_NOTIFY_MAX_KEY   15

#ifdef __cplusplus
extern "C" {
#endif
/* behaviors/telegram_notify — see its block.yml. */
typedef void (*telegram_notify_reply_cb_t)(const char *text, void *ctx);
esp_err_t telegram_notify_send(const char *text);
esp_err_t telegram_notify_send_keyed(const char *key, const char *text);
esp_err_t telegram_notify_on_reply(telegram_notify_reply_cb_t cb, void *ctx);
size_t telegram_notify_pending(void);
#ifdef __cplusplus
}
#endif
