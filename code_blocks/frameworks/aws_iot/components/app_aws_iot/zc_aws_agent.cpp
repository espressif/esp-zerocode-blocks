/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * The MQTT session: connect, reconnect, resubscribe, and the coreMQTT-Agent
 * command loop.
 *
 * ONE TASK owns MQTTContext_t, because coreMQTT is not thread-safe. Every other
 * task reaches the session through coreMQTT-Agent's command queue, which is
 * what makes app_aws_iot_publish() callable from anywhere.
 *
 * One task, not the two of Amazon's reference integration: MQTTAgent_CommandLoop
 * blocks until the session drops, so the same task can own the reconnect loop
 * around it. Commands enqueued while disconnected simply wait, which is the
 * behaviour a caller wants anyway.
 */

#include "zc_aws_internal.h"

#include <stdlib.h>
#include <string.h>

#include <esp_log.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <backoff_algorithm.h>
#include <clock.h>
#include <core_mqtt_agent.h>
#include <network_transport.h>
/* coreMQTT-Agent's FreeRTOS port: the queue-backed message context and the
 * command pool. core_mqtt_agent.h declares the interface but leaves
 * MQTTAgentMessageContext_t opaque — the port completes it. */
#include <freertos_agent_message.h>
#include <freertos_command_pool.h>

/* The command context coreMQTT-Agent hands back on completion. We define the
 * struct; the library only ever passes the pointer around.
 *
 * Both variants start with this header so one completion callback can free
 * either. The whole point of the allocation is that coreMQTT-Agent does NOT
 * copy: a queued command keeps pointing at the caller's MQTTPublishInfo_t,
 * topic and payload, so a caller that built them on its stack has a
 * use-after-free that only shows up under load. Copying here makes the public
 * API safe to call the obvious way. */
struct MQTTAgentCommandContext {
    uint8_t kind;   /* 0 = publish, 1 = subscribe */
};

#define ZC_CMD_PUBLISH    0
#define ZC_CMD_SUBSCRIBE  1

typedef struct {
    struct MQTTAgentCommandContext hdr;
    MQTTPublishInfo_t info;
    /* topic bytes, NUL, then payload bytes follow inline */
} zc_pub_cmd_t;

typedef struct {
    struct MQTTAgentCommandContext hdr;
    MQTTAgentSubscribeArgs_t args;
    MQTTSubscribeInfo_t info;
    /* filter bytes and NUL follow inline */
} zc_sub_cmd_t;

static MQTTAgentContext_t   s_agent;
static NetworkContext_t     s_net;
static SemaphoreHandle_t    s_tls_mutex;
static uint8_t              s_net_buf[ZC_AWS_NET_BUF_SIZE];
static MQTTPubAckInfo_t     s_out_acks[ZC_AWS_MAX_SUBS + 8];
static MQTTPubAckInfo_t     s_in_acks[ZC_AWS_MAX_SUBS + 8];
static MQTTAgentMessageContext_t s_msg_ctx;
static TaskHandle_t         s_task;
static volatile bool        s_agent_ready;   /* MQTTAgent_Init has succeeded */

MQTTAgentContext_t *app_aws_iot_agent(void)
{
    return s_agent_ready ? &s_agent : NULL;
}

/* ── agent callbacks ─────────────────────────────────────────────────── */

static void incoming_publish_cb(MQTTAgentContext_t *ctx, uint16_t packet_id,
                                MQTTPublishInfo_t *pub)
{
    (void)ctx;
    (void)packet_id;
    zc_aws_subs_dispatch(pub);
}

static void command_complete_cb(MQTTAgentCommandContext_t *cmd_ctx,
                                MQTTAgentReturnInfo_t *ret)
{
    if (cmd_ctx == NULL) return;

    if (ret != NULL && ret->returnCode != MQTTSuccess) {
        ESP_LOGW(ZC_AWS_TAG, "%s command failed: %s",
                 cmd_ctx->kind == ZC_CMD_SUBSCRIBE ? "SUBSCRIBE" : "PUBLISH",
                 MQTT_Status_strerror(ret->returnCode));
    } else if (cmd_ctx->kind == ZC_CMD_SUBSCRIBE && ret != NULL &&
               ret->pSubackCodes != NULL && ret->pSubackCodes[0] == MQTTSubAckFailure) {
        /* A SUBACK failure code is the broker refusing the filter — almost
         * always an IoT policy that does not grant iot:Subscribe on it. Worth
         * naming, because the symptom is silence. */
        ESP_LOGE(ZC_AWS_TAG, "broker REFUSED a subscription — check the thing's IoT policy");
    }

    free(cmd_ctx);
}

