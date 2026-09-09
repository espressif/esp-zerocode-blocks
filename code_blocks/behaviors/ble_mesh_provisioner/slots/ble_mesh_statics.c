/* ── {{prefix_lc}}: provisioner ──────────────────────────────────────── */
#define ZC_PROV_OWN_ADDR    0x0001
#define ZC_PROV_GROUP_ADDR  {{cfg.group_addr}}
#define ZC_PROV_MAX_NODES   {{cfg.max_nodes}}
#define ZC_PROV_APP_KEY_HEX "{{cfg.app_key}}"
#define ZC_PROV_MSG_TTL     7
#define ZC_PROV_MAX_RETRIES 3

typedef struct {
    uint8_t  uuid[16];
    uint16_t unicast;
    uint8_t  elem_num;
    uint8_t  step;
    uint8_t  retries;
    bool     in_use;
} zc_prov_node_t;

static zc_prov_node_t s_prov_nodes[ZC_PROV_MAX_NODES];
static uint16_t s_prov_net_idx = ESP_BLE_MESH_KEY_PRIMARY;
static uint16_t s_prov_app_idx = 0x0000;
static uint8_t  s_prov_app_key[16];

/* The configuration a freshly provisioned ZeroCode mesh node needs, in order.
 * Both SERVERS so a controller can write it, and both CLIENTS so it can send —
 * the step that is easy to leave out, and that turns a remote into a device
 * which joins the network and then never says anything. */
typedef struct {
    uint32_t opcode;
    uint16_t model_id;
} zc_prov_step_t;

static const zc_prov_step_t s_prov_steps[] = {
    { ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD,    0 },
    { ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND, ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV },
    { ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND, ESP_BLE_MESH_MODEL_ID_GEN_LEVEL_SRV },
    { ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND, ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI },
    { ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND, ESP_BLE_MESH_MODEL_ID_GEN_LEVEL_CLI },
    { ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD,  ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV },
    { ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD,  ESP_BLE_MESH_MODEL_ID_GEN_LEVEL_SRV },
    { ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET,  ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI },
    { ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET,  ESP_BLE_MESH_MODEL_ID_GEN_LEVEL_CLI },
};

static void zc_prov_parse_app_key(void)
{
    const char *hex = ZC_PROV_APP_KEY_HEX;
    if (strlen(hex) == sizeof(s_prov_app_key) * 2) {
        bool ok = true;
        for (size_t i = 0; i < sizeof(s_prov_app_key) && ok; i++) {
            char pair[3] = { hex[i * 2], hex[i * 2 + 1], '\0' };
            char *end = NULL;
            long value = strtol(pair, &end, 16);
            if (end != pair + 2) {
                ok = false;
            } else {
                s_prov_app_key[i] = (uint8_t)value;
            }
        }
        if (ok) {
            return;
        }
    }
    memset(s_prov_app_key, 0x12, sizeof(s_prov_app_key));
    ESP_LOGW(TAG, "provisioner: app_key is not 32 hexadecimal characters — "
                  "falling back to the built-in development key");
}

static zc_prov_node_t *zc_prov_find(uint16_t unicast)
{
    for (size_t i = 0; i < ZC_MESH_COUNT(s_prov_nodes); i++) {
        if (s_prov_nodes[i].in_use &&
            unicast >= s_prov_nodes[i].unicast &&
            unicast < (uint16_t)(s_prov_nodes[i].unicast + s_prov_nodes[i].elem_num)) {
            return &s_prov_nodes[i];
        }
    }
    return NULL;
}

/* A device that comes back (a factory reset, a re-provision) reuses its own
 * row and starts the configuration chain again, rather than filling the table
 * with copies of itself. */
static zc_prov_node_t *zc_prov_store(const uint8_t uuid[16], uint16_t unicast, uint8_t elem_num)
{
    zc_prov_node_t *slot = NULL;
    for (size_t i = 0; i < ZC_MESH_COUNT(s_prov_nodes); i++) {
        if (s_prov_nodes[i].in_use && !memcmp(s_prov_nodes[i].uuid, uuid, 16)) {
            slot = &s_prov_nodes[i];
            break;
        }
        if (!slot && !s_prov_nodes[i].in_use) {
            slot = &s_prov_nodes[i];
        }
    }
    if (!slot) {
        return NULL;
    }
    memcpy(slot->uuid, uuid, 16);
    slot->unicast = unicast;
    slot->elem_num = elem_num ? elem_num : 1;
    slot->step = 0;
    slot->retries = 0;
    slot->in_use = true;
    return slot;
}

