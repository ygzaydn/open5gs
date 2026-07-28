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

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static int connect_with_timeout(
    const char *host, uint16_t port, uint32_t timeout_ms)
{
    struct addrinfo hints, *res = NULL, *ai;
    char port_str[8];
    int fd = -1;
    int rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    ogs_snprintf(port_str, sizeof(port_str), "%u", port);

    rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0 || !res)
        return -1;

    for (ai = res; ai; ai = ai->ai_next) {
        int flags;
        struct pollfd pfd;
        int so_err;
        socklen_t so_err_len = sizeof(so_err);

        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0)
            continue;

        flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        rc = connect(fd, ai->ai_addr, ai->ai_addrlen);
        if (rc == 0) {
            fcntl(fd, F_SETFL, flags);
            break;
        }

        if (errno != EINPROGRESS) {
            close(fd);
            fd = -1;
            continue;
        }

        pfd.fd = fd;
        pfd.events = POLLOUT;

        rc = poll(&pfd, 1, (int)timeout_ms);
        if (rc <= 0) {
            close(fd);
            fd = -1;
            continue;
        }

        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_err, &so_err_len) < 0 ||
            so_err != 0) {
            close(fd);
            fd = -1;
            continue;
        }

        fcntl(fd, F_SETFL, flags);
        break;
    }

    freeaddrinfo(res);
    return fd;
}

static bool write_all_with_timeout(
    int fd, const uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    size_t sent = 0;

    while (sent < len) {
        struct pollfd pfd;
        ssize_t n;
        int rc;

        pfd.fd = fd;
        pfd.events = POLLOUT;

        rc = poll(&pfd, 1, (int)timeout_ms);
        if (rc <= 0)
            return false;

        n = send(fd, buf + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (n == 0)
            return false;

        sent += (size_t)n;
    }

    return true;
}

static bool read_all_with_timeout(
    int fd, uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    size_t got = 0;

    while (got < len) {
        struct pollfd pfd;
        ssize_t n;
        int rc;

        pfd.fd = fd;
        pfd.events = POLLIN;

        rc = poll(&pfd, 1, (int)timeout_ms);
        if (rc <= 0)
            return false;

        n = recv(fd, buf + got, len - got, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (n == 0)
            return false; /* peer closed early */

        got += (size_t)n;
    }

    return true;
}

hsm_tcp_rv_t hsm_tcp_round_trip(
    const char *host, uint16_t port,
    uint32_t connect_timeout_ms, uint32_t io_timeout_ms,
    uint32_t max_frame_size,
    const uint8_t *request, size_t request_len,
    uint8_t *response_buf, size_t response_buf_cap, size_t *response_len)
{
    int fd;
    hsm_tcp_rv_t rv = HSM_TCP_OK;
    uint8_t frame_header[OGS_HSM_DAEMON_FRAME_HEADER_LEN];
    uint32_t payload_len;
    uint8_t status;

    fd = connect_with_timeout(host, port, connect_timeout_ms);
    if (fd < 0)
        return HSM_TCP_ERROR_CONNECT;

    if (!write_all_with_timeout(fd, request, request_len, io_timeout_ms)) {
        close(fd);
        return HSM_TCP_ERROR_IO;
    }

    if (!read_all_with_timeout(
            fd, frame_header, sizeof(frame_header), io_timeout_ms)) {
        close(fd);
        return HSM_TCP_ERROR_IO;
    }

    payload_len = ((uint32_t)frame_header[0] << 24) |
                  ((uint32_t)frame_header[1] << 16) |
                  ((uint32_t)frame_header[2] << 8) |
                  (uint32_t)frame_header[3];
    status = frame_header[4];

    if (payload_len > max_frame_size) {
        close(fd);
        return HSM_TCP_ERROR_OVERSIZED;
    }

    if (status == OGS_HSM_DAEMON_STATUS_OK) {
        if (payload_len > response_buf_cap) {
            close(fd);
            return HSM_TCP_ERROR_OVERSIZED;
        }
        if (!read_all_with_timeout(
                fd, response_buf, payload_len, io_timeout_ms)) {
            close(fd);
            return HSM_TCP_ERROR_IO;
        }
        *response_len = payload_len;
        rv = HSM_TCP_OK;
    } else if (status == OGS_HSM_DAEMON_STATUS_ERROR) {
        /* Drain the error payload (bounded by max_frame_size check
         * above) so the connection can be closed cleanly; the text
         * itself is not surfaced to callers -- see ogs_hsm_rv_t. */
        uint8_t discard[256];
        uint32_t remaining = payload_len;

        while (remaining > 0) {
            uint32_t chunk = remaining < sizeof(discard) ?
                remaining : sizeof(discard);
            if (!read_all_with_timeout(fd, discard, chunk, io_timeout_ms)) {
                close(fd);
                return HSM_TCP_ERROR_IO;
            }
            remaining -= chunk;
        }
        rv = HSM_TCP_ERROR_DAEMON_REJECTED;
    } else {
        rv = HSM_TCP_ERROR_MALFORMED_FRAME;
    }

    close(fd);
    return rv;
}
