#define TG_NS        "zc_tg"
#define TG_QUEUE_NS  "zc_tg_q"
#define TG_URL       "https://api.telegram.org/bot"
#define TG_RESP_MAX  4096
#define TG_KEY_BYTES (TELEGRAM_NOTIFY_MAX_KEY + 1)
#define TG_MAX_BACKOFF_S ({{prefix}}_TG_MAX_RETRY_S < 5 ? 5 : {{prefix}}_TG_MAX_RETRY_S > 3600 ? 3600 : {{prefix}}_TG_MAX_RETRY_S)

static char s_tg_token[100] = {0};
static int64_t s_tg_chat = 0;
static bool s_tg_have_chat = false;
static int64_t s_tg_offset = 0;            /* next update id to read */
static bool s_tg_backlog_skipped = false;  /* once per boot, before the first poll */
static QueueHandle_t s_tg_in = NULL;       /* char *: key (TG_KEY_BYTES) then text */
static SemaphoreHandle_t s_tg_lock = NULL; /* token + chat + s_tg_last */
static TaskHandle_t s_tg_task = NULL;
static volatile bool s_tg_net = false;
static volatile bool s_tg_learn_req = false;
static telegram_notify_reply_cb_t s_tg_reply_cb = NULL;
static void *s_tg_reply_ctx = NULL;
static char s_tg_last[80] = "nothing sent yet"; /* under s_tg_lock: the Telegram task and the console both write it */

typedef struct {
    int status;
    size_t len;
    bool truncated;
} tg_resp_t;

/* ── settings ─────────────────────────────────────────────────────────── */

static void tg_i64_str(int64_t v, char *out, size_t cap)
{
    /* newlib-nano printf has no 64-bit conversions. */
    char tmp[24];
    int i = 0;
    bool neg = v < 0;
    uint64_t u = neg ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
    do {
        tmp[i++] = (char)('0' + (int)(u % 10));
        u /= 10;
    } while (u != 0 && i < (int)sizeof(tmp));
    size_t o = 0;
    if (neg && o + 1 < cap) out[o++] = '-';
    while (i > 0 && o + 1 < cap) out[o++] = tmp[--i];
    out[o] = 0;
}

static bool tg_parse_i64(const char *s, int64_t *out)
{
    if (s == NULL || s[0] == 0) return false;
    char *end = NULL;
    long long v = strtoll(s, &end, 10);
    if (end == s || *end != 0) return false;
    *out = (int64_t)v;
    return true;
}

