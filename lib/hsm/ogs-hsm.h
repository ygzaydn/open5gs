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

/*
 * HSM-backed Milenage/5G-AKA client (Open5GS vendor extension, not
 * part of 3GPP). Speaks the S5GM wire protocol and the TCP daemon
 * framing implemented by SoftHSMv2's softhsm2-milenaged over a plain,
 * unauthenticated TCP connection.
 *
 * SECURITY WARNING: this channel carries subscriber identity (SUPI)
 * and 5G authentication-vector material (RAND, AUTN, XRES-star,
 * KAUSF) in plaintext on the wire (TLS is out of scope for this PoC -- see
 * docs/open5gs-udm-hsm-milenage.md). Do not expose it to any network
 * you do not already trust as much as the HSM host's own process
 * memory.
 *
 * Does NOT implement Milenage, AES, or any 5G KDF math -- all of that
 * runs inside the HSM daemon. This module only builds/parses the wire
 * messages and drives the TCP round trip.
 */

#ifndef OGS_HSM_H
#define OGS_HSM_H

#include "core/ogs-core.h"

#define OGS_HSM_INSIDE

#include "hsm/hsm-wire.h"
#include "hsm/hsm-tcp.h"

#undef OGS_HSM_INSIDE

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------
 * Configuration (from udm.yaml udm.hsm -- see nudm-config parsing).
 * token_label/master_key_label/master_key_id/user_pin are stored for
 * operational/logging parity with the HSM-side configuration and for
 * forward compatibility; the current daemon protocol authenticates
 * nothing over the wire (see above), so user_pin is never sent to the
 * daemon. This is a deliberate, documented consequence of "use the
 * real existing protocol, do not invent a different one."
 * ------------------------------------------------------------------- */
/* Kept in sync with OGS_MAX_WRAPPED_KEY_LEN in lib/crypt/ogs-crypt.h
 * (the AES-KWP-wrapped credential buffer size used by src/udm); this
 * module does not depend on lib/crypt, so the bound is duplicated
 * here rather than shared via a header. */
#define OGS_HSM_MAX_WRAPPED_KEY_LEN 256

typedef struct ogs_hsm_config_s {
    bool enabled;

    char host[OGS_MAX_FILEPATH_LEN];
    uint16_t port;

    char token_label[128];
    char master_key_label[128];
    char master_key_id[8];
    char user_pin[64];

    uint32_t connect_timeout_ms;
    uint32_t io_timeout_ms;
    uint32_t max_frame_size;
} ogs_hsm_config_t;

void ogs_hsm_set_config(const ogs_hsm_config_t *config);
const ogs_hsm_config_t *ogs_hsm_config(void);
bool ogs_hsm_is_enabled(void);

/* ---------------------------------------------------------------------
 * High-level API
 * ------------------------------------------------------------------- */
typedef struct ogs_hsm_5g_av_request_s {
    const char *supi;

    const uint8_t *wrapped_k;
    size_t wrapped_k_len;

    const uint8_t *wrapped_opc;
    size_t wrapped_opc_len;

    uint8_t sqn[OGS_HSM_SQN_LEN];
    uint8_t amf[OGS_HSM_AMF_LEN];

    const char *serving_network_name;
} ogs_hsm_5g_av_request_t;

typedef struct ogs_hsm_5g_av_response_s {
    uint8_t rand[OGS_HSM_RAND_LEN];
    uint8_t autn[OGS_HSM_AUTN_LEN];
    uint8_t xres_star[OGS_HSM_XRES_STAR_LEN];
    uint8_t kausf[OGS_HSM_KAUSF_LEN];
} ogs_hsm_5g_av_response_t;

typedef struct ogs_hsm_resync_request_s {
    const char *supi;

    const uint8_t *wrapped_k;
    size_t wrapped_k_len;

    const uint8_t *wrapped_opc;
    size_t wrapped_opc_len;

    uint8_t rand[OGS_HSM_RAND_LEN];
    uint8_t auts[OGS_HSM_AUTS_LEN];
} ogs_hsm_resync_request_t;

/* Return codes for the high-level API (distinguishes WHERE a failure
 * happened, for logging -- see docs/open5gs-udm-hsm-milenage.md and
 * src/udm request-site logging). Never carries secret material. */
typedef enum {
    OGS_HSM_OK = 0,
    OGS_HSM_ERROR_DISABLED,        /* HSM support globally disabled */
    OGS_HSM_ERROR_INVALID_REQUEST, /* bad input, e.g. oversized wrapped blob */
    OGS_HSM_ERROR_CONNECT,         /* TCP connect failed or timed out */
    OGS_HSM_ERROR_IO,              /* read/write failed or timed out */
    OGS_HSM_ERROR_DAEMON_REJECTED, /* daemon returned status=ERROR */
    OGS_HSM_ERROR_MALFORMED_FRAME, /* daemon framing itself was invalid */
    OGS_HSM_ERROR_INVALID_RESPONSE /* frame OK but fields missing/wrong length */
} ogs_hsm_rv_t;

const char *ogs_hsm_rv_string(ogs_hsm_rv_t rv);

ogs_hsm_rv_t ogs_hsm_generate_5g_he_av(
    const ogs_hsm_5g_av_request_t *request,
    ogs_hsm_5g_av_response_t *response);

ogs_hsm_rv_t ogs_hsm_resynchronize(
    const ogs_hsm_resync_request_t *request,
    uint8_t sqn_ms[OGS_HSM_SQN_MS_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* OGS_HSM_H */
