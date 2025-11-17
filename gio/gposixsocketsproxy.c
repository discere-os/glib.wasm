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

#include "config.h"

#ifdef __EMSCRIPTEN__

#include "gposixsocketsproxy.h"
#include <emscripten/websocket.h>
#include <emscripten/threading.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>

struct _GPosixSocketsProxy {
    EMSCRIPTEN_WEBSOCKET_T ws;
    GMutex lock;
    GHashTable *pending_calls;  // callId → GAsyncQueue*
    guint32 next_call_id;
    gboolean connected;
    gchar *proxy_url;
};

typedef enum {
    MSG_SOCKET = 1,
    MSG_SOCKETPAIR = 2,
    MSG_SHUTDOWN = 3,
    MSG_BIND = 4,
    MSG_CONNECT = 5,
    MSG_LISTEN = 6,
    MSG_ACCEPT = 7,
    MSG_GETSOCKNAME = 8,
    MSG_GETPEERNAME = 9,
    MSG_SEND = 10,
    MSG_RECV = 11,
    MSG_SENDTO = 12,
    MSG_RECVFROM = 13,
    MSG_SENDMSG = 14,
    MSG_RECVMSG = 15,
    MSG_GETSOCKOPT = 16,
    MSG_SETSOCKOPT = 17,
    MSG_GETADDRINFO = 18,
    MSG_GETNAMEINFO = 19,
} ProxyMessageType;

typedef struct {
    guint32 callId;
    guint32 function;
} MessageHeader;

static EM_BOOL
websocket_open_callback(int eventType, const EmscriptenWebSocketOpenEvent *event,
                       void *userData)
{
    GPosixSocketsProxy *proxy = userData;
    g_mutex_lock(&proxy->lock);
    proxy->connected = TRUE;
    g_mutex_unlock(&proxy->lock);

    g_debug("[POSIX Proxy] WebSocket connected to %s", proxy->proxy_url);
    return EM_TRUE;
}

static EM_BOOL
websocket_message_callback(int eventType, const EmscriptenWebSocketMessageEvent *event,
                          void *userData)
{
    GPosixSocketsProxy *proxy = userData;

    if (!event->isText && event->numBytes >= sizeof(MessageHeader)) {
        // Binary message - parse response
        MessageHeader *header = (MessageHeader*)event->data;
        guint32 callId = GUINT32_FROM_LE(header->callId);

        g_mutex_lock(&proxy->lock);
        GAsyncQueue *queue = g_hash_table_lookup(proxy->pending_calls,
                                                GUINT_TO_POINTER(callId));
        if (queue) {
            // Copy response data
            guchar *response = g_malloc(event->numBytes);
            memcpy(response, event->data, event->numBytes);
            g_async_queue_push(queue, response);
        } else {
            g_warning("[POSIX Proxy] Received response for unknown callId: %u", callId);
        }
        g_mutex_unlock(&proxy->lock);
    }

    return EM_TRUE;
}

static EM_BOOL
websocket_error_callback(int eventType, const EmscriptenWebSocketErrorEvent *event,
                        void *userData)
{
    GPosixSocketsProxy *proxy = userData;
    g_warning("[POSIX Proxy] WebSocket error occurred on connection to %s", proxy->proxy_url);
    return EM_TRUE;
}

static EM_BOOL
websocket_close_callback(int eventType, const EmscriptenWebSocketCloseEvent *event,
                        void *userData)
{
    GPosixSocketsProxy *proxy = userData;
    g_mutex_lock(&proxy->lock);
    proxy->connected = FALSE;
    g_mutex_unlock(&proxy->lock);

    g_debug("[POSIX Proxy] WebSocket closed (code %d, reason: %s)",
            event->code, event->reason);
    return EM_TRUE;
}

GPosixSocketsProxy*
g_posix_sockets_proxy_new(const char *proxy_url)
{
    g_return_val_if_fail(proxy_url != NULL, NULL);

    GPosixSocketsProxy *proxy = g_new0(GPosixSocketsProxy, 1);

    g_mutex_init(&proxy->lock);
    proxy->pending_calls = g_hash_table_new(g_direct_hash, g_direct_equal);
    proxy->next_call_id = 1;
    proxy->connected = FALSE;
    proxy->proxy_url = g_strdup(proxy_url);

    // Create WebSocket connection
    EmscriptenWebSocketCreateAttributes attrs = {
        .url = proxy_url,
        .protocols = NULL,
        .createOnMainThread = EM_TRUE,
    };

    proxy->ws = emscripten_websocket_new(&attrs);

    if (proxy->ws <= 0) {
        g_warning("[POSIX Proxy] Failed to create WebSocket to %s", proxy_url);
        g_posix_sockets_proxy_free(proxy);
        return NULL;
    }

    // Set up callbacks
    emscripten_websocket_set_onopen_callback(proxy->ws, proxy,
                                            websocket_open_callback);
    emscripten_websocket_set_onmessage_callback(proxy->ws, proxy,
                                               websocket_message_callback);
    emscripten_websocket_set_onerror_callback(proxy->ws, proxy,
                                             websocket_error_callback);
    emscripten_websocket_set_onclose_callback(proxy->ws, proxy,
                                             websocket_close_callback);

    // Wait for connection with timeout
    int retries = 0;
    const int max_retries = 500; // 5 seconds with 10ms sleep
    while (!proxy->connected && retries < max_retries) {
        emscripten_sleep(10);
        retries++;
    }

    if (!proxy->connected) {
        g_warning("[POSIX Proxy] WebSocket connection timeout to %s", proxy_url);
        g_posix_sockets_proxy_free(proxy);
        return NULL;
    }

    g_debug("[POSIX Proxy] Successfully connected to %s", proxy_url);
    return proxy;
}

