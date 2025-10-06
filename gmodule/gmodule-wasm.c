/* GMODULE - WASM-enhanced dynamic module loading
 * Copyright © 2025 Superstruct Ltd
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Enhanced GModule implementation for WASM with:
 * - CDN URL support (https://wasm.discere.cloud/...)
 * - SIDE_MODULE automatic detection and loading
 * - WebAssembly.Instance management
 * - Standard GModule API compatibility
 */

#include "config.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <dlfcn.h>
#endif

#include <glib.h>
#include "gmodule.h"

#ifdef __EMSCRIPTEN__

/* WASM module cache for CDN-loaded modules */
static GHashTable *wasm_module_cache = NULL;
static GMutex wasm_module_mutex;

typedef struct {
  gchar *url;
  void *handle;
  GBytes *wasm_bytes;
} WasmModuleInfo;

static void
wasm_module_info_free (WasmModuleInfo *info)
{
  g_free (info->url);
  if (info->wasm_bytes)
    g_bytes_unref (info->wasm_bytes);
  g_free (info);
}

static void
init_wasm_module_cache (void)
{
  g_mutex_lock (&wasm_module_mutex);
  if (!wasm_module_cache)
    {
      wasm_module_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                   g_free,
                                                   (GDestroyNotify) wasm_module_info_free);
    }
  g_mutex_unlock (&wasm_module_mutex);
}

static gboolean
is_wasm_url (const gchar *file_name)
{
  if (!file_name)
    return FALSE;

  /* Check for URL patterns */
  if (g_str_has_prefix (file_name, "https://") ||
      g_str_has_prefix (file_name, "http://") ||
      g_str_has_prefix (file_name, "wasm://"))
    return TRUE;

  /* Check for .wasm extension */
  if (g_str_has_suffix (file_name, ".wasm"))
    return TRUE;

  return FALSE;
}

static GBytes*
fetch_wasm_from_url (const gchar *url,
                      GError **error)
{
  /* Use Emscripten's fetch API to download WASM module */
  gsize size = 0;
  guint8 *data = NULL;

  EM_ASM({
    const url = UTF8ToString($0);

    // Synchronous fetch using Emscripten's fetch API
    const xhr = new XMLHttpRequest();
    xhr.open('GET', url, false);  // Synchronous
    xhr.responseType = 'arraybuffer';

    try {
      xhr.send(null);

      if (xhr.status === 200) {
        const bytes = new Uint8Array(xhr.response);
        const size = bytes.length;
        const ptr = _malloc(size);
        HEAPU8.set(bytes, ptr);

        setValue($1, ptr, 'i32');
        setValue($2, size, 'i32');
      } else {
        setValue($1, 0, 'i32');
        setValue($2, 0, 'i32');
      }
    } catch (e) {
      console.error('Failed to fetch WASM module:', e);
      setValue($1, 0, 'i32');
      setValue($2, 0, 'i32');
    }
  }, url, &data, &size);

  if (!data || size == 0)
    {
      g_set_error (error, G_MODULE_ERROR, G_MODULE_ERROR_FAILED,
                   "Failed to fetch WASM module from %s", url);
      return NULL;
    }

  return g_bytes_new_take (data, size);
}