static void zc_prov_run_step(zc_prov_node_t *node)
{
    if (!node || !s_cfg_client.model) {
        return;
    }
    if (node->step >= ZC_MESH_COUNT(s_prov_steps)) {
        ESP_LOGI(TAG, "provisioner: node 0x%04x configured — key bound to its servers "
                      "and its clients, servers subscribed to 0x%04x, clients publishing to it",
                 node->unicast, (unsigned)ZC_PROV_GROUP_ADDR);
        return;
    }
    const zc_prov_step_t *step = &s_prov_steps[node->step];

    esp_ble_mesh_client_common_param_t common = {};
    common.opcode = step->opcode;
    common.model = s_cfg_client.model;
    common.ctx.net_idx = s_prov_net_idx;
    common.ctx.app_idx = s_prov_app_idx;
    common.ctx.addr = node->unicast;
    common.ctx.send_ttl = ZC_PROV_MSG_TTL;
    common.msg_timeout = 0;

    esp_ble_mesh_cfg_client_set_state_t set = {};
    switch (step->opcode) {
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            set.app_key_add.net_idx = s_prov_net_idx;
            set.app_key_add.app_idx = s_prov_app_idx;
            memcpy(set.app_key_add.app_key, s_prov_app_key, sizeof(s_prov_app_key));
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            set.model_app_bind.element_addr = node->unicast;
            set.model_app_bind.model_app_idx = s_prov_app_idx;
            set.model_app_bind.model_id = step->model_id;
            set.model_app_bind.company_id = ESP_BLE_MESH_CID_NVAL;
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD:
            set.model_sub_add.element_addr = node->unicast;
            set.model_sub_add.sub_addr = ZC_PROV_GROUP_ADDR;
            set.model_sub_add.model_id = step->model_id;
            set.model_sub_add.company_id = ESP_BLE_MESH_CID_NVAL;
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET:
            set.model_pub_set.element_addr = node->unicast;
            set.model_pub_set.publish_addr = ZC_PROV_GROUP_ADDR;
            set.model_pub_set.publish_app_idx = s_prov_app_idx;
            set.model_pub_set.cred_flag = false;
            set.model_pub_set.publish_ttl = ZC_PROV_MSG_TTL;
            set.model_pub_set.publish_period = 0;
            set.model_pub_set.publish_retransmit = ESP_BLE_MESH_PUBLISH_TRANSMIT(1, 50);
            set.model_pub_set.model_id = step->model_id;
            set.model_pub_set.company_id = ESP_BLE_MESH_CID_NVAL;
            break;
        default:
            break;
    }

    esp_err_t err = esp_ble_mesh_config_client_set_state(&common, &set);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "provisioner: node 0x%04x step %u could not be sent (%s)",
                 node->unicast, node->step, esp_err_to_name(err));
    }
}

static void zc_prov_cfg_client_cb(esp_ble_mesh_cfg_client_cb_event_t event,
                                  esp_ble_mesh_cfg_client_cb_param_t *param)
{
    if (!param || !param->params) {
        return;
    }
    zc_prov_node_t *node = zc_prov_find(param->params->ctx.addr);
    if (!node) {
        return;
    }
    switch (event) {
        case ESP_BLE_MESH_CFG_CLIENT_SET_STATE_EVT:
            /* Advance on ANY answer, a refusal included: a node that does not
             * carry one of these models still has to be configured for the
             * ones it does, and stopping here is how one absent model leaves a
             * whole device unbound. */
            node->step++;
            node->retries = 0;
            zc_prov_run_step(node);
            break;
        case ESP_BLE_MESH_CFG_CLIENT_TIMEOUT_EVT:
            if (node->retries < ZC_PROV_MAX_RETRIES) {
                node->retries++;
                ESP_LOGW(TAG, "provisioner: node 0x%04x step %u timed out, retry %u",
                         node->unicast, node->step, node->retries);
                zc_prov_run_step(node);
            } else {
                ESP_LOGE(TAG, "provisioner: node 0x%04x step %u gave up after %d retries — "
                              "continuing with the rest of its configuration",
                         node->unicast, node->step, ZC_PROV_MAX_RETRIES);
                node->step++;
                node->retries = 0;
                zc_prov_run_step(node);
            }
            break;
        default:
            break;
    }
}
