/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Char-level BPE encode/decode for the story model (no tokenizer library).
 * Greedy merge-by-rank — bit-identical to the trained HF char-BPE (verified on
 * host by train/bpe_host_test.c). Encode is only used on the short prompt;
 * generation emits ids directly, decoded one token at a time. */
#ifndef BPE_H
#define BPE_H
#include <string.h>
#include "bpe_vocab.h"

static int bpe_find_rank(int a, int b)
{
    for (int m = 0; m < BPE_NMERGE; m++)
        if (BPE_MERGE[m][0] == a && BPE_MERGE[m][1] == b) {
            return m;
        }
    return -1;
}

/* text -> token ids (greedy lowest-rank merge). Returns count. */
static int bpe_encode(const char *s, int *ids, int maxn)
{
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p && n < maxn; p++) {
        int id = BPE_CHAR2ID[*p];
        ids[n++] = (id >= 0) ? id : BPE_UNK;
    }
    for (;;) {
        int best = 0x7fffffff, bp = -1, bres = -1;
        for (int i = 0; i < n - 1; i++) {
            int m = bpe_find_rank(ids[i], ids[i + 1]);
            if (m >= 0 && m < best) {
                best = m;
                bp = i;
                bres = BPE_MERGE[m][2];
            }
        }
        if (bp < 0) {
            break;
        }
        ids[bp] = bres;
        for (int j = bp + 1; j < n - 1; j++) {
            ids[j] = ids[j + 1];
        }
        n--;
    }
    return n;
}

/* append one token's text to out; skips specials. Returns new length. */
static int bpe_decode_append(int id, char *out, int oi, int outsz)
{
    if (id == BPE_EOS || id == BPE_UNK || id < 0 || id >= BPE_NVOCAB) {
        return oi;
    }
    const char *t = BPE_VOCAB[id];
    for (int k = 0; t[k] && oi < outsz - 1; k++) {
        out[oi++] = t[k];
    }
    return oi;
}

#endif