static gpointer
load_wasm_side_module (const gchar *file_name,
                        gboolean bind_lazy,
                        GError **error)
{
  init_wasm_module_cache ();

  /* Check cache first */
  g_mutex_lock (&wasm_module_mutex);
  WasmModuleInfo *cached = g_hash_table_lookup (wasm_module_cache, file_name);
  if (cached)
    {
      g_mutex_unlock (&wasm_module_mutex);
      return cached->handle;
    }
  g_mutex_unlock (&wasm_module_mutex);

  /* Fetch WASM bytes */
  GBytes *wasm_bytes = fetch_wasm_from_url (file_name, error);
  if (!wasm_bytes)
    return NULL;

  gconstpointer wasm_data;
  gsize wasm_size;
  wasm_data = g_bytes_get_data (wasm_bytes, &wasm_size);

  /* Load SIDE_MODULE using Emscripten's dlopen with memory */
  void *handle = NULL;

  EM_ASM({
    const wasmBytes = new Uint8Array(HEAPU8.buffer, $0, $1);
    const flags = $2;

    try {
      // Create a temporary file in memory filesystem
      const filename = '/tmp/module_' + Date.now() + '.wasm';
      FS.writeFile(filename, wasmBytes);

      // Load using dlopen (Emscripten's SIDE_MODULE loader)
      const handle = _dlopen(
        allocateUTF8OnStack(filename),
        flags
      );

      // Clean up temp file
      FS.unlink(filename);

      setValue($3, handle, 'i32');
    } catch (e) {
      console.error('Failed to load WASM SIDE_MODULE:', e);
      setValue($3, 0, 'i32');
    }
  }, wasm_data, wasm_size,
     (bind_lazy ? RTLD_LAZY : RTLD_NOW) | RTLD_GLOBAL,
     &handle);

  if (!handle)
    {
      g_bytes_unref (wasm_bytes);
      g_set_error (error, G_MODULE_ERROR, G_MODULE_ERROR_FAILED,
                   "Failed to instantiate WASM module: %s", dlerror ());
      return NULL;
    }

  /* Cache the loaded module */
  WasmModuleInfo *info = g_new0 (WasmModuleInfo, 1);
  info->url = g_strdup (file_name);
  info->handle = handle;
  info->wasm_bytes = wasm_bytes;

  g_mutex_lock (&wasm_module_mutex);
  g_hash_table_insert (wasm_module_cache, g_strdup (file_name), info);
  g_mutex_unlock (&wasm_module_mutex);

  return handle;
}

/* Enhanced _g_module_open for WASM */
static gpointer
_g_module_open (const gchar *file_name,
                gboolean bind_lazy,
                gboolean bind_local,
                GError **error)
{
  /* Check if this is a WASM URL */
  if (is_wasm_url (file_name))
    {
      return load_wasm_side_module (file_name, bind_lazy, error);
    }

  /* Fall back to standard dlopen for local files */
  gpointer handle;

  handle = dlopen (file_name,
                   (bind_local ? RTLD_LOCAL : RTLD_GLOBAL) |
                   (bind_lazy ? RTLD_LAZY : RTLD_NOW));

  if (!handle)
    {
      const gchar *message = dlerror ();
      g_module_set_error (message);
      g_set_error_literal (error, G_MODULE_ERROR, G_MODULE_ERROR_FAILED, message);
    }

  return handle;
}

static gpointer
_g_module_self (void)
{
  /* For WASM, return the main module handle */
  return dlopen (NULL, RTLD_GLOBAL | RTLD_LAZY);
}

static void
_g_module_close (gpointer handle)
{
  /* Check if this is a cached WASM module */
  gboolean is_wasm = FALSE;

  g_mutex_lock (&wasm_module_mutex);
  if (wasm_module_cache)
    {
      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init (&iter, wasm_module_cache);

      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          WasmModuleInfo *info = value;
          if (info->handle == handle)
            {
              is_wasm = TRUE;
              break;
            }
        }
    }
  g_mutex_unlock (&wasm_module_mutex);

  if (!is_wasm)
    {
      /* Standard dlclose for non-WASM modules */
      if (dlclose (handle) != 0)
        g_module_set_error (dlerror ());
    }
  /* WASM modules stay cached - actual cleanup on process exit */
}

static gpointer
_g_module_symbol (gpointer handle,
                  const gchar *symbol_name)
{
  gpointer p;
  const gchar *msg;

  /* Clear any previous error */
  dlerror ();

  /* dlsym works for both regular and WASM modules */
  p = dlsym (handle, symbol_name);

  msg = dlerror ();
  if (msg)
    g_module_set_error (msg);

  return p;
}

/* Cleanup on exit */
static void __attribute__((destructor))
cleanup_wasm_modules (void)
{
  if (wasm_module_cache)
    {
      g_hash_table_destroy (wasm_module_cache);
      wasm_module_cache = NULL;
    }
}

#else /* !__EMSCRIPTEN__ */

/* Non-WASM builds: use standard implementations from gmodule-dl.c */
#error "This file should only be compiled for Emscripten/WASM builds"

#endif /* __EMSCRIPTEN__ */
