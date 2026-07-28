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

#if !defined(OGS_HSM_INSIDE) && !defined(OGS_HSM_COMPILATION)
#error "This header cannot be included directly."
#endif

#ifndef HSM_WIRE_H
#define HSM_WIRE_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------
 * S5GM wire protocol constants, ported 1:1 from SoftHSMv2's
 * src/lib/milenage/softhsm_milenage.h. Do not invent new values here;
 * if the daemon's protocol changes, port the change from that file.
 * ------------------------------------------------------------------- */
#define OGS_HSM_WIRE_MAGIC "S5GM" /* 4 bytes, no NUL terminator on the wire */
#define OGS_HSM_WIRE_VERSION 0x01
#define OGS_HSM_WIRE_HEADER_LEN 12 /* magic(4) version(1) op(1) reserved(2) total_len(4 BE) */

#define OGS_HSM_OP_5G_HE_AV 0x02
#define OGS_HSM_OP_RESYNC   0x03

#define OGS_HSM_TAG_SUPI          0x0001
#define OGS_HSM_TAG_WRAPPED_K     0x0002
#define OGS_HSM_TAG_WRAPPED_OPC   0x0003
#define OGS_HSM_TAG_SQN           0x0004
#define OGS_HSM_TAG_AMF           0x0005
#define OGS_HSM_TAG_SNN           0x0006
#define OGS_HSM_TAG_RAND          0x0007
#define OGS_HSM_TAG_AUTS          0x0008

#define OGS_HSM_TAG_OUT_RAND        0x0101
#define OGS_HSM_TAG_OUT_AUTN        0x0102
#define OGS_HSM_TAG_OUT_XRES_STAR   0x0103
#define OGS_HSM_TAG_OUT_KAUSF       0x0104
#define OGS_HSM_TAG_OUT_SQN_MS      0x0105

#define OGS_HSM_SQN_LEN       6
#define OGS_HSM_AMF_LEN       2
#define OGS_HSM_RAND_LEN      16
#define OGS_HSM_AUTN_LEN      16
#define OGS_HSM_AUTS_LEN      14
#define OGS_HSM_XRES_STAR_LEN 16
#define OGS_HSM_KAUSF_LEN     32
#define OGS_HSM_SQN_MS_LEN    6

/* Parsed TLV fields from a decoded S5GM response message. Only the
 * OUT_* tags are populated by hsm_wire_parse_response(); a field's
 * *_present flag is false if the daemon omitted it. */
typedef struct hsm_wire_fields_s {
    bool rand_present;
    uint8_t rand[OGS_HSM_RAND_LEN];

    bool autn_present;
    uint8_t autn[OGS_HSM_AUTN_LEN];

    bool xres_star_present;
    uint8_t xres_star[OGS_HSM_XRES_STAR_LEN];

    bool kausf_present;
    uint8_t kausf[OGS_HSM_KAUSF_LEN];

    bool sqn_ms_present;
    uint8_t sqn_ms[OGS_HSM_SQN_MS_LEN];
} hsm_wire_fields_t;

/* Builds an S5GM request message for the 5G-HE-AV operation into
 * `buf` (capacity `buf_cap`). Returns the encoded length, or -1 if it
 * does not fit in buf_cap. */
int hsm_wire_build_5g_he_av_request(
    uint8_t *buf, size_t buf_cap,
    const char *supi,
    const uint8_t *wrapped_k, size_t wrapped_k_len,
    const uint8_t *wrapped_opc, size_t wrapped_opc_len,
    const uint8_t sqn[OGS_HSM_SQN_LEN],
    const uint8_t amf[OGS_HSM_AMF_LEN],
    const char *serving_network_name);

/* Builds an S5GM request message for the resync operation into `buf`
 * (capacity `buf_cap`). Returns the encoded length, or -1 if it does
 * not fit in buf_cap. */
int hsm_wire_build_resync_request(
    uint8_t *buf, size_t buf_cap,
    const char *supi,
    const uint8_t *wrapped_k, size_t wrapped_k_len,
    const uint8_t *wrapped_opc, size_t wrapped_opc_len,
    const uint8_t rand[OGS_HSM_RAND_LEN],
    const uint8_t auts[OGS_HSM_AUTS_LEN]);

/* Parses and validates an S5GM response message (magic, version,
 * total_length must exactly match `len`, well-formed TLVs, no
 * truncation, no field with the wrong length). Returns true and
 * fills `fields` on success; returns false on any malformed input
 * without touching `fields`. */
bool hsm_wire_parse_response(
    const uint8_t *buf, size_t len, hsm_wire_fields_t *fields);

#ifdef __cplusplus
}
#endif

#endif /* HSM_WIRE_H */
