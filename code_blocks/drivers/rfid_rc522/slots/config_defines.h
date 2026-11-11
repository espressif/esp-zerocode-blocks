#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#define {{prefix}}_RFID_IF_i2c 1
#define {{prefix}}_RFID_I2C_PORT  {{cfg.i2c_port}}
#define {{prefix}}_RFID_I2C_ADDR  {{cfg.i2c_address}}
#define {{prefix}}_RFID_RST       {{cfg.rst_gpio}}
#define {{prefix}}_RFID_POLL_MS   {{cfg.poll_ms}}

#ifdef __cplusplus
extern "C" {
#endif
/* drivers/rfid_rc522 — see its block.yml. A card arriving (present = true) or
 * taken away (present = false, with that card's UID); uid is valid only during
 * the call. Runs on the reader's task. */
typedef void (*rfid_card_cb_t)(const uint8_t *uid, uint8_t uid_len, bool present, void *ctx);
esp_err_t rfid_on_card(rfid_card_cb_t cb, void *ctx);
#ifdef __cplusplus
}
#endif