void
g_posix_sockets_proxy_free(GPosixSocketsProxy *proxy)
{
    g_return_if_fail(proxy != NULL);

    if (proxy->ws > 0) {
        emscripten_websocket_close(proxy->ws, 1000, "Normal closure");
        emscripten_websocket_delete(proxy->ws);
    }

    if (proxy->pending_calls) {
        g_hash_table_destroy(proxy->pending_calls);
    }

    g_free(proxy->proxy_url);
    g_mutex_clear(&proxy->lock);
    g_free(proxy);
}

int
proxy_socket(GPosixSocketsProxy *proxy, int domain, int type, int protocol)
{
    g_return_val_if_fail(proxy != NULL, -1);

    if (!proxy->connected) {
        errno = ENOTCONN;
        return -1;
    }

    guint32 callId = g_atomic_int_add(&proxy->next_call_id, 1);

    // Build SOCKET message
    struct {
        MessageHeader header;
        gint32 domain;
        gint32 type;
        gint32 protocol;
    } msg;

    msg.header.callId = GUINT32_TO_LE(callId);
    msg.header.function = GUINT32_TO_LE(MSG_SOCKET);
    msg.domain = GINT32_TO_LE(domain);
    msg.type = GINT32_TO_LE(type);
    msg.protocol = GINT32_TO_LE(protocol);

    // Create response queue
    GAsyncQueue *queue = g_async_queue_new();
    g_mutex_lock(&proxy->lock);
    g_hash_table_insert(proxy->pending_calls, GUINT_TO_POINTER(callId), queue);
    g_mutex_unlock(&proxy->lock);

    // Send message
    EMSCRIPTEN_RESULT result = emscripten_websocket_send_binary(
        proxy->ws, &msg, sizeof(msg));

    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        g_async_queue_unref(queue);
        g_mutex_lock(&proxy->lock);
        g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
        g_mutex_unlock(&proxy->lock);
        errno = EIO;
        return -1;
    }

    // Wait for response
    guchar *response = g_async_queue_pop(queue);

    // Parse response
    struct {
        MessageHeader header;
        gint32 result;
        gint32 errno_val;
    } *resp = (void*)response;

    int fd = GINT32_FROM_LE(resp->result);
    if (fd < 0) {
        errno = GINT32_FROM_LE(resp->errno_val);
    }

    // Cleanup
    g_free(response);
    g_async_queue_unref(queue);
    g_mutex_lock(&proxy->lock);
    g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
    g_mutex_unlock(&proxy->lock);

    return fd;
}

int
proxy_connect(GPosixSocketsProxy *proxy, int sockfd,
              const struct sockaddr *addr, socklen_t addrlen)
{
    g_return_val_if_fail(proxy != NULL, -1);
    g_return_val_if_fail(addr != NULL, -1);

    if (!proxy->connected) {
        errno = ENOTCONN;
        return -1;
    }

    guint32 callId = g_atomic_int_add(&proxy->next_call_id, 1);

    // Build CONNECT message
    struct {
        MessageHeader header;
        gint32 fd;
        guint16 family;
        guint16 port;
        guint8 addr_bytes[16];
    } msg = {0};

    msg.header.callId = GUINT32_TO_LE(callId);
    msg.header.function = GUINT32_TO_LE(MSG_CONNECT);
    msg.fd = GINT32_TO_LE(sockfd);

    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in*)addr;
        msg.family = GUINT16_TO_BE(2);  // AF_INET
        msg.port = sin->sin_port;  // Already network byte order
        memcpy(msg.addr_bytes, &sin->sin_addr.s_addr, 4);
    } else if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)addr;
        msg.family = GUINT16_TO_BE(10);  // AF_INET6
        msg.port = sin6->sin6_port;
        memcpy(msg.addr_bytes, &sin6->sin6_addr, 16);
    } else {
        errno = EAFNOSUPPORT;
        return -1;
    }

    // Send and wait for response
    GAsyncQueue *queue = g_async_queue_new();
    g_mutex_lock(&proxy->lock);
    g_hash_table_insert(proxy->pending_calls, GUINT_TO_POINTER(callId), queue);
    g_mutex_unlock(&proxy->lock);

    EMSCRIPTEN_RESULT ws_result = emscripten_websocket_send_binary(proxy->ws, &msg, sizeof(msg));

    if (ws_result != EMSCRIPTEN_RESULT_SUCCESS) {
        g_async_queue_unref(queue);
        g_mutex_lock(&proxy->lock);
        g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
        g_mutex_unlock(&proxy->lock);
        errno = EIO;
        return -1;
    }

    guchar *response = g_async_queue_pop(queue);

    struct {
        MessageHeader header;
        gint32 result;
        gint32 errno_val;
    } *resp = (void*)response;

    int ret = GINT32_FROM_LE(resp->result);
    if (ret < 0) {
        errno = GINT32_FROM_LE(resp->errno_val);
    }

    g_free(response);
    g_async_queue_unref(queue);
    g_mutex_lock(&proxy->lock);
    g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
    g_mutex_unlock(&proxy->lock);

    return ret;
}

