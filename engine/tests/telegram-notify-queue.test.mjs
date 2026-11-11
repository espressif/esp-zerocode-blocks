// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// behaviors/telegram_notify keeps its waiting messages in NVS behind an index
// (order + keys). The index logic sits between the tg-queue markers in its
// logic_statics slot and touches no NVS, so it is compiled here, on the host,
// as the C++ it is on the device, and driven by a small program.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { execFileSync, spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const ROOT = dirname(dirname(dirname(fileURLToPath(import.meta.url))))
const SLOT = join(ROOT, 'code_blocks/behaviors/telegram_notify/slots/logic_statics.c')
const CXX = process.env.CXX || 'c++'
const haveCxx = spawnSync(CXX, ['--version']).status === 0

const DRIVER = String.raw`
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define TELEGRAM_NOTIFY_MAX_KEY 15
#include "queue.inc"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

/* The order of the waiting messages as their keys, "-" for un-keyed. */
static const char *order(const tg_index_t *q)
{
    static char out[TG_QCAP * (TELEGRAM_NOTIFY_MAX_KEY + 2)];
    out[0] = 0;
    for (int i = 0; i < q->n; i++) {
        if (i) strcat(out, ",");
        strcat(out, q->key[i][0] ? q->key[i] : "-");
    }
    return out;
}

static bool slots_distinct(const tg_index_t *q, int cap)
{
    for (int i = 0; i < q->n; i++) {
        if (q->slot[i] > cap) return false;
        for (int j = i + 1; j < q->n; j++) if (q->slot[i] == q->slot[j]) return false;
    }
    return true;
}

int main(void)
{
    tg_index_t q;

    /* A reminder sent again replaces its waiting copy and moves to the back. */
    memset(&q, 0, sizeof(q));
    tg_idx_push(&q, 4, "");
    tg_push_t a = tg_idx_push(&q, 4, "remind");
    tg_idx_push(&q, 4, "");
    tg_push_t b = tg_idx_push(&q, 4, "remind");
    CHECK(strcmp(order(&q), "-,-,remind") == 0);
    CHECK(b.replaced && b.dropped == 0);
    CHECK(b.freed == a.slot && b.slot != a.slot);
    CHECK(slots_distinct(&q, 4));

    /* Full: the oldest KEYED message goes, never the one-off ahead of it. */
    memset(&q, 0, sizeof(q));
    tg_idx_push(&q, 3, "");
    tg_idx_push(&q, 3, "r1");
    tg_idx_push(&q, 3, "r2");
    tg_push_t c = tg_idx_push(&q, 3, "");
    CHECK(strcmp(order(&q), "-,r2,-") == 0);
    CHECK(c.dropped == 1 && strcmp(c.dropped_key, "r1") == 0);
    CHECK(c.freed >= 0 && c.slot != c.freed);
    CHECK(slots_distinct(&q, 3));

    /* A reminder every 10 minutes for hours while offline: the one-off survives. */
    memset(&q, 0, sizeof(q));
    tg_idx_push(&q, 8, "");
    for (int i = 0; i < 100; i++) tg_idx_push(&q, 8, "remind");
    CHECK(strcmp(order(&q), "-,remind") == 0);
    CHECK(slots_distinct(&q, 8));

    /* Everything un-keyed: the oldest goes, as before. */
    memset(&q, 0, sizeof(q));
    for (int i = 0; i < 3; i++) tg_idx_push(&q, 3, "");
    tg_push_t d = tg_idx_push(&q, 3, "");
    CHECK(q.n == 3 && d.dropped == 2 && d.freed >= 0);
    CHECK(slots_distinct(&q, 3));

    /* Full with a keyed message whose key is already waiting: replaced, nothing dropped. */
    memset(&q, 0, sizeof(q));
    tg_idx_push(&q, 2, "");
    tg_idx_push(&q, 2, "k");
    tg_push_t e = tg_idx_push(&q, 2, "k");
    CHECK(strcmp(order(&q), "-,k") == 0 && e.replaced && e.dropped == 0);
    CHECK(slots_distinct(&q, 2));

    /* A shrunken max_pending trims keyed first. */
    memset(&q, 0, sizeof(q));
    tg_idx_push(&q, 5, "a");
    tg_idx_push(&q, 5, "");
    tg_idx_push(&q, 5, "b");
    tg_idx_push(&q, 5, "");
    CHECK(tg_idx_trim(&q, 2) == 2);
    CHECK(strcmp(order(&q), "-,-") == 0);

    /* An index read back from flash is checked. */
    memset(&q, 0, sizeof(q));
    q.n = TG_QCAP + 1;
    CHECK(!tg_idx_valid(&q));
    memset(&q, 0, sizeof(q));
    q.n = 1;
    q.slot[0] = TG_QCAP + 1;
    CHECK(!tg_idx_valid(&q));
    CHECK(tg_idx_cap(0) == 1 && tg_idx_cap(99) == TG_QCAP && tg_idx_cap(8) == 8);

    /* Never more than cap, slots always distinct, over a long random run. */
    unsigned seed = 1;
    memset(&q, 0, sizeof(q));
    for (int i = 0; i < 5000; i++) {
        seed = seed * 1103515245u + 12345u;
        const char *keys[] = { "", "", "x", "y", "z" };
        const char *k = keys[(seed >> 16) % 5];
        if ((seed >> 8) % 4 == 0 && q.n > 0) tg_idx_remove(&q, 0);
        else tg_idx_push(&q, 6, k);
        if (q.n > 6 || !slots_distinct(&q, 6)) { CHECK(false); break; }
        for (int j = 0; j < q.n; j++)
            for (int l = j + 1; l < q.n; l++)
                if (q.key[j][0] && strcmp(q.key[j], q.key[l]) == 0) { CHECK(false); j = q.n; break; }
    }

    /* A send answer: delivered, retried, held for the owner, or dropped so it
       cannot block every message behind it. */
    CHECK(tg_send_verdict(200, "", 0) == TG_SENT);
    CHECK(tg_send_verdict(429, "", 0) == TG_RETRY);
    CHECK(tg_send_verdict(401, "", 50) == TG_WAIT_OWNER);
    CHECK(tg_send_verdict(403, "{\"description\":\"Forbidden: bot was blocked by the user\"}", 50) == TG_WAIT_OWNER);
    CHECK(tg_send_verdict(400, "{\"description\":\"Bad Request: chat not found\"}", 50) == TG_WAIT_OWNER);
    CHECK(tg_send_verdict(400, "{\"description\":\"Bad Request: message is too long\"}", 0) == TG_DROP);
    CHECK(tg_send_verdict(413, "", 0) == TG_DROP);
    CHECK(tg_send_verdict(500, "", 0) == TG_RETRY);
    CHECK(tg_send_verdict(502, "", TG_MAX_SERVER_FAILS - 2) == TG_RETRY);
    CHECK(tg_send_verdict(502, "", TG_MAX_SERVER_FAILS - 1) == TG_DROP);

    if (fails == 0) printf("ok\n");
    return fails == 0 ? 0 : 1;
}
`

test('the keyed queue replaces, drops keyed first and keeps the one-off; a refused message is dropped, never retried forever', { skip: !haveCxx && 'no host C++ compiler' }, (t) => {
  const src = readFileSync(SLOT, 'utf-8')
  const m = src.match(/\/\* tg-queue: begin \*\/\n([\s\S]*?)\/\* tg-queue: end \*\//)
  assert.ok(m, 'tg-queue markers in logic_statics.c')
  assert.doesNotMatch(m[1], /\{\{|nvs_|ESP_LOG/, 'the marked section is pure: no template variables, NVS or logging')
  const dir = mkdtempSync(join(tmpdir(), 'zc-tgq-'))
  t.after(() => rmSync(dir, { recursive: true, force: true }))
  writeFileSync(join(dir, 'queue.inc'), m[1])
  writeFileSync(join(dir, 'main.cpp'), DRIVER)
  execFileSync(CXX, ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', dir, join(dir, 'main.cpp'), '-o', join(dir, 'q')], { stdio: 'pipe' })
  const out = spawnSync(join(dir, 'q'), { encoding: 'utf-8' })
  assert.equal(out.status, 0, out.stdout + out.stderr)
  assert.equal(out.stdout.trim(), 'ok')
})
