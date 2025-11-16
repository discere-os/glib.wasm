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

#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static void
test_socket_creation (void)
{
  GError *error = NULL;
  GSocket *sock;

  g_print ("Testing socket creation...\n");

  sock = g_socket_new (G_SOCKET_FAMILY_IPV4,
                       G_SOCKET_TYPE_STREAM,
                       G_SOCKET_PROTOCOL_TCP,
                       &error);

  g_assert_no_error (error);
  g_assert_nonnull (sock);

  g_print ("✓ Socket created successfully (fd=%d)\n",
           g_socket_get_fd (sock));

  g_object_unref (sock);
}

static void
test_dns_resolution (void)
{
  GError *error = NULL;
  GResolver *resolver;
  GList *addrs;

  g_print ("\nTesting DNS resolution...\n");

  resolver = g_resolver_get_default ();
  addrs = g_resolver_lookup_by_name (resolver, "cloudflare.com",
                                     NULL, &error);

  g_assert_no_error (error);
  g_assert_nonnull (addrs);
  g_assert_cmpint (g_list_length (addrs), >, 0);

  g_print ("✓ Resolved cloudflare.com to %u address(es)\n",
           g_list_length (addrs));

  for (GList *l = addrs; l != NULL; l = l->next)
    {
      GInetAddress *addr = G_INET_ADDRESS (l->data);
      gchar *addr_str = g_inet_address_to_string (addr);
      g_print ("  - %s\n", addr_str);
      g_free (addr_str);
    }

  g_list_free_full (addrs, g_object_unref);
  g_object_unref (resolver);
}