ssize_t
proxy_send(GPosixSocketsProxy *proxy, int sockfd,
           const void *buf, size_t len, int flags)
{
    g_return_val_if_fail(proxy != NULL, -1);
    g_return_val_if_fail(buf != NULL || len == 0, -1);

    if (!proxy->connected) {
        errno = ENOTCONN;
        return -1;
    }

    guint32 callId = g_atomic_int_add(&proxy->next_call_id, 1);

    // Build SEND message (variable length)
    size_t msg_size = sizeof(MessageHeader) + 12 + len;
    guchar *msg = g_malloc(msg_size);

    MessageHeader *header = (MessageHeader*)msg;
    header->callId = GUINT32_TO_LE(callId);
    header->function = GUINT32_TO_LE(MSG_SEND);

    gint32 *fd_ptr = (gint32*)(msg + sizeof(MessageHeader));
    *fd_ptr = GINT32_TO_LE(sockfd);

    gint32 *len_ptr = (gint32*)(msg + sizeof(MessageHeader) + 4);
    *len_ptr = GINT32_TO_LE((gint32)len);

    gint32 *flags_ptr = (gint32*)(msg + sizeof(MessageHeader) + 8);
    *flags_ptr = GINT32_TO_LE(flags);

    if (len > 0) {
        memcpy(msg + sizeof(MessageHeader) + 12, buf, len);
    }

    // Send and wait
    GAsyncQueue *queue = g_async_queue_new();
    g_mutex_lock(&proxy->lock);
    g_hash_table_insert(proxy->pending_calls, GUINT_TO_POINTER(callId), queue);
    g_mutex_unlock(&proxy->lock);

    EMSCRIPTEN_RESULT ws_result = emscripten_websocket_send_binary(proxy->ws, msg, msg_size);
    g_free(msg);

    if (ws_result != EMSCRIPTEN_RESULT_SUCCESS) {
        g_async_queue_unref(queue);
        g_mutex_lock(&proxy->lock);
        g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
        g_mutex_unlock(&proxy->lock);
        errno = EIO;
        return -1;
    }

    guchar *response = g_async_queue_pop(queue);

    struct {
        MessageHeader header;
        gint32 result;
        gint32 errno_val;
    } *resp = (void*)response;

    ssize_t sent = GINT32_FROM_LE(resp->result);
    if (sent < 0) {
        errno = GINT32_FROM_LE(resp->errno_val);
    }

    g_free(response);
    g_async_queue_unref(queue);
    g_mutex_lock(&proxy->lock);
    g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
    g_mutex_unlock(&proxy->lock);

    return sent;
}

ssize_t
proxy_recv(GPosixSocketsProxy *proxy, int sockfd,
           void *buf, size_t len, int flags)
{
    g_return_val_if_fail(proxy != NULL, -1);
    g_return_val_if_fail(buf != NULL || len == 0, -1);

    if (!proxy->connected) {
        errno = ENOTCONN;
        return -1;
    }

    guint32 callId = g_atomic_int_add(&proxy->next_call_id, 1);

    // Build RECV message
    struct {
        MessageHeader header;
        gint32 fd;
        gint32 length;
        gint32 flags;
    } msg;

    msg.header.callId = GUINT32_TO_LE(callId);
    msg.header.function = GUINT32_TO_LE(MSG_RECV);
    msg.fd = GINT32_TO_LE(sockfd);
    msg.length = GINT32_TO_LE((gint32)len);
    msg.flags = GINT32_TO_LE(flags);

    // Send and wait
    GAsyncQueue *queue = g_async_queue_new();
    g_mutex_lock(&proxy->lock);
    g_hash_table_insert(proxy->pending_calls, GUINT_TO_POINTER(callId), queue);
    g_mutex_unlock(&proxy->lock);

    EMSCRIPTEN_RESULT ws_result = emscripten_websocket_send_binary(proxy->ws, &msg, sizeof(msg));

    if (ws_result != EMSCRIPTEN_RESULT_SUCCESS) {
        g_async_queue_unref(queue);
        g_mutex_lock(&proxy->lock);
        g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
        g_mutex_unlock(&proxy->lock);
        errno = EIO;
        return -1;
    }

    guchar *response = g_async_queue_pop(queue);

    // Response includes data
    struct {
        MessageHeader header;
        gint32 result;
        gint32 errno_val;
        guchar data[];
    } *resp = (void*)response;

    ssize_t received = GINT32_FROM_LE(resp->result);

    if (received > 0) {
        size_t copy_len = (size_t)received < len ? (size_t)received : len;
        memcpy(buf, resp->data, copy_len);
    } else if (received < 0) {
        errno = GINT32_FROM_LE(resp->errno_val);
    }

    g_free(response);
    g_async_queue_unref(queue);
    g_mutex_lock(&proxy->lock);
    g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
    g_mutex_unlock(&proxy->lock);

    return received;
}

