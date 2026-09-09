/* {{prefix_lc}}: native Zigbee SDK 2.0 OTA client → inactive esp_ota
 * partition. The SDK 2.0 progress callback supplies blocks from the complete
 * Zigbee OTA file, so this parser follows the native ota_client example and
 * strips the OTA header and each six-byte sub-element header before writing
 * the upgrade-image element. */
#define {{prefix}}_ZB_OTA_MANUFACTURER_ID 0x131bU
#define {{prefix}}_ZB_OTA_IMAGE_TYPE      0xffbfU
#define {{prefix}}_ZB_OTA_FILE_VERSION    0x01010100U
#define {{prefix}}_ZB_OTA_QUERY_MINUTES   (24U * 60U)
#define {{prefix}}_ZB_OTA_BLOCK_SIZE      64U
#define {{prefix}}_ZB_OTA_ELEMENT_HEADER_LEN (sizeof(uint16_t) + sizeof(uint32_t))

typedef struct {
    uint32_t offset;
    uint32_t total_offset;
    uint32_t total_image_size;
    uint16_t in_length;
    uint8_t *in;
    ezb_zcl_ota_file_header_t header;
    struct {
        uint16_t type;
        uint32_t total;
        uint16_t length;
        uint8_t *val;
    } __attribute__((packed)) element;
} {{prefix_lc}}_zb_ota_parser_t;

static esp_ota_handle_t s_{{prefix_lc}}_handle = 0;
static const esp_partition_t *s_{{prefix_lc}}_partition = NULL;
static bool s_{{prefix_lc}}_in_progress = false;
static uint32_t s_{{prefix_lc}}_received = 0;
static uint32_t s_{{prefix_lc}}_chunks = 0;
static uint32_t s_{{prefix_lc}}_current_file_version = {{prefix}}_ZB_OTA_FILE_VERSION;
static {{prefix_lc}}_zb_ota_parser_t s_{{prefix_lc}}_parser;
static TaskHandle_t s_{{prefix_lc}}_query_task = NULL;
static volatile bool s_{{prefix_lc}}_on_network = false;

static uint16_t {{prefix_lc}}_zb_ota_copy(uint8_t *dst, uint16_t dst_offset,
                                          uint16_t dst_total, const uint8_t *src,
                                          uint16_t src_total)
{
    uint16_t size = 0;
    if (dst_offset < dst_total) {
        size = (uint16_t)((dst_total - dst_offset) < src_total ?
                          (dst_total - dst_offset) : src_total);
        memcpy(dst + dst_offset, src, size);
    }
    return size;
}

static void {{prefix_lc}}_zb_ota_parser_advance({{prefix_lc}}_zb_ota_parser_t *parser,
                                                uint16_t count)
{
    parser->offset += count;
    parser->total_offset += count;
    parser->in += count;
    parser->in_length -= count;
}

