/* GLib WASM - Web-Native Capability Detection Header
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __G_WEB_NATIVE_CAPABILITIES_H__
#define __G_WEB_NATIVE_CAPABILITIES_H__

#include <glib.h>

G_BEGIN_DECLS

// Web capability detection structure
typedef struct {
    gboolean has_opfs;
    gboolean has_shared_array_buffer;
    gboolean has_wasm_simd;
    gboolean has_web_crypto;
    gboolean has_intl_apis;
    gboolean has_fetch_api;
    gboolean has_web_workers;
    gboolean is_deno_runtime;
    gboolean is_chrome_based;
    gint chrome_version;
    gboolean has_pthread_support;
    gboolean has_proxy_to_pthread;
    gboolean has_optimized_workers;
    gint max_worker_threads;
} GWebCapabilities;

// Function declarations
const GWebCapabilities* g_web_get_capabilities(void);
gboolean g_web_is_modern_browser(void);
gboolean g_web_has_optimized_threading(void);
gboolean g_web_has_threading_support(void);
void g_web_detect_capabilities(void);

G_END_DECLS

#endif /* __G_WEB_NATIVE_CAPABILITIES_H__ */