static void tg_save_i64(const char *key, int64_t v)
{
    nvs_handle_t h;
    if (nvs_open(TG_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i64(h, key, v);
    nvs_commit(h);
    nvs_close(h);
}

static void tg_load_settings(void)
{
    nvs_handle_t h;
    /* Device data (fctry) first; console values override it. */
    esp_err_t err = nvs_flash_init_partition("fctry");
    if ((err == ESP_OK || err == ESP_ERR_INVALID_STATE) &&
        nvs_open_from_partition("fctry", TG_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t n = sizeof(s_tg_token);
        if (nvs_get_str(h, "token", s_tg_token, &n) != ESP_OK) s_tg_token[0] = 0;
        char chat[24] = {0};
        n = sizeof(chat);
        if (nvs_get_str(h, "chat", chat, &n) == ESP_OK && tg_parse_i64(chat, &s_tg_chat)) s_tg_have_chat = true;
        nvs_close(h);
    }
    if (nvs_open(TG_NS, NVS_READONLY, &h) == ESP_OK) {
        char tok[sizeof(s_tg_token)] = {0};
        size_t n = sizeof(tok);
        if (nvs_get_str(h, "token", tok, &n) == ESP_OK && tok[0] != 0) strlcpy(s_tg_token, tok, sizeof(s_tg_token));
        int64_t v = 0;
        if (nvs_get_i64(h, "chat", &v) == ESP_OK) {
            s_tg_chat = v;
            s_tg_have_chat = true;
        }
        if (nvs_get_i64(h, "offset", &v) == ESP_OK) s_tg_offset = v;
        nvs_close(h);
    }
}

static bool tg_get_chat(int64_t *chat)
{
    xSemaphoreTake(s_tg_lock, portMAX_DELAY);
    bool have = s_tg_have_chat;
    *chat = s_tg_chat;
    xSemaphoreGive(s_tg_lock);
    return have;
}

static void tg_set_chat(int64_t chat)
{
    xSemaphoreTake(s_tg_lock, portMAX_DELAY);
    s_tg_chat = chat;
    s_tg_have_chat = true;
    xSemaphoreGive(s_tg_lock);
    tg_save_i64("chat", chat);
}

static bool tg_have_token(void)
{
    xSemaphoreTake(s_tg_lock, portMAX_DELAY);
    bool have = s_tg_token[0] != 0;
    xSemaphoreGive(s_tg_lock);
    return have;
}

/* ── undelivered messages: one NVS slot per text, ordered by an index ── */

/* Each waiting message's text has its own NVS slot (m0..m<max_pending>, one
 * more slot than messages so a new text is written before the old one is
 * erased); the index blob holds their order and keys. A keyed message
 * replaces a waiting one with the same key: the new text goes to the BACK of
 * the queue, the old one is erased. A full queue drops its oldest KEYED
 * message first and an un-keyed one only when nothing waiting is keyed, so a
 * repeating reminder never pushes out a one-off. The section between the
 * tg-queue markers touches no NVS and is compiled on the host by
 * engine/tests/telegram-notify-queue.test.mjs. */
/* tg-queue: begin */
#define TG_QCAP 32 /* ceiling of max_pending */

typedef struct {
    uint8_t n;                                       /* messages waiting */
    uint8_t slot[TG_QCAP];                           /* NVS slot of each, oldest first */
    char key[TG_QCAP][TELEGRAM_NOTIFY_MAX_KEY + 1];  /* "" = un-keyed */
} tg_index_t;

typedef struct {
    int slot;      /* slot the new text goes to */
    int freed;     /* slot no message uses any more, -1 none */
    bool replaced; /* a waiting message with the same key was replaced */
    int dropped;   /* 0 none, 1 the oldest keyed message, 2 the oldest un-keyed one */
    char dropped_key[TELEGRAM_NOTIFY_MAX_KEY + 1];
} tg_push_t;

static int tg_idx_cap(int max_pending)
{
    return max_pending < 1 ? 1 : max_pending > TG_QCAP ? TG_QCAP : max_pending;
}

static int tg_idx_find(const tg_index_t *q, const char *key)
{
    if (key == NULL || key[0] == 0) return -1;
    for (int i = 0; i < q->n; i++) {
        if (strcmp(q->key[i], key) == 0) return i;
    }
    return -1;
}

static void tg_idx_remove(tg_index_t *q, int pos)
{
    for (int i = pos; i + 1 < q->n; i++) {
        q->slot[i] = q->slot[i + 1];
        memcpy(q->key[i], q->key[i + 1], sizeof(q->key[i]));
    }
    q->n--;
    q->slot[q->n] = 0;
    memset(q->key[q->n], 0, sizeof(q->key[q->n]));
}

/* What a full queue drops: the oldest keyed message, else the oldest. */
static int tg_idx_victim(const tg_index_t *q)
{
    for (int i = 0; i < q->n; i++) {
        if (q->key[i][0] != 0) return i;
    }
    return 0;
}

/* A slot in 0..cap no waiting message uses; there are cap + 1 of them. */
static int tg_idx_free_slot(const tg_index_t *q, int cap)
{
    for (int s = 0; s <= cap; s++) {
        bool used = false;
        for (int i = 0; i < q->n && !used; i++) used = q->slot[i] == s;
        if (!used) return s;
    }
    return -1;
}

/* An index read back from flash: false when it cannot be one of ours. */
static bool tg_idx_valid(tg_index_t *q)
{
    if (q->n > TG_QCAP) return false;
    for (int i = 0; i < q->n; i++) {
        if (q->slot[i] > TG_QCAP) return false;
        q->key[i][TELEGRAM_NOTIFY_MAX_KEY] = 0;
    }
    return true;
}

/* Drops messages (keyed first) until at most cap wait; how many it dropped. */
static int tg_idx_trim(tg_index_t *q, int cap)
{
    int dropped = 0;
    while (q->n > cap) {
        tg_idx_remove(q, tg_idx_victim(q));
        dropped++;
    }
    return dropped;
}

/* Puts a message with key ("" = un-keyed) at the back of q, keeping at most
 * cap. The caller writes the text to r.slot, saves q, then erases r.freed. */
static tg_push_t tg_idx_push(tg_index_t *q, int cap, const char *key)
{
    tg_push_t r;
    memset(&r, 0, sizeof(r));
    r.freed = -1;
    if (key == NULL) key = "";
    r.slot = tg_idx_free_slot(q, cap); /* before any removal: never the freed one */
    int pos = tg_idx_find(q, key);
    if (pos >= 0) {
        r.freed = q->slot[pos];
        r.replaced = true;
        tg_idx_remove(q, pos);
    } else if (q->n >= cap) {
        int v = tg_idx_victim(q);
        r.dropped = q->key[v][0] != 0 ? 1 : 2;
        memcpy(r.dropped_key, q->key[v], sizeof(r.dropped_key));
        r.freed = q->slot[v];
        tg_idx_remove(q, v);
    }
    q->slot[q->n] = (uint8_t)r.slot;
    snprintf(q->key[q->n], sizeof(q->key[q->n]), "%s", key);
    q->n++;
    return r;
}
/* What a sendMessage answer means for the message at the head of the queue.
 * A refusal about the token or the chat is the owner's to fix (tg-token,
 * tg-learn), so the message waits for it; any other 4xx refuses THIS message
 * (too long, malformed) and sending it again can never succeed, so it is
 * dropped instead of holding every message behind it. A server error is
 * retried, at most TG_MAX_SERVER_FAILS times in a row. */
typedef enum {
    TG_SENT,       /* delivered */
    TG_RETRY,      /* try again after the backoff (or retry_after) */
    TG_WAIT_OWNER, /* token or chat refused: wait, the owner fixes it */
    TG_DROP,       /* this message can never be delivered */
} tg_verdict_t;

#define TG_MAX_SERVER_FAILS 8

static tg_verdict_t tg_send_verdict(int status, const char *body, int server_fails)
{
    if (status == 200) return TG_SENT;
    if (status == 429) return TG_RETRY;
    if (status == 401 || status == 403 || status == 404) return TG_WAIT_OWNER;
    if (status >= 400 && status < 500) {
        if (body != NULL && (strstr(body, "chat not found") != NULL || strstr(body, "chat_id") != NULL)) return TG_WAIT_OWNER;
        return TG_DROP;
    }
    return server_fails + 1 >= TG_MAX_SERVER_FAILS ? TG_DROP : TG_RETRY;
}
/* tg-queue: end */

#define TG_CAP (tg_idx_cap({{prefix}}_TG_MAX_PENDING))

static tg_index_t s_tg_q; /* order and keys; the texts are in NVS */

static void tg_slot_key(int slot, char *key, size_t cap)
{
    snprintf(key, cap, "m%d", slot);
}

static esp_err_t tg_store_save_index(nvs_handle_t h)
{
    esp_err_t err = nvs_set_blob(h, "idx", &s_tg_q, sizeof(s_tg_q));
    if (err == ESP_OK) err = nvs_commit(h);
    return err;
}

static void tg_store_load(void)
{
    memset(&s_tg_q, 0, sizeof(s_tg_q));
    nvs_handle_t h;
    if (nvs_open(TG_QUEUE_NS, NVS_READWRITE, &h) != ESP_OK) return;
    size_t len = sizeof(s_tg_q);
    bool changed = false;
    if (nvs_get_blob(h, "idx", &s_tg_q, &len) != ESP_OK || len != sizeof(s_tg_q) || !tg_idx_valid(&s_tg_q)) {
        memset(&s_tg_q, 0, sizeof(s_tg_q));
    }
    if (tg_idx_trim(&s_tg_q, TG_CAP) > 0) changed = true;
    if (changed) tg_store_save_index(h);
    /* Texts no waiting message uses (a reboot between two writes). */
    for (int s = 0; s <= TG_QCAP; s++) {
        bool used = false;
        for (int i = 0; i < s_tg_q.n && !used; i++) used = s_tg_q.slot[i] == s;
        char key[16];
        tg_slot_key(s, key, sizeof(key));
        if (!used) nvs_erase_key(h, key);
    }
    nvs_commit(h);
    nvs_close(h);
    if (s_tg_q.n > 0) ESP_LOGI(TAG, "telegram: %u message(s) from before the restart still to send", (unsigned)s_tg_q.n);
}

static uint32_t tg_store_count(void)
{
    return s_tg_q.n;
}

static void tg_store_push(const char *key, const char *text)
{
    nvs_handle_t h;
    if (nvs_open(TG_QUEUE_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGE(TAG, "telegram: storage unavailable - message dropped");
        return;
    }
    tg_index_t before = s_tg_q;
    tg_push_t p = tg_idx_push(&s_tg_q, TG_CAP, key);
    char slot[16];
    tg_slot_key(p.slot, slot, sizeof(slot));
    if (nvs_set_str(h, slot, text) != ESP_OK || tg_store_save_index(h) != ESP_OK) {
        s_tg_q = before;
        ESP_LOGE(TAG, "telegram: could not store the message - dropped");
        nvs_close(h);
        return;
    }
    if (p.freed >= 0) {
        tg_slot_key(p.freed, slot, sizeof(slot));
        nvs_erase_key(h, slot);
        nvs_commit(h);
    }
    nvs_close(h);
    if (p.replaced) ESP_LOGI(TAG, "telegram: the waiting \"%s\" message was replaced by the newer one", key);
    else if (p.dropped == 1) ESP_LOGW(TAG, "telegram: %d messages waiting - the oldest \"%s\" message was dropped", TG_CAP, p.dropped_key);
    else if (p.dropped == 2) ESP_LOGW(TAG, "telegram: %d messages waiting - the oldest was dropped", TG_CAP);
}

/* The oldest message into buf (TELEGRAM_NOTIFY_MAX_TEXT + 1 bytes): 1 read,
 * 0 its text is unreadable (skip it), -1 storage cannot be opened (wait). */
static int tg_store_peek(char *buf)
{
    if (s_tg_q.n == 0) return 0;
    nvs_handle_t h;
    if (nvs_open(TG_QUEUE_NS, NVS_READONLY, &h) != ESP_OK) return -1;
    char key[16];
    tg_slot_key(s_tg_q.slot[0], key, sizeof(key));
    size_t n = TELEGRAM_NOTIFY_MAX_TEXT + 1;
    esp_err_t err = nvs_get_str(h, key, buf, &n);
    nvs_close(h);
    return err == ESP_OK ? 1 : 0;
}

static void tg_store_pop(void)
{
    if (s_tg_q.n == 0) return;
    nvs_handle_t h;
    if (nvs_open(TG_QUEUE_NS, NVS_READWRITE, &h) != ESP_OK) {
        /* Gone from this run's queue; flash catches up on the next write. */
        tg_idx_remove(&s_tg_q, 0);
        return;
    }
    char key[16];
    tg_slot_key(s_tg_q.slot[0], key, sizeof(key));
    tg_idx_remove(&s_tg_q, 0);
    tg_store_save_index(h);
    nvs_erase_key(h, key);
    nvs_commit(h);
    nvs_close(h);
}

/* ── HTTPS ───────────────────────────────────────────────────────────── */

static size_t tg_urlencode(const char *in, char *out, size_t cap)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p != 0; p++) {
        bool plain = (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
                     *p == '-' || *p == '_' || *p == '.' || *p == '~';
        if (plain) {
            if (o + 1 >= cap) break;
            out[o++] = (char)*p;
        } else {
            if (o + 3 >= cap) break;
            out[o++] = '%';
            out[o++] = hex[*p >> 4];
            out[o++] = hex[*p & 15];
        }
    }
    out[o] = 0;
    return o;
}

/* One request on a fresh connection. The body is read into buf (cap bytes,
 * NUL-terminated); a body that does not fit sets truncated. */
static esp_err_t tg_http(const char *method, const char *query, const char *body,
                         char *buf, size_t cap, tg_resp_t *r, int timeout_s)
{
    memset(r, 0, sizeof(*r));
    buf[0] = 0;
    char token[sizeof(s_tg_token)];
    xSemaphoreTake(s_tg_lock, portMAX_DELAY);
    strlcpy(token, s_tg_token, sizeof(token));
    xSemaphoreGive(s_tg_lock);
    size_t ulen = strlen(TG_URL) + strlen(token) + strlen(method) + (query ? strlen(query) : 0) + 4;
    char *url = (char *)malloc(ulen);
    if (url == NULL) return ESP_ERR_NO_MEM;
    snprintf(url, ulen, "%s%s/%s%s%s", TG_URL, token, method, query ? "?" : "", query ? query : "");

    esp_http_client_config_t c = {};
    c.url = url;
    c.method = body ? HTTP_METHOD_POST : HTTP_METHOD_GET;
    c.timeout_ms = timeout_s * 1000;
    c.crt_bundle_attach = esp_crt_bundle_attach;
    c.buffer_size = 1024;
    c.buffer_size_tx = 1024;
    c.disable_auto_redirect = true;
    esp_http_client_handle_t cl = esp_http_client_init(&c);
    if (cl == NULL) {
        free(url);
        return ESP_ERR_NO_MEM;
    }
    size_t blen = body ? strlen(body) : 0;
    if (body) esp_http_client_set_header(cl, "Content-Type", "application/x-www-form-urlencoded");
    esp_err_t err = esp_http_client_open(cl, (int)blen);
    for (size_t off = 0; err == ESP_OK && off < blen;) {
        int w = esp_http_client_write(cl, body + off, (int)(blen - off));
        if (w <= 0) err = ESP_FAIL;
        else off += (size_t)w;
    }
    if (err == ESP_OK && esp_http_client_fetch_headers(cl) < 0) err = ESP_FAIL;
    if (err == ESP_OK) {
        r->status = esp_http_client_get_status_code(cl);
        while (r->len + 1 < cap) {
            int n = esp_http_client_read(cl, buf + r->len, (int)(cap - 1 - r->len));
            if (n < 0) {
                err = ESP_FAIL;
                break;
            }
            if (n == 0) break;
            r->len += (size_t)n;
        }
        if (err == ESP_OK && r->len + 1 >= cap) {
            char extra;
            if (esp_http_client_read(cl, &extra, 1) > 0) r->truncated = true;
        }
        if (err == ESP_OK && !r->truncated && !esp_http_client_is_complete_data_received(cl)) err = ESP_FAIL;
    }
    buf[r->len] = 0;
    esp_http_client_close(cl);
    esp_http_client_cleanup(cl);
    free(url);
    return err;
}

/* The last outcome, for tg-status. Written by the Telegram task and the
 * console (tg-learn), read by the console. */
static void tg_set_last(const char *text)
{
    xSemaphoreTake(s_tg_lock, portMAX_DELAY);
    strlcpy(s_tg_last, text, sizeof(s_tg_last));
    xSemaphoreGive(s_tg_lock);
    ESP_LOGW(TAG, "telegram: %s", text);
}

static void tg_get_last(char *out, size_t cap)
{
    xSemaphoreTake(s_tg_lock, portMAX_DELAY);
    strlcpy(out, s_tg_last, cap);
    xSemaphoreGive(s_tg_lock);
}

/* Telegram's {"ok":false,"description":...} into s_tg_last. */
static void tg_note_error(const char *what, const tg_resp_t *r, const char *buf, int *retry_after)
{
    char desc[64] = {0};
    jparse_ctx_t j;
    if (!r->truncated && json_parse_start(&j, buf, (int)r->len) == 0) {
        json_obj_get_string(&j, "description", desc, sizeof(desc));
        if (retry_after != NULL && json_obj_get_object(&j, "parameters") == 0) {
            json_obj_get_int(&j, "retry_after", retry_after);
            json_obj_leave_object(&j);
        }
        json_parse_end(&j);
    }
    char last[sizeof(s_tg_last)];
    snprintf(last, sizeof(last), "%s: HTTP %d %s", what, r->status, desc);
    tg_set_last(r->status == 401 ? "the bot token was rejected - set it again with tg-token" : last);
}

/* 0 sent, > 0 retry after that many seconds, -1 failed (retry with the
 * backoff), -2 refused for good (drop it). server_fails: answered failures of
 * this same message so far. */
static int tg_send_one(const char *text, int server_fails)
{
    int64_t chat = 0;
    if (!tg_get_chat(&chat)) return -1;
    char chat_s[24];
    tg_i64_str(chat, chat_s, sizeof(chat_s));
    size_t cap = strlen(text) * 3 + 48;
    char *body = (char *)malloc(cap);
    char *resp = (char *)malloc(1024);
    if (body == NULL || resp == NULL) {
        free(body);
        free(resp);
        return -1;
    }
    int n = snprintf(body, cap, "chat_id=%s&text=", chat_s);
    tg_urlencode(text, body + n, cap - (size_t)n);
    tg_resp_t r;
    esp_err_t err = tg_http("sendMessage", NULL, body, resp, 1024, &r, 20);
    int rc = -1;
    if (err != ESP_OK) {
        char last[sizeof(s_tg_last)];
        snprintf(last, sizeof(last), "could not reach Telegram (%s)", esp_err_to_name(err));
        tg_set_last(last);
    } else {
        tg_verdict_t v = tg_send_verdict(r.status, resp, server_fails);
        if (v == TG_SENT) {
            rc = 0;
            xSemaphoreTake(s_tg_lock, portMAX_DELAY);
            strlcpy(s_tg_last, "last message delivered", sizeof(s_tg_last));
            xSemaphoreGive(s_tg_lock);
        } else {
            int retry_after = 0;
            tg_note_error(v == TG_DROP ? "message refused and dropped" : "send failed", &r, resp, &retry_after);
            if (v == TG_DROP) rc = -2;
            else if (r.status == 429 && retry_after > 0) rc = retry_after;
        }
    }
    free(body);
    free(resp);
    return rc;
}

/* The update id of the first update in a body that did not fit: a reply too
 * long to read is skipped, not read again forever. */
static bool tg_scan_update_id(const char *buf, int64_t *id)
{
    const char *p = strstr(buf, "\"update_id\":");
    if (p == NULL) return false;
    char *end = NULL;
    long long v = strtoll(p + 12, &end, 10);
    if (end == p + 12) return false;
    *id = (int64_t)v;
    return true;
}

static void tg_set_offset(int64_t next)
{
    if (next == s_tg_offset) return;
    s_tg_offset = next;
    tg_save_i64("offset", next);
}

/* The newest update only (offset -1). Out: its id, and the chat and text of
 * its message when it is one. found = false when there is none. */
typedef struct {
    bool found;
    int64_t update_id;
    bool is_message;
    int64_t chat;
    char *text;  /* malloc'd, may be NULL */
} tg_update_t;

static bool tg_parse_update(const char *buf, size_t len, tg_update_t *u)
{
    memset(u, 0, sizeof(*u));
    jparse_ctx_t j;
    if (json_parse_start(&j, buf, (int)len) != 0) return false;
    bool ok = false;
    int n = 0;
    if (json_obj_get_bool(&j, "ok", &ok) != 0 || !ok || json_obj_get_array(&j, "result", &n) != 0) {
        json_parse_end(&j);
        return false;
    }
    if (n > 0 && json_arr_get_object(&j, 0) == 0) {
        u->found = json_obj_get_int64(&j, "update_id", &u->update_id) == 0;
        if (json_obj_get_object(&j, "message") == 0) {
            if (json_obj_get_object(&j, "chat") == 0) {
                u->is_message = json_obj_get_int64(&j, "id", &u->chat) == 0;
                json_obj_leave_object(&j);
            }
            int tl = 0;
            if (json_obj_get_strlen(&j, "text", &tl) == 0 && tl >= 0) {
                u->text = (char *)malloc((size_t)tl + 1);
                if (u->text != NULL && json_obj_get_string(&j, "text", u->text, tl + 1) != 0) {
                    free(u->text);
                    u->text = NULL;
                }
            }
            json_obj_leave_object(&j);
        }
        json_arr_leave_object(&j);
    }
    json_obj_leave_array(&j);
    json_parse_end(&j);
    return true;
}

static void tg_learn(void)
{
    char *buf = (char *)malloc(TG_RESP_MAX);
    if (buf == NULL) return;
    tg_resp_t r;
    esp_err_t err = tg_http("getUpdates", "offset=-1&limit=1&timeout=0", NULL, buf, TG_RESP_MAX, &r, 20);
    tg_update_t u;
    if (err != ESP_OK) {
        printf("tg-learn: could not reach Telegram (%s) - check the network and try again\n", esp_err_to_name(err));
    } else if (r.status != 200) {
        tg_note_error("tg-learn", &r, buf, NULL);
        char last[sizeof(s_tg_last)];
        tg_get_last(last, sizeof(last));
        printf("tg-learn: %s\n", last);
    } else if (r.truncated) {
        printf("tg-learn: the newest message is too long to read here - send the bot a short message (hi) and run tg-learn again\n");
    } else if (!tg_parse_update(buf, r.len, &u)) {
        printf("tg-learn: Telegram's answer could not be read - try again\n");
    } else {
        if (!u.found) {
            printf("tg-learn: no message found - send your bot a message from your phone, then run tg-learn again\n");
        } else if (!u.is_message) {
            printf("tg-learn: the newest update is not a message - send your bot a text message, then run tg-learn again\n");
        } else {
            tg_set_chat(u.chat);
            /* That message (a /start, a hello) is setup, never a reply. */
            tg_set_offset(u.update_id + 1);
            s_tg_backlog_skipped = true;
            char chat_s[24];
            tg_i64_str(u.chat, chat_s, sizeof(chat_s));
            printf("tg-learn: chat %s saved - messages now go there\n", chat_s);
        }
        free(u.text);
    }
    free(buf);
}

/* Before the first poll of a boot: everything already queued is older than
 * this boot and is never a reply. */
static bool tg_skip_backlog(void)
{
    char *buf = (char *)malloc(TG_RESP_MAX);
    if (buf == NULL) return false;
    tg_resp_t r;
    bool done = false;
    if (tg_http("getUpdates", "offset=-1&limit=1&timeout=0", NULL, buf, TG_RESP_MAX, &r, 20) == ESP_OK && r.status == 200) {
        int64_t id = 0;
        tg_update_t u;
        if (r.truncated) {
            if (tg_scan_update_id(buf, &id)) tg_set_offset(id + 1);
            done = true;
        } else if (tg_parse_update(buf, r.len, &u)) {
            if (u.found) tg_set_offset(u.update_id + 1);
            free(u.text);
            done = true;
        }
    } else if (r.status != 0) {
        tg_note_error("reading replies", &r, buf, NULL);
    }
    free(buf);
    return done;
}

static bool tg_poll(void)
{
    char off[24];
    tg_i64_str(s_tg_offset, off, sizeof(off));
    char q[112];
    snprintf(q, sizeof(q), "offset=%s&limit=1&timeout=%d&allowed_updates=%%5B%%22message%%22%%5D", off, {{prefix}}_TG_POLL_S);
    char *buf = (char *)malloc(TG_RESP_MAX);
    if (buf == NULL) return false;
    tg_resp_t r;
    esp_err_t err = tg_http("getUpdates", q, NULL, buf, TG_RESP_MAX, &r, {{prefix}}_TG_POLL_S + 15);
    if (err != ESP_OK || r.status != 200) {
        if (r.status == 409) tg_set_last("a webhook is set for this bot - delete it to read replies");
        else if (r.status != 0) tg_note_error("reading replies", &r, buf, NULL);
        free(buf);
        return false;
    }
    int64_t id = 0;
    tg_update_t u;
    bool ok = true;
    if (r.truncated) {
        if (tg_scan_update_id(buf, &id)) {
            tg_set_offset(id + 1);
            ESP_LOGW(TAG, "telegram: a reply too long to read was skipped");
        } else {
            ok = false;
        }
    } else if (tg_parse_update(buf, r.len, &u)) {
        if (u.found) {
            tg_set_offset(u.update_id + 1);
            int64_t chat = 0;
            if (u.is_message && u.text != NULL && tg_get_chat(&chat) && u.chat == chat && s_tg_reply_cb != NULL) {
                s_tg_reply_cb(u.text, s_tg_reply_ctx);
            }
        }
        free(u.text);
    } else {
        ok = false;
    }
    free(buf);
    return ok;
}

/* ── the task ─────────────────────────────────────────────────────────── */

static bool tg_net_up(void)
{
    if (s_tg_net) return true;
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip = {};
    return sta != NULL && esp_netif_get_ip_info(sta, &ip) == ESP_OK && ip.ip.addr != 0;
}

static void tg_net_cb(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)data;
    s_tg_net = (id == IP_EVENT_STA_GOT_IP);
    if (s_tg_net && s_tg_task != NULL) xTaskNotifyGive(s_tg_task);
}

static uint32_t tg_next_backoff(uint32_t cur)
{
    uint32_t b = cur == 0 ? 5 : cur * 2;
    return b > TG_MAX_BACKOFF_S ? TG_MAX_BACKOFF_S : b;
}

static void tg_task(void *arg)
{
    (void)arg;
    char *text = (char *)malloc(TELEGRAM_NOTIFY_MAX_TEXT + 1);
    uint32_t send_backoff = 0, poll_backoff = 0;
    int head_fails = 0;  /* failed sends of the message at the head */
    int head_slot = -1;  /* which message that is */
    int64_t next_send_us = 0, next_poll_us = 0;
    for (;;) {
        char *m = NULL;
        while (xQueueReceive(s_tg_in, &m, 0) == pdTRUE) {
            tg_store_push(m, m + TG_KEY_BYTES);
            free(m);
        }
        if (!tg_net_up()) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
            continue;
        }
        if (!tg_have_token()) {
            if (s_tg_learn_req) {
                s_tg_learn_req = false;
                printf("tg-learn: set the bot token first: tg-token <token>\n");
            }
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }
        if (s_tg_learn_req) {
            s_tg_learn_req = false;
            tg_learn();
            next_send_us = 0;
            continue;
        }
        int64_t chat = 0;
        bool have_chat = tg_get_chat(&chat);
        int64_t now = esp_timer_get_time();
        if (have_chat && tg_store_count() > 0 && now >= next_send_us && text != NULL) {
            int got = tg_store_peek(text);
            if (got < 0) {
                /* Storage will not open: wait, rather than spin on it. */
                ESP_LOGW(TAG, "telegram: message storage unavailable, next try in 5 s");
                next_send_us = esp_timer_get_time() + 5000000LL;
                continue;
            }
            if (got == 0) {
                tg_store_pop();  /* unreadable slot: skip it */
                head_fails = 0;
                continue;
            }
            if (s_tg_q.slot[0] != head_slot) {
                head_slot = s_tg_q.slot[0];
                head_fails = 0;
            }
            int rc = tg_send_one(text, head_fails);
            if (rc == 0 || rc == -2) {
                tg_store_pop();
                head_fails = 0;
                send_backoff = 0;
                next_send_us = 0;
            } else {
                head_fails++;
                send_backoff = rc > 0 ? (uint32_t)rc : tg_next_backoff(send_backoff);
                next_send_us = esp_timer_get_time() + (int64_t)send_backoff * 1000000LL;
                ESP_LOGW(TAG, "telegram: %u message(s) waiting, next try in %u s",
                         (unsigned)tg_store_count(), (unsigned)send_backoff);
            }
            continue;
        }
        if (have_chat && s_tg_reply_cb != NULL && now >= next_poll_us) {
            bool ok = s_tg_backlog_skipped ? tg_poll() : (s_tg_backlog_skipped = tg_skip_backlog());
            if (ok) {
                poll_backoff = 0;
                next_poll_us = 0;
            } else {
                poll_backoff = tg_next_backoff(poll_backoff);
                next_poll_us = esp_timer_get_time() + (int64_t)poll_backoff * 1000000LL;
            }
            continue;
        }
        /* Nothing due: sleep until the next retry or a wake-up. */
        int64_t due = 0;
        if (have_chat && tg_store_count() > 0) due = next_send_us;
        if (have_chat && s_tg_reply_cb != NULL && (due == 0 || next_poll_us < due)) due = next_poll_us;
        TickType_t wait = portMAX_DELAY;
        if (due > now) wait = pdMS_TO_TICKS((uint32_t)((due - now) / 1000) + 1);
        ulTaskNotifyTake(pdTRUE, wait);
    }
}

/* ── public API (declared in app_config.h) ────────────────────────────── */

/* Hands key + text to the task; never touches flash or the network. */
static esp_err_t tg_enqueue(const char *key, const char *text)
{
    if (text == NULL || text[0] == 0) return ESP_ERR_INVALID_ARG;
    if (s_tg_in == NULL || s_tg_task == NULL) return ESP_ERR_INVALID_STATE;
    size_t n = strlen(text);
    if (n > TELEGRAM_NOTIFY_MAX_TEXT) {
        n = TELEGRAM_NOTIFY_MAX_TEXT;
        while (n > 0 && ((unsigned char)text[n] & 0xC0) == 0x80) n--;  /* whole UTF-8 characters */
    }
    char *m = (char *)calloc(1, TG_KEY_BYTES + n + 1);
    if (m == NULL) return ESP_ERR_NO_MEM;
    memcpy(m, key, strlen(key));
    memcpy(m + TG_KEY_BYTES, text, n);
    if (xQueueSend(s_tg_in, &m, 0) != pdTRUE) {
        free(m);
        ESP_LOGW(TAG, "telegram: too many messages at once - one dropped");
        return ESP_ERR_NO_MEM;
    }
    xTaskNotifyGive(s_tg_task);
    return ESP_OK;
}

esp_err_t telegram_notify_send(const char *text)
{
    return tg_enqueue("", text);
}

esp_err_t telegram_notify_send_keyed(const char *key, const char *text)
{
    if (key == NULL || key[0] == 0 || strlen(key) > TELEGRAM_NOTIFY_MAX_KEY) return ESP_ERR_INVALID_ARG;
    return tg_enqueue(key, text);
}

esp_err_t telegram_notify_on_reply(telegram_notify_reply_cb_t cb, void *ctx)
{
    s_tg_reply_ctx = ctx;
    s_tg_reply_cb = cb;
    if (s_tg_task != NULL) xTaskNotifyGive(s_tg_task);
    return ESP_OK;
}

size_t telegram_notify_pending(void)
{
    return (size_t)tg_store_count() + (s_tg_in ? (size_t)uxQueueMessagesWaiting(s_tg_in) : 0);
}

{{#if cfg.trigger_param}}
static void tg_trigger_cb(app_driver_param_id_t param_id, app_driver_param_val_t val,
                          app_driver_handle_t source, void *ctx)
{
    (void)source; (void)ctx;
    if (param_id != {{cfg.trigger_param}} || !val.b) return;
    telegram_notify_send({{prefix}}_TG_TRIGGER);
}
{{/if}}

/* ── console (queues work for the task, never sends from here) ────────── */

static int tg_token_cmd(int argc, char **argv)
{
    if (argc != 2 || strchr(argv[1], ':') == NULL || strlen(argv[1]) >= sizeof(s_tg_token)) {
        printf("usage: tg-token <token>   (the token @BotFather gave you, like 123456:ABC...)\n");
        return 1;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(TG_NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_str(h, "token", argv[1]);
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
    }
    if (err != ESP_OK) {
        printf("not saved: %s\n", esp_err_to_name(err));
        return 1;
    }
    xSemaphoreTake(s_tg_lock, portMAX_DELAY);
    strlcpy(s_tg_token, argv[1], sizeof(s_tg_token));
    xSemaphoreGive(s_tg_lock);
    xTaskNotifyGive(s_tg_task);
    printf("token saved%s\n", s_tg_have_chat ? "" : " - now send your bot a message and run tg-learn");
    return 0;
}

static int tg_chat_cmd(int argc, char **argv)
{
    int64_t chat = 0;
    if (argc != 2 || !tg_parse_i64(argv[1], &chat)) {
        printf("usage: tg-chat <chat id>   (a number; tg-learn finds it for you)\n");
        return 1;
    }
    tg_set_chat(chat);
    xTaskNotifyGive(s_tg_task);
    printf("chat saved\n");
    return 0;
}

static int tg_learn_cmd(int argc, char **argv)
{
    (void)argc; (void)argv;
    s_tg_learn_req = true;
    xTaskNotifyGive(s_tg_task);
    printf("asking Telegram for the newest message sent to the bot - the answer is printed here in a moment\n");
    return 0;
}

static int tg_test_cmd(int argc, char **argv)
{
    char line[200] = {0};
    for (int i = 1; i < argc; i++) {
        if (i > 1) strlcat(line, " ", sizeof(line));
        strlcat(line, argv[i], sizeof(line));
    }
    if (line[0] == 0) snprintf(line, sizeof(line), "Test message from %s", APP_PRODUCT_NAME);
    esp_err_t err = telegram_notify_send(line);
    if (err != ESP_OK) {
        printf("not queued: %s\n", esp_err_to_name(err));
        return 1;
    }
    if (!tg_have_token()) printf("queued - it is sent once a bot token is set (tg-token)\n");
    else if (!s_tg_have_chat) printf("queued - it is sent once the chat is known (tg-learn)\n");
    else printf("queued - watch your phone\n");
    return 0;
}

static int tg_status_cmd(int argc, char **argv)
{
    (void)argc; (void)argv;
    int64_t chat = 0;
    bool have_chat = tg_get_chat(&chat);
    char chat_s[24] = "not set - run tg-learn";
    if (have_chat) tg_i64_str(chat, chat_s, sizeof(chat_s));
    printf("bot token: %s\n", tg_have_token() ? "set" : "not set - run tg-token");
    printf("chat     : %s\n", chat_s);
    printf("network  : %s\n", tg_net_up() ? "up" : "down");
    printf("waiting  : %u message(s)\n", (unsigned)telegram_notify_pending());
    printf("replies  : %s\n", s_tg_reply_cb ? "listening" : "not listening");
    char last[sizeof(s_tg_last)];
    tg_get_last(last, sizeof(last));
    printf("last     : %s\n", last);
    return 0;
}