/* ── subscriptions ───────────────────────────────────────────────────── */

void zc_aws_agent_send_subscribe(size_t index)
{
    const char *filter = NULL;
    uint16_t len = 0;
    uint8_t qos = 0;

    if (!s_agent_ready || !g_zc_aws_connected) return;
    if (!zc_aws_subs_get(index, &filter, &len, &qos)) return;

    zc_sub_cmd_t *cmd = (zc_sub_cmd_t *)calloc(1, sizeof(zc_sub_cmd_t) + len + 1);
    if (cmd == NULL) {
        ESP_LOGE(ZC_AWS_TAG, "out of memory subscribing \"%s\"", filter);
        return;
    }
    char *filter_copy = (char *)(cmd + 1);
    memcpy(filter_copy, filter, len);
    filter_copy[len] = '\0';

    cmd->hdr.kind = ZC_CMD_SUBSCRIBE;
    cmd->info.qos = (qos == 0) ? MQTTQoS0 : MQTTQoS1;
    cmd->info.pTopicFilter = filter_copy;
    cmd->info.topicFilterLength = len;
    cmd->args.pSubscribeInfo = &cmd->info;
    cmd->args.numSubscriptions = 1;

    MQTTAgentCommandInfo_t ci = {};
    ci.cmdCompleteCallback = command_complete_cb;
    ci.pCmdCompleteCallbackContext = &cmd->hdr;
    ci.blockTimeMs = 1000;

    MQTTStatus_t st = MQTTAgent_Subscribe(&s_agent, &cmd->args, &ci);
    if (st != MQTTSuccess) {
        ESP_LOGW(ZC_AWS_TAG, "could not enqueue SUBSCRIBE \"%s\": %s",
                 filter_copy, MQTT_Status_strerror(st));
        free(cmd);
    }
}

static void resubscribe_all(void)
{
    size_t n = zc_aws_subs_count();
    for (size_t i = 0; i < n; i++) {
        zc_aws_agent_send_subscribe(i);
    }
    if (n > 0) {
        ESP_LOGI(ZC_AWS_TAG, "replayed %u subscription(s)", (unsigned)n);
    }
}

/* ── public API ──────────────────────────────────────────────────────── */

esp_err_t app_aws_iot_subscribe(const char *topic_filter, uint8_t qos,
                                zc_aws_msg_cb_t cb, void *ctx)
{
    esp_err_t err = zc_aws_subs_add(topic_filter, qos, cb, ctx);
    if (err != ESP_OK) return err;

    /* Registered before the session exists? It is replayed at connect. Already
     * connected? Send it now so the caller does not wait for a reconnect. */
    if (g_zc_aws_connected) {
        zc_aws_agent_send_subscribe(zc_aws_subs_count() - 1);
    }
    return ESP_OK;
}

esp_err_t app_aws_iot_publish(const char *topic, const void *payload, size_t len,
                              uint8_t qos, TickType_t block)
{
    if (topic == NULL || topic[0] == '\0') return ESP_ERR_INVALID_ARG;
    if (qos > 1) return ESP_ERR_INVALID_ARG;          /* AWS has no QoS 2 */
    if (payload == NULL && len > 0) return ESP_ERR_INVALID_ARG;
    if (!s_agent_ready || !g_zc_aws_connected) return ESP_ERR_INVALID_STATE;

    size_t topic_len = strlen(topic);
    if (topic_len > UINT16_MAX) return ESP_ERR_INVALID_ARG;

    zc_pub_cmd_t *cmd = (zc_pub_cmd_t *)calloc(1, sizeof(zc_pub_cmd_t) + topic_len + 1 + len);
    if (cmd == NULL) return ESP_ERR_NO_MEM;

    char *topic_copy = (char *)(cmd + 1);
    memcpy(topic_copy, topic, topic_len);
    topic_copy[topic_len] = '\0';
    char *payload_copy = topic_copy + topic_len + 1;
    if (len > 0) memcpy(payload_copy, payload, len);

    cmd->hdr.kind = ZC_CMD_PUBLISH;
    cmd->info.qos = (qos == 0) ? MQTTQoS0 : MQTTQoS1;
    cmd->info.pTopicName = topic_copy;
    cmd->info.topicNameLength = (uint16_t)topic_len;
    cmd->info.pPayload = (len > 0) ? payload_copy : NULL;
    cmd->info.payloadLength = len;

    MQTTAgentCommandInfo_t ci = {};
    ci.cmdCompleteCallback = command_complete_cb;
    ci.pCmdCompleteCallbackContext = &cmd->hdr;
    ci.blockTimeMs = (uint32_t)(block * portTICK_PERIOD_MS);

    MQTTStatus_t st = MQTTAgent_Publish(&s_agent, &cmd->info, &ci);
    if (st != MQTTSuccess) {
        free(cmd);
        /* MQTTSendFailed here is the command QUEUE being full for blockTimeMs,
         * not the network. */
        return (st == MQTTSendFailed) ? ESP_ERR_TIMEOUT : ESP_FAIL;
    }
    return ESP_OK;
}