int
proxy_shutdown(GPosixSocketsProxy *proxy, int sockfd, int how)
{
    g_return_val_if_fail(proxy != NULL, -1);

    if (!proxy->connected) {
        errno = ENOTCONN;
        return -1;
    }

    guint32 callId = g_atomic_int_add(&proxy->next_call_id, 1);

    struct {
        MessageHeader header;
        gint32 fd;
        gint32 how;
    } msg;

    msg.header.callId = GUINT32_TO_LE(callId);
    msg.header.function = GUINT32_TO_LE(MSG_SHUTDOWN);
    msg.fd = GINT32_TO_LE(sockfd);
    msg.how = GINT32_TO_LE(how);

    GAsyncQueue *queue = g_async_queue_new();
    g_mutex_lock(&proxy->lock);
    g_hash_table_insert(proxy->pending_calls, GUINT_TO_POINTER(callId), queue);
    g_mutex_unlock(&proxy->lock);

    EMSCRIPTEN_RESULT ws_result = emscripten_websocket_send_binary(proxy->ws, &msg, sizeof(msg));

    if (ws_result != EMSCRIPTEN_RESULT_SUCCESS) {
        g_async_queue_unref(queue);
        g_mutex_lock(&proxy->lock);
        g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
        g_mutex_unlock(&proxy->lock);
        errno = EIO;
        return -1;
    }

    guchar *response = g_async_queue_pop(queue);

    struct {
        MessageHeader header;
        gint32 result;
        gint32 errno_val;
    } *resp = (void*)response;

    int ret = GINT32_FROM_LE(resp->result);
    if (ret < 0) {
        errno = GINT32_FROM_LE(resp->errno_val);
    }

    g_free(response);
    g_async_queue_unref(queue);
    g_mutex_lock(&proxy->lock);
    g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
    g_mutex_unlock(&proxy->lock);

    return ret;
}

int
proxy_bind(GPosixSocketsProxy *proxy, int sockfd,
           const struct sockaddr *addr, socklen_t addrlen)
{
    g_return_val_if_fail(proxy != NULL, -1);
    g_return_val_if_fail(addr != NULL, -1);

    if (!proxy->connected) {
        errno = ENOTCONN;
        return -1;
    }

    guint32 callId = g_atomic_int_add(&proxy->next_call_id, 1);

    struct {
        MessageHeader header;
        gint32 fd;
        guint16 family;
        guint16 port;
        guint8 addr_bytes[16];
    } msg = {0};

    msg.header.callId = GUINT32_TO_LE(callId);
    msg.header.function = GUINT32_TO_LE(MSG_BIND);
    msg.fd = GINT32_TO_LE(sockfd);

    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in*)addr;
        msg.family = GUINT16_TO_BE(2);
        msg.port = sin->sin_port;
        memcpy(msg.addr_bytes, &sin->sin_addr.s_addr, 4);
    } else if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)addr;
        msg.family = GUINT16_TO_BE(10);
        msg.port = sin6->sin6_port;
        memcpy(msg.addr_bytes, &sin6->sin6_addr, 16);
    } else {
        errno = EAFNOSUPPORT;
        return -1;
    }

    GAsyncQueue *queue = g_async_queue_new();
    g_mutex_lock(&proxy->lock);
    g_hash_table_insert(proxy->pending_calls, GUINT_TO_POINTER(callId), queue);
    g_mutex_unlock(&proxy->lock);

    EMSCRIPTEN_RESULT ws_result = emscripten_websocket_send_binary(proxy->ws, &msg, sizeof(msg));

    if (ws_result != EMSCRIPTEN_RESULT_SUCCESS) {
        g_async_queue_unref(queue);
        g_mutex_lock(&proxy->lock);
        g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
        g_mutex_unlock(&proxy->lock);
        errno = EIO;
        return -1;
    }

    guchar *response = g_async_queue_pop(queue);

    struct {
        MessageHeader header;
        gint32 result;
        gint32 errno_val;
    } *resp = (void*)response;

    int ret = GINT32_FROM_LE(resp->result);
    if (ret < 0) {
        errno = GINT32_FROM_LE(resp->errno_val);
    }

    g_free(response);
    g_async_queue_unref(queue);
    g_mutex_lock(&proxy->lock);
    g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
    g_mutex_unlock(&proxy->lock);

    return ret;
}

int
proxy_listen(GPosixSocketsProxy *proxy, int sockfd, int backlog)
{
    g_return_val_if_fail(proxy != NULL, -1);

    if (!proxy->connected) {
        errno = ENOTCONN;
        return -1;
    }

    guint32 callId = g_atomic_int_add(&proxy->next_call_id, 1);

    struct {
        MessageHeader header;
        gint32 fd;
        gint32 backlog;
    } msg;

    msg.header.callId = GUINT32_TO_LE(callId);
    msg.header.function = GUINT32_TO_LE(MSG_LISTEN);
    msg.fd = GINT32_TO_LE(sockfd);
    msg.backlog = GINT32_TO_LE(backlog);

    GAsyncQueue *queue = g_async_queue_new();
    g_mutex_lock(&proxy->lock);
    g_hash_table_insert(proxy->pending_calls, GUINT_TO_POINTER(callId), queue);
    g_mutex_unlock(&proxy->lock);

    EMSCRIPTEN_RESULT ws_result = emscripten_websocket_send_binary(proxy->ws, &msg, sizeof(msg));

    if (ws_result != EMSCRIPTEN_RESULT_SUCCESS) {
        g_async_queue_unref(queue);
        g_mutex_lock(&proxy->lock);
        g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
        g_mutex_unlock(&proxy->lock);
        errno = EIO;
        return -1;
    }

    guchar *response = g_async_queue_pop(queue);

    struct {
        MessageHeader header;
        gint32 result;
        gint32 errno_val;
    } *resp = (void*)response;

    int ret = GINT32_FROM_LE(resp->result);
    if (ret < 0) {
        errno = GINT32_FROM_LE(resp->errno_val);
    }

    g_free(response);
    g_async_queue_unref(queue);
    g_mutex_lock(&proxy->lock);
    g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
    g_mutex_unlock(&proxy->lock);

    return ret;
}

