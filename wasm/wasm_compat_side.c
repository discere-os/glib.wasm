/* GLib WASM - SIDE_MODULE compatibility layer
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * SIDE_MODULE compatibility layer per Emscripten dynamic linking docs.
 * System functions are provided by host MAIN_MODULE.
 * Only GLib-specific globals and integration stubs are included.
 */

#ifdef GLIB_WASM_SIDE_MODULE

#include <stddef.h>

/* GLib type definitions */
typedef int gboolean;
typedef char gchar;
#define FALSE 0
#define TRUE 1

/* GLib global variables are provided by the host MAIN_MODULE
 * These symbols are already defined in glib-init.c and linked from the host.
 * SIDE_MODULE should not redefine them to avoid duplicate symbol errors.
 */

/* Locale and charset functions - GLib-specific implementations */
const char **_g_locale_get_charset_aliases(void) {
    static const char *aliases[] = { NULL };
    return aliases;
}

const char *_g_locale_charset_raw(void) {
    return "UTF-8";
}

const char *_g_locale_charset_unalias(const char *charset) {
    return charset ? charset : "UTF-8";
}

/* WebGPU/Browser integration stubs - GLib WASM specific */
int g_webgpu_initialize(void *config) {
    return 1;  /* Handled by host application */
}

void g_webgpu_cleanup(void) {
    /* Handled by host application */
}

void g_mainloop_browser_cleanup(void) {
    /* Handled by host application */
}

void g_threading_wasm_cleanup(void) {
    /* Handled by host application */
}

void g_filesystem_opfs_cleanup(void) {
    /* Handled by host application */
}

void g_timing_webgpu_cleanup(void) {
    /* Handled by host application */
}

#endif /* GLIB_WASM_SIDE_MODULE */