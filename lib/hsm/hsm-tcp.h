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

#ifndef HSM_TCP_H
#define HSM_TCP_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------
 * Daemon TCP framing (softhsm2-milenaged.cpp), exactly as implemented:
 *
 *   request  := <S5GM message>                              (client -> daemon)
 *   response := u32be payload_len | u8 status | payload      (daemon -> client)
 *
 * - payload_len is big-endian, 4 bytes, and counts ONLY the payload
 *   that follows the status byte -- it does NOT include the status
 *   byte itself.
 * - status: 0x00 = success (payload is an S5GM response message),
 *           0x01 = error (payload is a short UTF-8 error string,
 *           generic -- the daemon does not send distinguishing error
 *           sub-codes over the wire).
 * - The request needs no extra framing beyond the S5GM header itself,
 *   which is self-describing (magic + total_length).
 * ------------------------------------------------------------------- */
#define OGS_HSM_DAEMON_FRAME_HEADER_LEN 5 /* payload_len(4) + status(1) */
#define OGS_HSM_DAEMON_STATUS_OK    0x00
#define OGS_HSM_DAEMON_STATUS_ERROR 0x01

typedef enum {
    HSM_TCP_OK = 0,
    HSM_TCP_ERROR_CONNECT,
    HSM_TCP_ERROR_IO,
    HSM_TCP_ERROR_MALFORMED_FRAME,
    HSM_TCP_ERROR_DAEMON_REJECTED,
    HSM_TCP_ERROR_OVERSIZED
} hsm_tcp_rv_t;

/* Opens one TCP connection to host:port (with connect_timeout_ms),
 * writes `request`/`request_len` in full, reads back one daemon
 * frame (with io_timeout_ms applied to each read), and:
 *   - on daemon status OK, copies the payload (an S5GM response
 *     message) into `response_buf` (capacity `response_buf_cap`) and
 *     sets *response_len;
 *   - on daemon status ERROR, returns HSM_TCP_ERROR_DAEMON_REJECTED;
 *   - rejects (before allocating/reading) any frame whose declared
 *     payload_len exceeds max_frame_size.
 * Always closes the socket before returning, on every path. Performs
 * no automatic retry. */
hsm_tcp_rv_t hsm_tcp_round_trip(
    const char *host, uint16_t port,
    uint32_t connect_timeout_ms, uint32_t io_timeout_ms,
    uint32_t max_frame_size,
    const uint8_t *request, size_t request_len,
    uint8_t *response_buf, size_t response_buf_cap, size_t *response_len);

#ifdef __cplusplus
}
#endif

#endif /* HSM_TCP_H */