int
proxy_accept(GPosixSocketsProxy *proxy, int sockfd,
             struct sockaddr *addr, socklen_t *addrlen)
{
    g_return_val_if_fail(proxy != NULL, -1);

    if (!proxy->connected) {
        errno = ENOTCONN;
        return -1;
    }

    guint32 callId = g_atomic_int_add(&proxy->next_call_id, 1);

    struct {
        MessageHeader header;
        gint32 fd;
    } msg;

    msg.header.callId = GUINT32_TO_LE(callId);
    msg.header.function = GUINT32_TO_LE(MSG_ACCEPT);
    msg.fd = GINT32_TO_LE(sockfd);

    GAsyncQueue *queue = g_async_queue_new();
    g_mutex_lock(&proxy->lock);
    g_hash_table_insert(proxy->pending_calls, GUINT_TO_POINTER(callId), queue);
    g_mutex_unlock(&proxy->lock);

    EMSCRIPTEN_RESULT ws_result = emscripten_websocket_send_binary(proxy->ws, &msg, sizeof(msg));

    if (ws_result != EMSCRIPTEN_RESULT_SUCCESS) {
        g_async_queue_unref(queue);
        g_mutex_lock(&proxy->lock);
        g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
        g_mutex_unlock(&proxy->lock);
        errno = EIO;
        return -1;
    }

    guchar *response = g_async_queue_pop(queue);

    struct {
        MessageHeader header;
        gint32 result;
        gint32 errno_val;
        guint16 family;
        guint16 port;
        guint8 addr_bytes[16];
    } *resp = (void*)response;

    int new_fd = GINT32_FROM_LE(resp->result);

    if (new_fd >= 0 && addr != NULL && addrlen != NULL) {
        guint16 family = GUINT16_FROM_BE(resp->family);

        if (family == 2) {  // AF_INET
            struct sockaddr_in *sin = (struct sockaddr_in*)addr;
            memset(sin, 0, sizeof(*sin));
            sin->sin_family = AF_INET;
            sin->sin_port = resp->port;
            memcpy(&sin->sin_addr.s_addr, resp->addr_bytes, 4);
            *addrlen = sizeof(*sin);
        } else if (family == 10) {  // AF_INET6
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)addr;
            memset(sin6, 0, sizeof(*sin6));
            sin6->sin6_family = AF_INET6;
            sin6->sin6_port = resp->port;
            memcpy(&sin6->sin6_addr, resp->addr_bytes, 16);
            *addrlen = sizeof(*sin6);
        }
    } else if (new_fd < 0) {
        errno = GINT32_FROM_LE(resp->errno_val);
    }

    g_free(response);
    g_async_queue_unref(queue);
    g_mutex_lock(&proxy->lock);
    g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
    g_mutex_unlock(&proxy->lock);

    return new_fd;
}

int
proxy_getsockname(GPosixSocketsProxy *proxy, int sockfd,
                  struct sockaddr *addr, socklen_t *addrlen)
{
    g_return_val_if_fail(proxy != NULL, -1);
    g_return_val_if_fail(addr != NULL, -1);
    g_return_val_if_fail(addrlen != NULL, -1);

    if (!proxy->connected) {
        errno = ENOTCONN;
        return -1;
    }

    guint32 callId = g_atomic_int_add(&proxy->next_call_id, 1);

    struct {
        MessageHeader header;
        gint32 fd;
    } msg;

    msg.header.callId = GUINT32_TO_LE(callId);
    msg.header.function = GUINT32_TO_LE(MSG_GETSOCKNAME);
    msg.fd = GINT32_TO_LE(sockfd);

    GAsyncQueue *queue = g_async_queue_new();
    g_mutex_lock(&proxy->lock);
    g_hash_table_insert(proxy->pending_calls, GUINT_TO_POINTER(callId), queue);
    g_mutex_unlock(&proxy->lock);

    EMSCRIPTEN_RESULT ws_result = emscripten_websocket_send_binary(proxy->ws, &msg, sizeof(msg));

    if (ws_result != EMSCRIPTEN_RESULT_SUCCESS) {
        g_async_queue_unref(queue);
        g_mutex_lock(&proxy->lock);
        g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
        g_mutex_unlock(&proxy->lock);
        errno = EIO;
        return -1;
    }

    guchar *response = g_async_queue_pop(queue);

    struct {
        MessageHeader header;
        gint32 result;
        gint32 errno_val;
        guint16 family;
        guint16 port;
        guint8 addr_bytes[16];
    } *resp = (void*)response;

    int ret = GINT32_FROM_LE(resp->result);

    if (ret == 0) {
        guint16 family = GUINT16_FROM_BE(resp->family);

        if (family == 2) {  // AF_INET
            struct sockaddr_in *sin = (struct sockaddr_in*)addr;
            memset(sin, 0, sizeof(*sin));
            sin->sin_family = AF_INET;
            sin->sin_port = resp->port;
            memcpy(&sin->sin_addr.s_addr, resp->addr_bytes, 4);
            *addrlen = sizeof(*sin);
        } else if (family == 10) {  // AF_INET6
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)addr;
            memset(sin6, 0, sizeof(*sin6));
            sin6->sin6_family = AF_INET6;
            sin6->sin6_port = resp->port;
            memcpy(&sin6->sin6_addr, resp->addr_bytes, 16);
            *addrlen = sizeof(*sin6);
        }
    } else {
        errno = GINT32_FROM_LE(resp->errno_val);
    }

    g_free(response);
    g_async_queue_unref(queue);
    g_mutex_lock(&proxy->lock);
    g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
    g_mutex_unlock(&proxy->lock);

    return ret;
}