/* ── connect ─────────────────────────────────────────────────────────── */

static bool session_connect(bool clean_session)
{
    memset(&s_net, 0, sizeof(s_net));
    s_net.xTlsContextSemaphore = s_tls_mutex;
    s_net.pxTls = NULL;
    s_net.pcHostname = g_zc_aws_endpoint;
    s_net.xPort = (int)g_zc_aws_port;
    /* SNI is MANDATORY for AWS IoT — the endpoint is multi-tenant. The field
     * name is inverted: 0 KEEPS server name indication on. */
    s_net.disableSni = 0;
    s_net.pcServerRootCA = (g_zc_aws_rootca != NULL) ? g_zc_aws_rootca : ZC_AWS_ROOT_CA;
    s_net.pcServerRootCASize = (uint32_t)((g_zc_aws_rootca != NULL) ? g_zc_aws_rootca_len
                                                                   : zc_aws_root_ca_size());
    s_net.pcClientCert = g_zc_aws_cert;
    s_net.pcClientCertSize = (uint32_t)g_zc_aws_cert_len;
    s_net.pcClientKey = g_zc_aws_key;
    s_net.pcClientKeySize = (uint32_t)g_zc_aws_key_len;
    /* ALPN only on 443. On 8883 the protocol is implied, and offering
     * x-amzn-mqtt-ca there makes AWS drop the handshake. */
    static const char *alpn[] = { "x-amzn-mqtt-ca", NULL };
    s_net.pAlpnProtos = (g_zc_aws_port == 443) ? alpn : NULL;

    if (xTlsConnect(&s_net) != TLS_TRANSPORT_SUCCESS) {
        ESP_LOGW(ZC_AWS_TAG, "TLS connect to %s:%d failed",
                 g_zc_aws_endpoint, (int)g_zc_aws_port);
        return false;
    }

    TransportInterface_t transport = {};
    transport.pNetworkContext = &s_net;
    transport.send = espTlsTransportSend;
    transport.recv = espTlsTransportRecv;
    transport.writev = NULL;

    MQTTFixedBuffer_t netbuf = {};
    netbuf.pBuffer = s_net_buf;
    netbuf.size = sizeof(s_net_buf);

    MQTTAgentMessageInterface_t msg = {};
    msg.pMsgCtx = &s_msg_ctx;
    msg.send = Agent_MessageSend;
    msg.recv = Agent_MessageReceive;
    msg.getCommand = Agent_GetCommand;
    msg.releaseCommand = Agent_ReleaseCommand;

    /* Clock_GetTimeMs is posix_compat's — coreMQTT keeps no clock of its own,
     * and a dummy one silently disables keep-alive. */
    MQTTStatus_t st = MQTTAgent_Init(&s_agent, &msg, &netbuf, &transport,
                                     Clock_GetTimeMs, incoming_publish_cb, NULL);
    if (st == MQTTSuccess) {
        /* REQUIRED for QoS1. Without it every QoS1 publish is rejected before
         * it reaches the wire. */
        st = MQTT_InitStatefulQoS(&s_agent.mqttContext,
                                  s_out_acks, sizeof(s_out_acks) / sizeof(s_out_acks[0]),
                                  s_in_acks, sizeof(s_in_acks) / sizeof(s_in_acks[0]));
    }
    if (st != MQTTSuccess) {
        ESP_LOGE(ZC_AWS_TAG, "MQTTAgent_Init: %s", MQTT_Status_strerror(st));
        xTlsDisconnect(&s_net);
        return false;
    }
    s_agent_ready = true;

    MQTTConnectInfo_t ci = {};
    ci.cleanSession = clean_session;
    ci.pClientIdentifier = g_zc_aws_thing;
    ci.clientIdentifierLength = (uint16_t)strlen(g_zc_aws_thing);
    ci.keepAliveSeconds = ZC_AWS_KEEPALIVE_S;

    bool session_present = false;
    st = MQTT_Connect(&s_agent.mqttContext, &ci, NULL, ZC_AWS_CONNACK_MS, &session_present);
    if (st != MQTTSuccess) {
        ESP_LOGW(ZC_AWS_TAG, "MQTT_Connect: %s", MQTT_Status_strerror(st));
        xTlsDisconnect(&s_net);
        return false;
    }

    if (!clean_session) {
        st = MQTTAgent_ResumeSession(&s_agent, session_present);
        if (st != MQTTSuccess) {
            ESP_LOGW(ZC_AWS_TAG, "MQTTAgent_ResumeSession: %s", MQTT_Status_strerror(st));
        }
    }

    ESP_LOGI(ZC_AWS_TAG, "connected — thing \"%s\" at %s:%d (session %s)",
             g_zc_aws_thing, g_zc_aws_endpoint, (int)g_zc_aws_port,
             session_present ? "resumed" : "new");

    g_zc_aws_connected = true;
    zc_aws_post_event(ZC_AWS_EVENT_CONNECTED);

    /* THE reason subscriptions are declarative. When the broker reports no
     * surviving session every filter must go back on the wire, or the device
     * stays connected and silently stops receiving. */
    if (!session_present) {
        resubscribe_all();
    }
    return true;
}

