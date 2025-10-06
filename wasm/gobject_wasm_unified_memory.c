/* GObject WASM - Unified Memory Management
 * Copyright © 2025 Superstruct Ltd
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This file implements unified memory allocation for cross-module safety.
 * All GObject allocations from SIDE_MODULEs are routed through the
 * MAIN_MODULE's allocator to prevent:
 * - Double frees across module boundaries
 * - Memory leaks from mismatched allocators
 * - Heap corruption from wrong deallocator
 */

#include <glib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#if !defined(GOBJECT_SIDE_MODULE) && !defined(BUILD_SIDE_MODULE)

/* MAIN_MODULE: Provide unified allocator with tracking */

typedef struct {
  void *ptr;
  gsize size;
  const char *file;
  int line;
  const char *module;
} AllocationRecord;

static GHashTable *allocation_tracker = NULL;
static GMutex allocation_mutex;
static gboolean tracking_enabled = FALSE;

void
g_wasm_memory_tracking_enable (void)
{
  g_mutex_lock (&allocation_mutex);
  if (!allocation_tracker)
    {
      allocation_tracker = g_hash_table_new_full (g_direct_hash,
                                                   g_direct_equal,
                                                   NULL,
                                                   g_free);
    }
  tracking_enabled = TRUE;
  g_mutex_unlock (&allocation_mutex);
}

void
g_wasm_memory_tracking_disable (void)
{
  g_mutex_lock (&allocation_mutex);
  tracking_enabled = FALSE;
  g_mutex_unlock (&allocation_mutex);
}

static void
track_allocation (void *ptr,
                  gsize size,
                  const char *file,
                  int line,
                  const char *module)
{
  if (!tracking_enabled || !allocation_tracker)
    return;

  g_mutex_lock (&allocation_mutex);

  AllocationRecord *record = g_new (AllocationRecord, 1);
  record->ptr = ptr;
  record->size = size;
  record->file = file;
  record->line = line;
  record->module = module;

  g_hash_table_insert (allocation_tracker, ptr, record);

  g_mutex_unlock (&allocation_mutex);
}

static void
track_deallocation (void *ptr,
                     const char *module)
{
  if (!tracking_enabled || !allocation_tracker)
    return;

  g_mutex_lock (&allocation_mutex);

  AllocationRecord *record = g_hash_table_lookup (allocation_tracker, ptr);
  if (!record)
    {
      g_warning ("Module %s freeing untracked pointer %p", module, ptr);
    }
  else if (g_strcmp0 (record->module, module) != 0)
    {
      g_warning ("Module %s freeing memory allocated by %s (%s:%d)",
                 module, record->module, record->file, record->line);
    }

  g_hash_table_remove (allocation_tracker, ptr);

  g_mutex_unlock (&allocation_mutex);
}

void
g_wasm_memory_dump_leaks (void)
{
  if (!allocation_tracker)
    return;

  g_mutex_lock (&allocation_mutex);

  guint count = g_hash_table_size (allocation_tracker);
  if (count > 0)
    {
      g_warning ("Memory leak detected: %u allocations not freed", count);

      GHashTableIter iter;
      gpointer key, value;
      g_hash_table_iter_init (&iter, allocation_tracker);

      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          AllocationRecord *record = value;
          g_warning ("  %p: %zu bytes allocated by %s at %s:%d",
                     record->ptr, record->size, record->module,
                     record->file, record->line);
        }
    }

  g_mutex_unlock (&allocation_mutex);
}

/* Export unified allocator functions for SIDE_MODULEs */

EMSCRIPTEN_KEEPALIVE void*
g_wasm_unified_malloc (gsize n_bytes,
                        const char *file,
                        int line,
                        const char *module)
{
  void *ptr = g_malloc (n_bytes);
  track_allocation (ptr, n_bytes, file, line, module);
  return ptr;
}

EMSCRIPTEN_KEEPALIVE void*
g_wasm_unified_malloc0 (gsize n_bytes,
                         const char *file,
                         int line,
                         const char *module)
{
  void *ptr = g_malloc0 (n_bytes);
  track_allocation (ptr, n_bytes, file, line, module);
  return ptr;
}

EMSCRIPTEN_KEEPALIVE void*
g_wasm_unified_realloc (gpointer mem,
                         gsize n_bytes,
                         const char *file,
                         int line,
                         const char *module)
{
  if (mem)
    track_deallocation (mem, module);

  void *ptr = g_realloc (mem, n_bytes);

  if (n_bytes > 0)
    track_allocation (ptr, n_bytes, file, line, module);

  return ptr;
}

EMSCRIPTEN_KEEPALIVE void
g_wasm_unified_free (gpointer mem,
                      const char *module)
{
  if (!mem)
    return;

  track_deallocation (mem, module);
  g_free (mem);
}

EMSCRIPTEN_KEEPALIVE gchar*
g_wasm_unified_strdup (const gchar *str,
                        const char *file,
                        int line,
                        const char *module)
{
  gchar *ptr = g_strdup (str);
  if (ptr)
    track_allocation (ptr, strlen (ptr) + 1, file, line, module);
  return ptr;
}