int
proxy_getpeername(GPosixSocketsProxy *proxy, int sockfd,
                  struct sockaddr *addr, socklen_t *addrlen)
{
    g_return_val_if_fail(proxy != NULL, -1);
    g_return_val_if_fail(addr != NULL, -1);
    g_return_val_if_fail(addrlen != NULL, -1);

    if (!proxy->connected) {
        errno = ENOTCONN;
        return -1;
    }

    guint32 callId = g_atomic_int_add(&proxy->next_call_id, 1);

    struct {
        MessageHeader header;
        gint32 fd;
    } msg;

    msg.header.callId = GUINT32_TO_LE(callId);
    msg.header.function = GUINT32_TO_LE(MSG_GETPEERNAME);
    msg.fd = GINT32_TO_LE(sockfd);

    GAsyncQueue *queue = g_async_queue_new();
    g_mutex_lock(&proxy->lock);
    g_hash_table_insert(proxy->pending_calls, GUINT_TO_POINTER(callId), queue);
    g_mutex_unlock(&proxy->lock);

    EMSCRIPTEN_RESULT ws_result = emscripten_websocket_send_binary(proxy->ws, &msg, sizeof(msg));

    if (ws_result != EMSCRIPTEN_RESULT_SUCCESS) {
        g_async_queue_unref(queue);
        g_mutex_lock(&proxy->lock);
        g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
        g_mutex_unlock(&proxy->lock);
        errno = EIO;
        return -1;
    }

    guchar *response = g_async_queue_pop(queue);

    struct {
        MessageHeader header;
        gint32 result;
        gint32 errno_val;
        guint16 family;
        guint16 port;
        guint8 addr_bytes[16];
    } *resp = (void*)response;

    int ret = GINT32_FROM_LE(resp->result);

    if (ret == 0) {
        guint16 family = GUINT16_FROM_BE(resp->family);

        if (family == 2) {  // AF_INET
            struct sockaddr_in *sin = (struct sockaddr_in*)addr;
            memset(sin, 0, sizeof(*sin));
            sin->sin_family = AF_INET;
            sin->sin_port = resp->port;
            memcpy(&sin->sin_addr.s_addr, resp->addr_bytes, 4);
            *addrlen = sizeof(*sin);
        } else if (family == 10) {  // AF_INET6
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)addr;
            memset(sin6, 0, sizeof(*sin6));
            sin6->sin6_family = AF_INET6;
            sin6->sin6_port = resp->port;
            memcpy(&sin6->sin6_addr, resp->addr_bytes, 16);
            *addrlen = sizeof(*sin6);
        }
    } else {
        errno = GINT32_FROM_LE(resp->errno_val);
    }

    g_free(response);
    g_async_queue_unref(queue);
    g_mutex_lock(&proxy->lock);
    g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
    g_mutex_unlock(&proxy->lock);

    return ret;
}

int
proxy_getsockopt(GPosixSocketsProxy *proxy, int sockfd,
                 int level, int optname,
                 void *optval, socklen_t *optlen)
{
    g_return_val_if_fail(proxy != NULL, -1);
    g_return_val_if_fail(optval != NULL, -1);
    g_return_val_if_fail(optlen != NULL, -1);

    if (!proxy->connected) {
        errno = ENOTCONN;
        return -1;
    }

    guint32 callId = g_atomic_int_add(&proxy->next_call_id, 1);

    struct {
        MessageHeader header;
        gint32 fd;
        gint32 level;
        gint32 optname;
        gint32 optlen;
    } msg;

    msg.header.callId = GUINT32_TO_LE(callId);
    msg.header.function = GUINT32_TO_LE(MSG_GETSOCKOPT);
    msg.fd = GINT32_TO_LE(sockfd);
    msg.level = GINT32_TO_LE(level);
    msg.optname = GINT32_TO_LE(optname);
    msg.optlen = GINT32_TO_LE(*optlen);

    GAsyncQueue *queue = g_async_queue_new();
    g_mutex_lock(&proxy->lock);
    g_hash_table_insert(proxy->pending_calls, GUINT_TO_POINTER(callId), queue);
    g_mutex_unlock(&proxy->lock);

    EMSCRIPTEN_RESULT ws_result = emscripten_websocket_send_binary(proxy->ws, &msg, sizeof(msg));

    if (ws_result != EMSCRIPTEN_RESULT_SUCCESS) {
        g_async_queue_unref(queue);
        g_mutex_lock(&proxy->lock);
        g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
        g_mutex_unlock(&proxy->lock);
        errno = EIO;
        return -1;
    }

    guchar *response = g_async_queue_pop(queue);

    struct {
        MessageHeader header;
        gint32 result;
        gint32 errno_val;
        gint32 returned_optlen;
        guchar optval_data[];
    } *resp = (void*)response;

    int ret = GINT32_FROM_LE(resp->result);

    if (ret == 0) {
        gint32 returned_len = GINT32_FROM_LE(resp->returned_optlen);
        if (returned_len > 0 && (size_t)returned_len <= *optlen) {
            memcpy(optval, resp->optval_data, returned_len);
            *optlen = returned_len;
        }
    } else {
        errno = GINT32_FROM_LE(resp->errno_val);
    }

    g_free(response);
    g_async_queue_unref(queue);
    g_mutex_lock(&proxy->lock);
    g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
    g_mutex_unlock(&proxy->lock);

    return ret;
}

