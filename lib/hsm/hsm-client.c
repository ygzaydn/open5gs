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

static ogs_hsm_config_t self_config;
static bool self_config_set = false;

void ogs_hsm_set_config(const ogs_hsm_config_t *config)
{
    ogs_assert(config);
    self_config = *config;
    self_config_set = true;
}

const ogs_hsm_config_t *ogs_hsm_config(void)
{
    return self_config_set ? &self_config : NULL;
}

bool ogs_hsm_is_enabled(void)
{
    return self_config_set && self_config.enabled;
}

const char *ogs_hsm_rv_string(ogs_hsm_rv_t rv)
{
    switch (rv) {
    case OGS_HSM_OK: return "OK";
    case OGS_HSM_ERROR_DISABLED: return "HSM support disabled";
    case OGS_HSM_ERROR_INVALID_REQUEST: return "invalid request";
    case OGS_HSM_ERROR_CONNECT: return "TCP connect failed";
    case OGS_HSM_ERROR_IO: return "TCP I/O failed";
    case OGS_HSM_ERROR_DAEMON_REJECTED: return "daemon rejected request";
    case OGS_HSM_ERROR_MALFORMED_FRAME: return "malformed daemon frame";
    case OGS_HSM_ERROR_INVALID_RESPONSE: return "invalid response fields";
    default: return "unknown";
    }
}

#define HSM_WIRE_MSG_MAX 2048

static ogs_hsm_rv_t rv_from_tcp(hsm_tcp_rv_t tcp_rv)
{
    switch (tcp_rv) {
    case HSM_TCP_OK: return OGS_HSM_OK;
    case HSM_TCP_ERROR_CONNECT: return OGS_HSM_ERROR_CONNECT;
    case HSM_TCP_ERROR_IO: return OGS_HSM_ERROR_IO;
    case HSM_TCP_ERROR_DAEMON_REJECTED: return OGS_HSM_ERROR_DAEMON_REJECTED;
    case HSM_TCP_ERROR_MALFORMED_FRAME: return OGS_HSM_ERROR_MALFORMED_FRAME;
    case HSM_TCP_ERROR_OVERSIZED: return OGS_HSM_ERROR_MALFORMED_FRAME;
    default: return OGS_HSM_ERROR_IO;
    }
}

ogs_hsm_rv_t ogs_hsm_generate_5g_he_av(
    const ogs_hsm_5g_av_request_t *request,
    ogs_hsm_5g_av_response_t *response)
{
    const ogs_hsm_config_t *config = ogs_hsm_config();
    uint8_t req_buf[HSM_WIRE_MSG_MAX];
    uint8_t resp_buf[HSM_WIRE_MSG_MAX];
    int req_len;
    size_t resp_len = 0;
    hsm_tcp_rv_t tcp_rv;
    hsm_wire_fields_t fields;

    ogs_assert(request);
    ogs_assert(response);

    if (!ogs_hsm_is_enabled())
        return OGS_HSM_ERROR_DISABLED;

    if (!request->supi ||
        !request->wrapped_k || !request->wrapped_k_len ||
        !request->wrapped_opc || !request->wrapped_opc_len ||
        request->wrapped_k_len > OGS_HSM_MAX_WRAPPED_KEY_LEN ||
        request->wrapped_opc_len > OGS_HSM_MAX_WRAPPED_KEY_LEN)
        return OGS_HSM_ERROR_INVALID_REQUEST;

    req_len = hsm_wire_build_5g_he_av_request(
        req_buf, sizeof(req_buf),
        request->supi,
        request->wrapped_k, request->wrapped_k_len,
        request->wrapped_opc, request->wrapped_opc_len,
        request->sqn, request->amf,
        request->serving_network_name);
    if (req_len < 0)
        return OGS_HSM_ERROR_INVALID_REQUEST;

    tcp_rv = hsm_tcp_round_trip(
        config->host, config->port,
        config->connect_timeout_ms, config->io_timeout_ms,
        config->max_frame_size,
        req_buf, (size_t)req_len,
        resp_buf, sizeof(resp_buf), &resp_len);
    if (tcp_rv != HSM_TCP_OK)
        return rv_from_tcp(tcp_rv);

    if (!hsm_wire_parse_response(resp_buf, resp_len, &fields))
        return OGS_HSM_ERROR_MALFORMED_FRAME;

    if (!fields.rand_present || !fields.autn_present ||
        !fields.xres_star_present || !fields.kausf_present)
        return OGS_HSM_ERROR_INVALID_RESPONSE;

    memcpy(response->rand, fields.rand, OGS_HSM_RAND_LEN);
    memcpy(response->autn, fields.autn, OGS_HSM_AUTN_LEN);
    memcpy(response->xres_star, fields.xres_star, OGS_HSM_XRES_STAR_LEN);
    memcpy(response->kausf, fields.kausf, OGS_HSM_KAUSF_LEN);

    return OGS_HSM_OK;
}

ogs_hsm_rv_t ogs_hsm_resynchronize(
    const ogs_hsm_resync_request_t *request,
    uint8_t sqn_ms[OGS_HSM_SQN_MS_LEN])
{
    const ogs_hsm_config_t *config = ogs_hsm_config();
    uint8_t req_buf[HSM_WIRE_MSG_MAX];
    uint8_t resp_buf[HSM_WIRE_MSG_MAX];
    int req_len;
    size_t resp_len = 0;
    hsm_tcp_rv_t tcp_rv;
    hsm_wire_fields_t fields;

    ogs_assert(request);
    ogs_assert(sqn_ms);

    if (!ogs_hsm_is_enabled())
        return OGS_HSM_ERROR_DISABLED;

    if (!request->supi ||
        !request->wrapped_k || !request->wrapped_k_len ||
        !request->wrapped_opc || !request->wrapped_opc_len ||
        request->wrapped_k_len > OGS_HSM_MAX_WRAPPED_KEY_LEN ||
        request->wrapped_opc_len > OGS_HSM_MAX_WRAPPED_KEY_LEN)
        return OGS_HSM_ERROR_INVALID_REQUEST;

    req_len = hsm_wire_build_resync_request(
        req_buf, sizeof(req_buf),
        request->supi,
        request->wrapped_k, request->wrapped_k_len,
        request->wrapped_opc, request->wrapped_opc_len,
        request->rand, request->auts);
    if (req_len < 0)
        return OGS_HSM_ERROR_INVALID_REQUEST;

    tcp_rv = hsm_tcp_round_trip(
        config->host, config->port,
        config->connect_timeout_ms, config->io_timeout_ms,
        config->max_frame_size,
        req_buf, (size_t)req_len,
        resp_buf, sizeof(resp_buf), &resp_len);
    if (tcp_rv != HSM_TCP_OK)
        return rv_from_tcp(tcp_rv);

    if (!hsm_wire_parse_response(resp_buf, resp_len, &fields))
        return OGS_HSM_ERROR_MALFORMED_FRAME;

    if (!fields.sqn_ms_present)
        return OGS_HSM_ERROR_INVALID_RESPONSE;

    memcpy(sqn_ms, fields.sqn_ms, OGS_HSM_SQN_MS_LEN);

    return OGS_HSM_OK;
}