static void
test_tcp_connection (void)
{
  GError *error = NULL;
  GSocket *sock;
  GResolver *resolver;
  GList *addrs;
  GInetAddress *addr;
  GSocketAddress *sockaddr;
  gboolean connected;

  g_print ("\nTesting TCP connection...\n");

  /* Create socket */
  sock = g_socket_new (G_SOCKET_FAMILY_IPV4,
                       G_SOCKET_TYPE_STREAM,
                       G_SOCKET_PROTOCOL_TCP,
                       &error);
  g_assert_no_error (error);
  g_assert_nonnull (sock);

  /* Resolve address */
  resolver = g_resolver_get_default ();
  addrs = g_resolver_lookup_by_name (resolver, "example.com",
                                     NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (addrs);

  addr = G_INET_ADDRESS (addrs->data);
  sockaddr = g_inet_socket_address_new (addr, 80);

  g_print ("Connecting to example.com:80...\n");

  /* Connect */
  connected = g_socket_connect (sock, sockaddr, NULL, &error);

  g_assert_no_error (error);
  g_assert_true (connected);

  g_print ("✓ Connected successfully!\n");

  /* Cleanup */
  g_socket_close (sock, NULL);
  g_object_unref (sock);
  g_object_unref (sockaddr);
  g_list_free_full (addrs, g_object_unref);
  g_object_unref (resolver);
}

static void
test_http_request (void)
{
  GError *error = NULL;
  GSocket *sock;
  GResolver *resolver;
  GList *addrs;
  GInetAddress *addr;
  GSocketAddress *sockaddr;
  gboolean connected;
  const char *request;
  gssize sent, received;
  char response[4096];

  g_print ("\nTesting HTTP request...\n");

  /* Create socket */
  sock = g_socket_new (G_SOCKET_FAMILY_IPV4,
                       G_SOCKET_TYPE_STREAM,
                       G_SOCKET_PROTOCOL_TCP,
                       &error);
  g_assert_no_error (error);

  /* Resolve and connect */
  resolver = g_resolver_get_default ();
  addrs = g_resolver_lookup_by_name (resolver, "example.com",
                                     NULL, &error);
  g_assert_no_error (error);

  addr = G_INET_ADDRESS (addrs->data);
  sockaddr = g_inet_socket_address_new (addr, 80);

  connected = g_socket_connect (sock, sockaddr, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (connected);

  /* Send HTTP request */
  request = "GET / HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Connection: close\r\n"
            "\r\n";

  g_print ("Sending HTTP GET request...\n");

  sent = g_socket_send (sock, request, strlen (request), NULL, &error);

  g_assert_no_error (error);
  g_assert_cmpint (sent, ==, strlen (request));

  g_print ("✓ Sent %zd bytes\n", sent);

  /* Receive response */
  g_print ("Receiving HTTP response...\n");

  received = g_socket_receive (sock, response, sizeof (response) - 1,
                               NULL, &error);

  g_assert_no_error (error);
  g_assert_cmpint (received, >, 0);

  response[received] = '\0';

  g_print ("✓ Received %zd bytes\n", received);
  g_print ("\nFirst 200 characters of response:\n");
  g_print ("%.200s\n", response);

  /* Verify it's a valid HTTP response */
  g_assert_true (g_str_has_prefix (response, "HTTP/1.1 ") ||
                 g_str_has_prefix (response, "HTTP/1.0 "));

  /* Cleanup */
  g_socket_close (sock, NULL);
  g_object_unref (sock);
  g_object_unref (sockaddr);
  g_list_free_full (addrs, g_object_unref);
  g_object_unref (resolver);
}

static void
test_gsocketclient (void)
{
  GError *error = NULL;
  GSocketClient *client;
  GSocketConnection *conn;
  GOutputStream *output;
  GInputStream *input;
  const char *request;
  gsize written;
  char response[4096];
  gssize read_bytes;

  g_print ("\nTesting GSocketClient (high-level API)...\n");

  client = g_socket_client_new ();

  g_print ("Connecting to example.com:80...\n");

  conn = g_socket_client_connect_to_host (client, "example.com", 80,
                                          NULL, &error);

  g_assert_no_error (error);
  g_assert_nonnull (conn);

  g_print ("✓ Connected!\n");

  /* Get streams */
  output = g_io_stream_get_output_stream (G_IO_STREAM (conn));
  input = g_io_stream_get_input_stream (G_IO_STREAM (conn));

  /* Send HTTP request */
  request = "GET / HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Connection: close\r\n"
            "\r\n";

  g_print ("Sending HTTP GET request via streams...\n");

  g_output_stream_write_all (output, request, strlen (request),
                              &written, NULL, &error);

  g_assert_no_error (error);
  g_assert_cmpint (written, ==, strlen (request));

  g_print ("✓ Sent %zu bytes\n", written);

  /* Receive response */
  g_print ("Receiving HTTP response via streams...\n");

  read_bytes = g_input_stream_read (input, response, sizeof (response) - 1,
                                    NULL, &error);

  g_assert_no_error (error);
  g_assert_cmpint (read_bytes, >, 0);

  response[read_bytes] = '\0';

  g_print ("✓ Received %zd bytes\n", read_bytes);
  g_print ("\nFirst 200 characters of response:\n");
  g_print ("%.200s\n", response);

  /* Verify it's a valid HTTP response */
  g_assert_true (g_str_has_prefix (response, "HTTP/1.1 ") ||
                 g_str_has_prefix (response, "HTTP/1.0 "));

  /* Cleanup */
  g_io_stream_close (G_IO_STREAM (conn), NULL, NULL);
  g_object_unref (conn);
  g_object_unref (client);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_print ("\n");
  g_print ("==============================================\n");
  g_print ("  GIO POSIX Sockets Proxy Integration Test\n");
  g_print ("==============================================\n");
  g_print ("\n");

#ifdef __EMSCRIPTEN__
  const char *proxy_url = g_getenv ("POSIX_PROXY_URL");
  if (proxy_url == NULL)
    proxy_url = "wss://posix-proxy.discere.cloud/v1";

  g_print ("POSIX Proxy URL: %s\n\n", proxy_url);
#else
  g_print ("Running in native mode (not WASM)\n\n");
#endif

  /* Register tests */
  g_test_add_func ("/gio/proxy/socket-creation", test_socket_creation);
  g_test_add_func ("/gio/proxy/dns-resolution", test_dns_resolution);
  g_test_add_func ("/gio/proxy/tcp-connection", test_tcp_connection);
  g_test_add_func ("/gio/proxy/http-request", test_http_request);
  g_test_add_func ("/gio/proxy/gsocketclient", test_gsocketclient);

  int result = g_test_run ();

  if (result == 0)
    {
      g_print ("\n");
      g_print ("==============================================\n");
      g_print ("  ✅ All GIO proxy tests passed!\n");
      g_print ("==============================================\n");
      g_print ("\n");
    }

  return result;
}