int
proxy_setsockopt(GPosixSocketsProxy *proxy, int sockfd,
                 int level, int optname,
                 const void *optval, socklen_t optlen)
{
    g_return_val_if_fail(proxy != NULL, -1);
    g_return_val_if_fail(optval != NULL || optlen == 0, -1);

    if (!proxy->connected) {
        errno = ENOTCONN;
        return -1;
    }

    guint32 callId = g_atomic_int_add(&proxy->next_call_id, 1);

    size_t msg_size = sizeof(MessageHeader) + 16 + optlen;
    guchar *msg = g_malloc(msg_size);

    MessageHeader *header = (MessageHeader*)msg;
    header->callId = GUINT32_TO_LE(callId);
    header->function = GUINT32_TO_LE(MSG_SETSOCKOPT);

    gint32 *fd_ptr = (gint32*)(msg + sizeof(MessageHeader));
    *fd_ptr = GINT32_TO_LE(sockfd);

    gint32 *level_ptr = (gint32*)(msg + sizeof(MessageHeader) + 4);
    *level_ptr = GINT32_TO_LE(level);

    gint32 *optname_ptr = (gint32*)(msg + sizeof(MessageHeader) + 8);
    *optname_ptr = GINT32_TO_LE(optname);

    gint32 *optlen_ptr = (gint32*)(msg + sizeof(MessageHeader) + 12);
    *optlen_ptr = GINT32_TO_LE(optlen);

    if (optlen > 0) {
        memcpy(msg + sizeof(MessageHeader) + 16, optval, optlen);
    }

    GAsyncQueue *queue = g_async_queue_new();
    g_mutex_lock(&proxy->lock);
    g_hash_table_insert(proxy->pending_calls, GUINT_TO_POINTER(callId), queue);
    g_mutex_unlock(&proxy->lock);

    EMSCRIPTEN_RESULT ws_result = emscripten_websocket_send_binary(proxy->ws, msg, msg_size);
    g_free(msg);

    if (ws_result != EMSCRIPTEN_RESULT_SUCCESS) {
        g_async_queue_unref(queue);
        g_mutex_lock(&proxy->lock);
        g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
        g_mutex_unlock(&proxy->lock);
        errno = EIO;
        return -1;
    }

    guchar *response = g_async_queue_pop(queue);

    struct {
        MessageHeader header;
        gint32 result;
        gint32 errno_val;
    } *resp = (void*)response;

    int ret = GINT32_FROM_LE(resp->result);
    if (ret < 0) {
        errno = GINT32_FROM_LE(resp->errno_val);
    }

    g_free(response);
    g_async_queue_unref(queue);
    g_mutex_lock(&proxy->lock);
    g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
    g_mutex_unlock(&proxy->lock);

    return ret;
}

int
proxy_getaddrinfo(GPosixSocketsProxy *proxy,
                  const char *node, const char *service,
                  const struct addrinfo *hints,
                  struct addrinfo **res)
{
    g_return_val_if_fail(proxy != NULL, EAI_SYSTEM);
    g_return_val_if_fail(node != NULL, EAI_NONAME);
    g_return_val_if_fail(res != NULL, EAI_SYSTEM);

    if (!proxy->connected) {
        errno = ENOTCONN;
        return EAI_SYSTEM;
    }

    guint32 callId = g_atomic_int_add(&proxy->next_call_id, 1);

    // Build GETADDRINFO message
    size_t node_len = strlen(node) + 1;
    size_t service_len = service ? strlen(service) + 1 : 1;
    size_t msg_size = sizeof(MessageHeader) + 4 + node_len + service_len;

    guchar *msg = g_malloc(msg_size);
    MessageHeader *header = (MessageHeader*)msg;
    header->callId = GUINT32_TO_LE(callId);
    header->function = GUINT32_TO_LE(MSG_GETADDRINFO);

    gint32 *hints_ptr = (gint32*)(msg + sizeof(MessageHeader));
    *hints_ptr = hints ? GINT32_TO_LE(hints->ai_flags) : 0;

    memcpy(msg + sizeof(MessageHeader) + 4, node, node_len);
    if (service) {
        memcpy(msg + sizeof(MessageHeader) + 4 + node_len, service, service_len);
    } else {
        msg[sizeof(MessageHeader) + 4 + node_len] = 0;
    }

    GAsyncQueue *queue = g_async_queue_new();
    g_mutex_lock(&proxy->lock);
    g_hash_table_insert(proxy->pending_calls, GUINT_TO_POINTER(callId), queue);
    g_mutex_unlock(&proxy->lock);

    EMSCRIPTEN_RESULT ws_result = emscripten_websocket_send_binary(proxy->ws, msg, msg_size);
    g_free(msg);

    if (ws_result != EMSCRIPTEN_RESULT_SUCCESS) {
        g_async_queue_unref(queue);
        g_mutex_lock(&proxy->lock);
        g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
        g_mutex_unlock(&proxy->lock);
        errno = EIO;
        return EAI_SYSTEM;
    }