static esp_err_t {{prefix_lc}}_zb_ota_parser_process({{prefix_lc}}_zb_ota_parser_t *parser)
{
    if (!parser || !parser->in || parser->in_length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t copied = 0;
    if (parser->total_offset < sizeof(parser->header)) {
        copied = {{prefix_lc}}_zb_ota_copy((uint8_t *)&parser->header, parser->offset,
                                           sizeof(parser->header), parser->in,
                                           parser->in_length);
        {{prefix_lc}}_zb_ota_parser_advance(parser, copied);
        if (parser->total_offset >= sizeof(parser->header)) {
            parser->offset = 0;
        }
        return parser->in_length ? ESP_ERR_NOT_FINISHED : ESP_OK;
    }

    if (parser->total_offset < parser->header.hdr_length) {
        copied = (uint16_t)((parser->header.hdr_length - parser->total_offset) <
                            parser->in_length ?
                            (parser->header.hdr_length - parser->total_offset) :
                            parser->in_length);
        {{prefix_lc}}_zb_ota_parser_advance(parser, copied);
        if (parser->total_offset >= parser->header.hdr_length) {
            parser->offset = 0;
        }
        return parser->in_length ? ESP_ERR_NOT_FINISHED : ESP_OK;
    }

    if (parser->offset < {{prefix}}_ZB_OTA_ELEMENT_HEADER_LEN) {
        copied = {{prefix_lc}}_zb_ota_copy((uint8_t *)&parser->element,
                                           parser->offset,
                                           {{prefix}}_ZB_OTA_ELEMENT_HEADER_LEN,
                                           parser->in, parser->in_length);
        {{prefix_lc}}_zb_ota_parser_advance(parser, copied);
        if (parser->offset >= {{prefix}}_ZB_OTA_ELEMENT_HEADER_LEN) {
            parser->element.total += {{prefix}}_ZB_OTA_ELEMENT_HEADER_LEN;
        }
        parser->element.length = 0;
        parser->element.val = NULL;
        return parser->in_length ? ESP_ERR_NOT_FINISHED : ESP_OK;
    }

    if (parser->offset < parser->element.total) {
        parser->element.length =
            (uint16_t)((parser->element.total - parser->offset) < parser->in_length ?
                       (parser->element.total - parser->offset) : parser->in_length);
        parser->element.val = parser->in;
        copied = parser->element.length;
        {{prefix_lc}}_zb_ota_parser_advance(parser, copied);
        if (parser->offset >= parser->element.total) {
            parser->offset = 0;
        }
        return parser->in_length ? ESP_ERR_NOT_FINISHED : ESP_OK;
    }

    return ESP_FAIL;
}

static esp_err_t {{prefix_lc}}_zb_ota_abort(void)
{
    esp_err_t err = ESP_OK;
    if (s_{{prefix_lc}}_in_progress && s_{{prefix_lc}}_handle) {
        err = esp_ota_abort(s_{{prefix_lc}}_handle);
    }
    s_{{prefix_lc}}_handle = 0;
    s_{{prefix_lc}}_in_progress = false;
    memset(&s_{{prefix_lc}}_parser, 0, sizeof(s_{{prefix_lc}}_parser));
    return err;
}

static esp_err_t {{prefix_lc}}_zb_ota_query_image(void)
{
    ezb_zcl_ota_upgrade_query_next_image_req_cmd_t request = {
        .cmd_ctrl = {
            .dst_ep = 0xff,
            .src_ep = {{cfg.zb_ota_endpoint}},
        },
        .payload = {
            .manuf_code = {{prefix}}_ZB_OTA_MANUFACTURER_ID,
            .image_type = {{prefix}}_ZB_OTA_IMAGE_TYPE,
            .file_version = {{prefix}}_ZB_OTA_FILE_VERSION,
        },
    };
    ezb_address_set_short(&request.cmd_ctrl.dst_addr, 0xffff);
    return esp_zigbee_err_to_esp(ezb_zcl_ota_upgrade_query_next_image_cmd_req(&request));
}

static void {{prefix_lc}}_zb_ota_query_task(void *arg)
{
    (void)arg;
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE,
            pdMS_TO_TICKS({{prefix}}_ZB_OTA_QUERY_MINUTES * 60U * 1000U));
        if (!s_{{prefix_lc}}_on_network) {
            continue;
        }
        if (esp_zigbee_lock_acquire(portMAX_DELAY)) {
            esp_err_t err = {{prefix_lc}}_zb_ota_query_image();
            esp_zigbee_lock_release();
            if (err != ESP_OK) {
                ESP_LOGI(TAG, "{{prefix_lc}}: OTA image query not sent (%s)",
                         esp_err_to_name(err));
            }
        }
    }
}

static void {{prefix_lc}}_zb_ota_arm_query(void)
{
    s_{{prefix_lc}}_on_network = true;
    if (!s_{{prefix_lc}}_query_task) {
        (void)ezb_zcl_ota_upgrade_set_download_block_size(
            {{cfg.zb_ota_endpoint}}, {{prefix}}_ZB_OTA_BLOCK_SIZE);
        if (xTaskCreate({{prefix_lc}}_zb_ota_query_task, "{{prefix_lc}}_ota_query",
                        3072, NULL, 5, &s_{{prefix_lc}}_query_task) != pdPASS) {
            ESP_LOGE(TAG, "{{prefix_lc}}: failed to start periodic OTA query task");
            return;
        }
    }
    xTaskNotifyGive(s_{{prefix_lc}}_query_task);
}

