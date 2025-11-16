/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2024 Discere OS
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef G_POSIX_SOCKETS_PROXY_H
#define G_POSIX_SOCKETS_PROXY_H

#include <glib.h>

#ifdef __EMSCRIPTEN__

#include <sys/socket.h>
#include <netdb.h>

G_BEGIN_DECLS

typedef struct _GPosixSocketsProxy GPosixSocketsProxy;

/**
 * g_posix_sockets_proxy_new:
 * @proxy_url: WebSocket URL of POSIX proxy (e.g., wss://posix-proxy.discere.cloud/v1)
 *
 * Creates a new POSIX sockets proxy client.
 *
 * Returns: New proxy instance, or NULL on error
 */
GPosixSocketsProxy* g_posix_sockets_proxy_new(const char *proxy_url);

/**
 * g_posix_sockets_proxy_free:
 * @proxy: Proxy to free
 *
 * Frees proxy resources and closes all sockets.
 */
void g_posix_sockets_proxy_free(GPosixSocketsProxy *proxy);

// Proxied socket operations (identical to POSIX API)

int proxy_socket(GPosixSocketsProxy *proxy,
                 int domain, int type, int protocol);

int proxy_connect(GPosixSocketsProxy *proxy, int sockfd,
                  const struct sockaddr *addr, socklen_t addrlen);

ssize_t proxy_send(GPosixSocketsProxy *proxy, int sockfd,
                   const void *buf, size_t len, int flags);

ssize_t proxy_recv(GPosixSocketsProxy *proxy, int sockfd,
                   void *buf, size_t len, int flags);

int proxy_shutdown(GPosixSocketsProxy *proxy, int sockfd, int how);

int proxy_bind(GPosixSocketsProxy *proxy, int sockfd,
               const struct sockaddr *addr, socklen_t addrlen);

int proxy_listen(GPosixSocketsProxy *proxy, int sockfd, int backlog);

int proxy_accept(GPosixSocketsProxy *proxy, int sockfd,
                 struct sockaddr *addr, socklen_t *addrlen);

int proxy_getsockname(GPosixSocketsProxy *proxy, int sockfd,
                      struct sockaddr *addr, socklen_t *addrlen);

int proxy_getpeername(GPosixSocketsProxy *proxy, int sockfd,
                      struct sockaddr *addr, socklen_t *addrlen);

int proxy_getsockopt(GPosixSocketsProxy *proxy, int sockfd,
                     int level, int optname,
                     void *optval, socklen_t *optlen);

int proxy_setsockopt(GPosixSocketsProxy *proxy, int sockfd,
                     int level, int optname,
                     const void *optval, socklen_t optlen);

int proxy_getaddrinfo(GPosixSocketsProxy *proxy,
                      const char *node, const char *service,
                      const struct addrinfo *hints,
                      struct addrinfo **res);

void proxy_freeaddrinfo(struct addrinfo *res);

int proxy_getnameinfo(GPosixSocketsProxy *proxy,
                      const struct sockaddr *addr, socklen_t addrlen,
                      char *host, socklen_t hostlen,
                      char *serv, socklen_t servlen, int flags);

G_END_DECLS

#endif /* __EMSCRIPTEN__ */

#endif /* G_POSIX_SOCKETS_PROXY_H */