    guchar *response = g_async_queue_pop(queue);

    // Parse addrinfo response
    struct {
        MessageHeader header;
        gint32 result;
        gint32 num_addrs;
    } *resp = (void*)response;

    int ret = GINT32_FROM_LE(resp->result);

    if (ret == 0) {
        // Success - parse addresses
        int num_addrs = GINT32_FROM_LE(resp->num_addrs);
        struct addrinfo *head = NULL, *prev = NULL;

        guchar *addr_data = response + sizeof(*resp);

        for (int i = 0; i < num_addrs; i++) {
            struct {
                gint32 family;
                guint16 port;
                guint8 addr[16];
            } *addr_info = (void*)(addr_data + i * 22);

            struct addrinfo *ai = g_new0(struct addrinfo, 1);
            ai->ai_family = GINT32_FROM_LE(addr_info->family);
            ai->ai_socktype = SOCK_STREAM;
            ai->ai_protocol = 0;

            if (ai->ai_family == AF_INET) {
                struct sockaddr_in *sin = g_new0(struct sockaddr_in, 1);
                sin->sin_family = AF_INET;
                sin->sin_port = addr_info->port;
                memcpy(&sin->sin_addr, addr_info->addr, 4);
                ai->ai_addr = (struct sockaddr*)sin;
                ai->ai_addrlen = sizeof(*sin);
            } else if (ai->ai_family == AF_INET6) {
                struct sockaddr_in6 *sin6 = g_new0(struct sockaddr_in6, 1);
                sin6->sin6_family = AF_INET6;
                sin6->sin6_port = addr_info->port;
                memcpy(&sin6->sin6_addr, addr_info->addr, 16);
                ai->ai_addr = (struct sockaddr*)sin6;
                ai->ai_addrlen = sizeof(*sin6);
            }

            if (prev) {
                prev->ai_next = ai;
            } else {
                head = ai;
            }
            prev = ai;
        }

        *res = head;
    }

    g_free(response);
    g_async_queue_unref(queue);
    g_mutex_lock(&proxy->lock);
    g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
    g_mutex_unlock(&proxy->lock);

    return ret;
}

void
proxy_freeaddrinfo(struct addrinfo *res)
{
    while (res) {
        struct addrinfo *next = res->ai_next;
        g_free(res->ai_addr);
        g_free(res);
        res = next;
    }
}

int
proxy_getnameinfo(GPosixSocketsProxy *proxy,
                  const struct sockaddr *addr, socklen_t addrlen,
                  char *host, socklen_t hostlen,
                  char *serv, socklen_t servlen, int flags)
{
    g_return_val_if_fail(proxy != NULL, EAI_SYSTEM);
    g_return_val_if_fail(addr != NULL, EAI_FAIL);

    if (!proxy->connected) {
        errno = ENOTCONN;
        return EAI_SYSTEM;
    }

    guint32 callId = g_atomic_int_add(&proxy->next_call_id, 1);

    struct {
        MessageHeader header;
        guint16 family;
        guint16 port;
        guint8 addr_bytes[16];
        gint32 flags;
    } msg = {0};

    msg.header.callId = GUINT32_TO_LE(callId);
    msg.header.function = GUINT32_TO_LE(MSG_GETNAMEINFO);
    msg.flags = GINT32_TO_LE(flags);

    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in*)addr;
        msg.family = GUINT16_TO_BE(2);
        msg.port = sin->sin_port;
        memcpy(msg.addr_bytes, &sin->sin_addr.s_addr, 4);
    } else if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)addr;
        msg.family = GUINT16_TO_BE(10);
        msg.port = sin6->sin6_port;
        memcpy(msg.addr_bytes, &sin6->sin6_addr, 16);
    } else {
        return EAI_FAMILY;
    }

    GAsyncQueue *queue = g_async_queue_new();
    g_mutex_lock(&proxy->lock);
    g_hash_table_insert(proxy->pending_calls, GUINT_TO_POINTER(callId), queue);
    g_mutex_unlock(&proxy->lock);

    EMSCRIPTEN_RESULT ws_result = emscripten_websocket_send_binary(proxy->ws, &msg, sizeof(msg));

    if (ws_result != EMSCRIPTEN_RESULT_SUCCESS) {
        g_async_queue_unref(queue);
        g_mutex_lock(&proxy->lock);
        g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
        g_mutex_unlock(&proxy->lock);
        errno = EIO;
        return EAI_SYSTEM;
    }

    guchar *response = g_async_queue_pop(queue);

    struct {
        MessageHeader header;
        gint32 result;
        char data[];  // hostname and service strings
    } *resp = (void*)response;

    int ret = GINT32_FROM_LE(resp->result);

    if (ret == 0) {
        // Parse hostname and service from response
        const char *hostname = resp->data;
        size_t hostname_len = strlen(hostname);
        const char *service_str = resp->data + hostname_len + 1;

        if (host && hostlen > 0) {
            g_strlcpy(host, hostname, hostlen);
        }

        if (serv && servlen > 0) {
            g_strlcpy(serv, service_str, servlen);
        }
    }

    g_free(response);
    g_async_queue_unref(queue);
    g_mutex_lock(&proxy->lock);
    g_hash_table_remove(proxy->pending_calls, GUINT_TO_POINTER(callId));
    g_mutex_unlock(&proxy->lock);

    return ret;
}

#endif /* __EMSCRIPTEN__ */