/* Runs ON the Zigbee task via zb_action_handler (no stack lock needed). */
static void {{prefix_lc}}_zb_ota_upgrade_handler(
    ezb_zcl_ota_upgrade_client_progress_message_t *message)
{
    if (!message) return;
    esp_err_t err = ESP_OK;
    if (message->info.status != EZB_ZCL_STATUS_SUCCESS) {
        ESP_LOGW(TAG, "{{prefix_lc}}: OTA message status %d — aborting",
                 message->info.status);
        message->out.result = EZB_ZCL_STATUS_ABORT;
        return;
    }

    switch (message->in.progress) {
    case EZB_ZCL_OTA_UPGRADE_PROGRESS_START:
        ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee OTA start — image type 0x%04x, version 0x%08x, %u bytes",
                 message->in.start.image_type,
                 (unsigned)message->in.start.file_version,
                 (unsigned)message->in.start.image_size);
        (void){{prefix_lc}}_zb_ota_abort();
        s_{{prefix_lc}}_partition = esp_ota_get_next_update_partition(NULL);
        if (!s_{{prefix_lc}}_partition) {
            ESP_LOGE(TAG, "{{prefix_lc}}: no inactive OTA partition — check the partition table");
            err = ESP_ERR_NOT_FOUND;
            break;
        }
        err = esp_ota_begin(s_{{prefix_lc}}_partition, OTA_SIZE_UNKNOWN,
                            &s_{{prefix_lc}}_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "{{prefix_lc}}: esp_ota_begin failed (%s)",
                     esp_err_to_name(err));
            break;
        }
        s_{{prefix_lc}}_in_progress = true;
        s_{{prefix_lc}}_received = 0;
        s_{{prefix_lc}}_chunks = 0;
        memset(&s_{{prefix_lc}}_parser, 0, sizeof(s_{{prefix_lc}}_parser));
        s_{{prefix_lc}}_parser.total_image_size = message->in.start.image_size;
        break;

    case EZB_ZCL_OTA_UPGRADE_PROGRESS_RECEIVING:
        if (!s_{{prefix_lc}}_in_progress || !message->in.receiving.block) {
            err = ESP_ERR_INVALID_STATE;
            break;
        }
        if (message->in.receiving.file_offset != s_{{prefix_lc}}_parser.total_offset) {
            ESP_LOGE(TAG, "{{prefix_lc}}: non-sequential OTA block at %u (expected %u)",
                     (unsigned)message->in.receiving.file_offset,
                     (unsigned)s_{{prefix_lc}}_parser.total_offset);
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        s_{{prefix_lc}}_parser.in = message->in.receiving.block;
        s_{{prefix_lc}}_parser.in_length = message->in.receiving.block_size;
        do {
            err = {{prefix_lc}}_zb_ota_parser_process(&s_{{prefix_lc}}_parser);
            if (s_{{prefix_lc}}_parser.element.length &&
                s_{{prefix_lc}}_parser.element.val) {
                if (s_{{prefix_lc}}_parser.element.type == 0) {
                    if (s_{{prefix_lc}}_parser.element.total >
                        s_{{prefix_lc}}_partition->size +
                        {{prefix}}_ZB_OTA_ELEMENT_HEADER_LEN) {
                        err = ESP_ERR_INVALID_SIZE;
                        break;
                    }
                    err = esp_ota_write(s_{{prefix_lc}}_handle,
                                        s_{{prefix_lc}}_parser.element.val,
                                        s_{{prefix_lc}}_parser.element.length);
                    if (err != ESP_OK) break;
                    s_{{prefix_lc}}_received +=
                        s_{{prefix_lc}}_parser.element.length;
                } else {
                    ESP_LOGW(TAG, "{{prefix_lc}}: skipping OTA element tag 0x%04x",
                             s_{{prefix_lc}}_parser.element.type);
                }
            }
        } while (err == ESP_ERR_NOT_FINISHED);
        if (err == ESP_OK) {
            s_{{prefix_lc}}_chunks++;
            if ((s_{{prefix_lc}}_chunks % 256U) == 1U) {
                ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee OTA progress %u bytes",
                         (unsigned)s_{{prefix_lc}}_received);
            }
        }
        break;

    case EZB_ZCL_OTA_UPGRADE_PROGRESS_CHECK:
        ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee OTA download complete (%u bytes) — validating",
                 (unsigned)s_{{prefix_lc}}_received);
        if (!s_{{prefix_lc}}_in_progress ||
            s_{{prefix_lc}}_parser.total_offset !=
                s_{{prefix_lc}}_parser.total_image_size) {
            err = ESP_FAIL;
        }
        break;

    case EZB_ZCL_OTA_UPGRADE_PROGRESS_APPLY:
        if (!s_{{prefix_lc}}_in_progress) {
            err = ESP_ERR_INVALID_STATE;
            break;
        }
        err = esp_ota_end(s_{{prefix_lc}}_handle);
        s_{{prefix_lc}}_handle = 0;
        s_{{prefix_lc}}_in_progress = false;
        if (err == ESP_OK) {
            err = esp_ota_set_boot_partition(s_{{prefix_lc}}_partition);
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "{{prefix_lc}}: image activation failed (%s) — staying on current firmware",
                     esp_err_to_name(err));
        }
        break;

    case EZB_ZCL_OTA_UPGRADE_PROGRESS_FINISH:
        ESP_LOGI(TAG, "{{prefix_lc}}: Zigbee OTA applied — rebooting in %u seconds",
                 (unsigned)message->in.finish.count_down_delay);
        esp_restart();
        break;

    case EZB_ZCL_OTA_UPGRADE_PROGRESS_ABORT:
        ESP_LOGW(TAG, "{{prefix_lc}}: Zigbee OTA aborted");
        err = {{prefix_lc}}_zb_ota_abort();
        break;

    default:
        ESP_LOGW(TAG, "{{prefix_lc}}: unknown Zigbee OTA progress 0x%x",
                 message->in.progress);
        break;
    }

    if (err != ESP_OK && s_{{prefix_lc}}_in_progress) {
        (void){{prefix_lc}}_zb_ota_abort();
    }
    message->out.result =
        err == ESP_OK ? EZB_ZCL_STATUS_SUCCESS : EZB_ZCL_STATUS_ABORT;
}

static void {{prefix_lc}}_zb_ota_query_response_handler(
    ezb_zcl_ota_upgrade_query_next_image_rsp_message_t *message)
{
    if (!message) return;
    if (message->in.image.status == EZB_ZCL_OTA_UPGRADE_STATUS_CODE_SUCCESS) {
        ESP_LOGI(TAG, "{{prefix_lc}}: OTA image available — version 0x%08x, %u bytes",
                 (unsigned)message->in.image.file_version,
                 (unsigned)message->in.image.size);
    } else {
        ESP_LOGI(TAG, "{{prefix_lc}}: no OTA image available (status 0x%02x)",
                 message->in.image.status);
    }
    message->out.result = EZB_ZCL_STATUS_SUCCESS;
}