/* ── the session task ────────────────────────────────────────────────── */

static void session_task(void *arg)
{
    (void)arg;
    bool clean_session = true;   /* only the very first connect */

    /* Blocks for a grace period, then decides whether this block owns Wi-Fi. */
    zc_aws_net_start();

    for (;;) {
        if (!zc_aws_configured() || !g_zc_aws_net_up) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (session_connect(clean_session)) {
            clean_session = false;

            /* Blocks until the session drops or MQTTAgent_Terminate is called. */
            MQTTStatus_t st = MQTTAgent_CommandLoop(&s_agent);

            g_zc_aws_connected = false;
            zc_aws_post_event(ZC_AWS_EVENT_DISCONNECTED);
            ESP_LOGW(ZC_AWS_TAG, "session ended: %s", MQTT_Status_strerror(st));

            (void)MQTT_Disconnect(&s_agent.mqttContext);
            (void)xTlsDisconnect(&s_net);

            /* Straight back round: a drop after a working session is usually
             * transient, and backing off from the first retry delays recovery
             * for no reason. */
            continue;
        }

        /* Connect failed. Back off with jitter from the hardware RNG — the
         * reference example seeds rand() from tv_nsec, which on a device with
         * no RTC is near-constant across boots, so a whole fleet retries in
         * lockstep after a regional outage. */
        BackoffAlgorithmContext_t backoff;
        BackoffAlgorithm_InitializeParams(&backoff, ZC_AWS_BACKOFF_BASE_MS,
                                          ZC_AWS_BACKOFF_MAX_MS,
                                          BACKOFF_ALGORITHM_RETRY_FOREVER);
        uint16_t delay_ms = ZC_AWS_BACKOFF_BASE_MS;
        (void)BackoffAlgorithm_GetNextBackoff(&backoff, esp_random(), &delay_ms);
        ESP_LOGI(ZC_AWS_TAG, "retrying in %u ms", (unsigned)delay_ms);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

esp_err_t zc_aws_agent_start(void)
{
    s_tls_mutex = xSemaphoreCreateMutex();
    if (s_tls_mutex == NULL) {
        ESP_LOGE(ZC_AWS_TAG, "could not create the TLS context mutex");
        return ESP_ERR_NO_MEM;
    }

    Agent_InitializePool();
    s_msg_ctx.queue = xQueueCreate(ZC_AWS_CMD_QUEUE_LEN, sizeof(MQTTAgentCommand_t *));
    if (s_msg_ctx.queue == NULL) {
        ESP_LOGE(ZC_AWS_TAG, "could not create the agent command queue");
        return ESP_ERR_NO_MEM;
    }

    /* 8 KB covers mbedTLS's handshake frames on top of coreMQTT and the agent. */
    if (xTaskCreate(session_task, "aws_iot", 8192, NULL, 5, &s_task) != pdPASS) {
        ESP_LOGE(ZC_AWS_TAG, "could not start the AWS IoT session task");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