EMSCRIPTEN_KEEPALIVE gchar*
g_wasm_unified_strndup (const gchar *str,
                         gsize n,
                         const char *file,
                         int line,
                         const char *module)
{
  gchar *ptr = g_strndup (str, n);
  if (ptr)
    track_allocation (ptr, strlen (ptr) + 1, file, line, module);
  return ptr;
}

EMSCRIPTEN_KEEPALIVE gpointer
g_wasm_unified_memdup (gconstpointer mem,
                        guint byte_size,
                        const char *file,
                        int line,
                        const char *module)
{
  gpointer ptr = g_memdup (mem, byte_size);
  if (ptr)
    track_allocation (ptr, byte_size, file, line, module);
  return ptr;
}

/* Slice allocator (simpler - just forward to malloc) */

EMSCRIPTEN_KEEPALIVE gpointer
g_wasm_unified_slice_alloc (gsize block_size,
                             const char *file,
                             int line,
                             const char *module)
{
  void *ptr = g_slice_alloc (block_size);
  track_allocation (ptr, block_size, file, line, module);
  return ptr;
}

EMSCRIPTEN_KEEPALIVE gpointer
g_wasm_unified_slice_alloc0 (gsize block_size,
                              const char *file,
                              int line,
                              const char *module)
{
  void *ptr = g_slice_alloc0 (block_size);
  track_allocation (ptr, block_size, file, line, module);
  return ptr;
}

EMSCRIPTEN_KEEPALIVE void
g_wasm_unified_slice_free1 (gsize block_size,
                             gpointer mem_block,
                             const char *module)
{
  if (!mem_block)
    return;

  track_deallocation (mem_block, module);
  g_slice_free1 (block_size, mem_block);
}

#endif /* MAIN_MODULE */

#if defined(GOBJECT_SIDE_MODULE) || defined(BUILD_SIDE_MODULE)

/* SIDE_MODULE: Use unified allocator from MAIN_MODULE
 *
 * These macros redirect all allocation calls to the tracked versions
 * that automatically pass module name for leak detection.
 */

#define CURRENT_MODULE_NAME __FILE__

/* Import unified allocator functions from MAIN_MODULE */
extern void* g_wasm_unified_malloc (gsize n_bytes,
                                     const char *file,
                                     int line,
                                     const char *module);

extern void* g_wasm_unified_malloc0 (gsize n_bytes,
                                      const char *file,
                                      int line,
                                      const char *module);

extern void* g_wasm_unified_realloc (gpointer mem,
                                      gsize n_bytes,
                                      const char *file,
                                      int line,
                                      const char *module);

extern void g_wasm_unified_free (gpointer mem,
                                  const char *module);

extern gchar* g_wasm_unified_strdup (const gchar *str,
                                      const char *file,
                                      int line,
                                      const char *module);

extern gchar* g_wasm_unified_strndup (const gchar *str,
                                       gsize n,
                                       const char *file,
                                       int line,
                                       const char *module);

extern gpointer g_wasm_unified_memdup (gconstpointer mem,
                                        guint byte_size,
                                        const char *file,
                                        int line,
                                        const char *module);

extern gpointer g_wasm_unified_slice_alloc (gsize block_size,
                                             const char *file,
                                             int line,
                                             const char *module);

extern gpointer g_wasm_unified_slice_alloc0 (gsize block_size,
                                              const char *file,
                                              int line,
                                              const char *module);

extern void g_wasm_unified_slice_free1 (gsize block_size,
                                         gpointer mem_block,
                                         const char *module);

/* Override standard allocators to use unified versions */

#undef g_malloc
#define g_malloc(n) \
  g_wasm_unified_malloc(n, __FILE__, __LINE__, CURRENT_MODULE_NAME)

#undef g_malloc0
#define g_malloc0(n) \
  g_wasm_unified_malloc0(n, __FILE__, __LINE__, CURRENT_MODULE_NAME)

#undef g_realloc
#define g_realloc(p, n) \
  g_wasm_unified_realloc(p, n, __FILE__, __LINE__, CURRENT_MODULE_NAME)

#undef g_free
#define g_free(p) \
  g_wasm_unified_free(p, CURRENT_MODULE_NAME)

#undef g_strdup
#define g_strdup(s) \
  g_wasm_unified_strdup(s, __FILE__, __LINE__, CURRENT_MODULE_NAME)

#undef g_strndup
#define g_strndup(s, n) \
  g_wasm_unified_strndup(s, n, __FILE__, __LINE__, CURRENT_MODULE_NAME)

#undef g_memdup
#define g_memdup(m, s) \
  g_wasm_unified_memdup(m, s, __FILE__, __LINE__, CURRENT_MODULE_NAME)

#undef g_slice_alloc
#define g_slice_alloc(s) \
  g_wasm_unified_slice_alloc(s, __FILE__, __LINE__, CURRENT_MODULE_NAME)

#undef g_slice_alloc0
#define g_slice_alloc0(s) \
  g_wasm_unified_slice_alloc0(s, __FILE__, __LINE__, CURRENT_MODULE_NAME)

#undef g_slice_free1
#define g_slice_free1(s, p) \
  g_wasm_unified_slice_free1(s, p, CURRENT_MODULE_NAME)

#endif /* SIDE_MODULE */
