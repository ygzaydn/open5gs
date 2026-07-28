/*
 * Copyright (C) 2019-2026 by Sukchan Lee <acetcom@gmail.com>
 *
 * This file is part of Open5GS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ogs-hsm.h"

#define TLV_HEADER_LEN 6 /* tag(2 BE) + length(4 BE) */

static void put_u16be(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v);
}

static void put_u32be(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v);
}

static uint16_t get_u16be(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t get_u32be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* Appends one TLV to *pos (bounded by buf_cap), advancing *pos.
 * Returns false if it would not fit. */
static bool append_tlv(uint8_t *buf, size_t buf_cap, size_t *pos,
        uint16_t tag, const uint8_t *value, size_t value_len)
{
    if (*pos + TLV_HEADER_LEN + value_len > buf_cap)
        return false;

    put_u16be(buf + *pos, tag);
    put_u32be(buf + *pos + 2, (uint32_t)value_len);
    if (value_len)
        memcpy(buf + *pos + TLV_HEADER_LEN, value, value_len);

    *pos += TLV_HEADER_LEN + value_len;
    return true;
}

static int build_header_and_tlvs(
    uint8_t *buf, size_t buf_cap, uint8_t op,
    bool (*fill)(uint8_t *buf, size_t buf_cap, size_t *pos, const void *ctx),
    const void *ctx)
{
    size_t pos = OGS_HSM_WIRE_HEADER_LEN;

    if (buf_cap < OGS_HSM_WIRE_HEADER_LEN)
        return -1;

    if (!fill(buf, buf_cap, &pos, ctx))
        return -1;

    if (pos > UINT32_MAX)
        return -1;

    memcpy(buf, OGS_HSM_WIRE_MAGIC, 4);
    buf[4] = OGS_HSM_WIRE_VERSION;
    buf[5] = op;
    buf[6] = 0;
    buf[7] = 0;
    put_u32be(buf + 8, (uint32_t)pos);

    return (int)pos;
}

typedef struct av_request_ctx_s {
    const char *supi;
    const uint8_t *wrapped_k;
    size_t wrapped_k_len;
    const uint8_t *wrapped_opc;
    size_t wrapped_opc_len;
    const uint8_t *sqn;
    const uint8_t *amf;
    const char *serving_network_name;
} av_request_ctx_t;

static bool fill_av_request(uint8_t *buf, size_t buf_cap, size_t *pos,
        const void *vctx)
{
    const av_request_ctx_t *ctx = vctx;

    if (!append_tlv(buf, buf_cap, pos, OGS_HSM_TAG_SUPI,
            (const uint8_t *)ctx->supi, strlen(ctx->supi)))
        return false;
    if (!append_tlv(buf, buf_cap, pos, OGS_HSM_TAG_WRAPPED_K,
            ctx->wrapped_k, ctx->wrapped_k_len))
        return false;
    if (!append_tlv(buf, buf_cap, pos, OGS_HSM_TAG_WRAPPED_OPC,
            ctx->wrapped_opc, ctx->wrapped_opc_len))
        return false;
    if (!append_tlv(buf, buf_cap, pos, OGS_HSM_TAG_SQN,
            ctx->sqn, OGS_HSM_SQN_LEN))
        return false;
    if (!append_tlv(buf, buf_cap, pos, OGS_HSM_TAG_AMF,
            ctx->amf, OGS_HSM_AMF_LEN))
        return false;
    if (ctx->serving_network_name &&
        !append_tlv(buf, buf_cap, pos, OGS_HSM_TAG_SNN,
            (const uint8_t *)ctx->serving_network_name,
            strlen(ctx->serving_network_name)))
        return false;

    return true;
}

int hsm_wire_build_5g_he_av_request(
    uint8_t *buf, size_t buf_cap,
    const char *supi,
    const uint8_t *wrapped_k, size_t wrapped_k_len,
    const uint8_t *wrapped_opc, size_t wrapped_opc_len,
    const uint8_t sqn[OGS_HSM_SQN_LEN],
    const uint8_t amf[OGS_HSM_AMF_LEN],
    const char *serving_network_name)
{
    av_request_ctx_t ctx = {
        .supi = supi,
        .wrapped_k = wrapped_k, .wrapped_k_len = wrapped_k_len,
        .wrapped_opc = wrapped_opc, .wrapped_opc_len = wrapped_opc_len,
        .sqn = sqn, .amf = amf,
        .serving_network_name = serving_network_name,
    };
    return build_header_and_tlvs(
        buf, buf_cap, OGS_HSM_OP_5G_HE_AV, fill_av_request, &ctx);
}

typedef struct resync_request_ctx_s {
    const char *supi;
    const uint8_t *wrapped_k;
    size_t wrapped_k_len;
    const uint8_t *wrapped_opc;
    size_t wrapped_opc_len;
    const uint8_t *rand;
    const uint8_t *auts;
} resync_request_ctx_t;

static bool fill_resync_request(uint8_t *buf, size_t buf_cap, size_t *pos,
        const void *vctx)
{
    const resync_request_ctx_t *ctx = vctx;

    if (!append_tlv(buf, buf_cap, pos, OGS_HSM_TAG_SUPI,
            (const uint8_t *)ctx->supi, strlen(ctx->supi)))
        return false;
    if (!append_tlv(buf, buf_cap, pos, OGS_HSM_TAG_WRAPPED_K,
            ctx->wrapped_k, ctx->wrapped_k_len))
        return false;
    if (!append_tlv(buf, buf_cap, pos, OGS_HSM_TAG_WRAPPED_OPC,
            ctx->wrapped_opc, ctx->wrapped_opc_len))
        return false;
    if (!append_tlv(buf, buf_cap, pos, OGS_HSM_TAG_RAND,
            ctx->rand, OGS_HSM_RAND_LEN))
        return false;
    if (!append_tlv(buf, buf_cap, pos, OGS_HSM_TAG_AUTS,
            ctx->auts, OGS_HSM_AUTS_LEN))
        return false;

    return true;
}

int hsm_wire_build_resync_request(
    uint8_t *buf, size_t buf_cap,
    const char *supi,
    const uint8_t *wrapped_k, size_t wrapped_k_len,
    const uint8_t *wrapped_opc, size_t wrapped_opc_len,
    const uint8_t rand[OGS_HSM_RAND_LEN],
    const uint8_t auts[OGS_HSM_AUTS_LEN])
{
    resync_request_ctx_t ctx = {
        .supi = supi,
        .wrapped_k = wrapped_k, .wrapped_k_len = wrapped_k_len,
        .wrapped_opc = wrapped_opc, .wrapped_opc_len = wrapped_opc_len,
        .rand = rand, .auts = auts,
    };
    return build_header_and_tlvs(
        buf, buf_cap, OGS_HSM_OP_RESYNC, fill_resync_request, &ctx);
}

static bool copy_field_if_len_matches(
    bool *present, uint8_t *dst, size_t expected_len,
    const uint8_t *value, uint32_t value_len)
{
    if (value_len != expected_len)
        return false;
    memcpy(dst, value, expected_len);
    *present = true;
    return true;
}

bool hsm_wire_parse_response(
    const uint8_t *buf, size_t len, hsm_wire_fields_t *fields)
{
    uint32_t total_len;
    size_t pos;

    if (len < OGS_HSM_WIRE_HEADER_LEN)
        return false;
    if (memcmp(buf, OGS_HSM_WIRE_MAGIC, 4) != 0)
        return false;
    if (buf[4] != OGS_HSM_WIRE_VERSION)
        return false;

    total_len = get_u32be(buf + 8);
    if (total_len != len)
        return false;

    memset(fields, 0, sizeof(*fields));

    pos = OGS_HSM_WIRE_HEADER_LEN;
    while (pos < len) {
        uint16_t tag;
        uint32_t value_len;
        const uint8_t *value;

        if (pos + TLV_HEADER_LEN > len)
            return false;

        tag = get_u16be(buf + pos);
        value_len = get_u32be(buf + pos + 2);
        value = buf + pos + TLV_HEADER_LEN;

        if (pos + TLV_HEADER_LEN + value_len > len)
            return false;

        switch (tag) {
        case OGS_HSM_TAG_OUT_RAND:
            if (!copy_field_if_len_matches(&fields->rand_present,
                    fields->rand, OGS_HSM_RAND_LEN, value, value_len))
                return false;
            break;
        case OGS_HSM_TAG_OUT_AUTN:
            if (!copy_field_if_len_matches(&fields->autn_present,
                    fields->autn, OGS_HSM_AUTN_LEN, value, value_len))
                return false;
            break;
        case OGS_HSM_TAG_OUT_XRES_STAR:
            if (!copy_field_if_len_matches(&fields->xres_star_present,
                    fields->xres_star, OGS_HSM_XRES_STAR_LEN,
                    value, value_len))
                return false;
            break;
        case OGS_HSM_TAG_OUT_KAUSF:
            if (!copy_field_if_len_matches(&fields->kausf_present,
                    fields->kausf, OGS_HSM_KAUSF_LEN, value, value_len))
                return false;
            break;
        case OGS_HSM_TAG_OUT_SQN_MS:
            if (!copy_field_if_len_matches(&fields->sqn_ms_present,
                    fields->sqn_ms, OGS_HSM_SQN_MS_LEN, value, value_len))
                return false;
            break;
        default:
            /* Unknown tag: ignore, forward-compatible. */
            break;
        }

        pos += TLV_HEADER_LEN + value_len;
    }

    return pos == len;
}
