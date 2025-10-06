/* GObject WASM Dynamic Loading Architecture
 * Copyright © 2025 Superstruct Ltd
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This file defines the infrastructure for GObject SIDE_MODULE support,
 * enabling unified type registry and memory management across dynamically
 * loaded WASM modules while maintaining 100% upstream API compatibility.
 */

#ifndef __GOBJECT_WASM_H__
#define __GOBJECT_WASM_H__

#include <glib.h>
#include <gobject/gtype.h>

G_BEGIN_DECLS

/* Build configuration detection */
#if defined(GLIB_WASM_SIDE_MODULE) || defined(BUILD_SIDE_MODULE)
  #define GOBJECT_SIDE_MODULE 1
  #define GOBJECT_MAIN_MODULE 0
#else
  #define GOBJECT_SIDE_MODULE 0
  #define GOBJECT_MAIN_MODULE 1
#endif

/* Architecture: Unified Type Registry
 *
 * MAIN_MODULE (discere-concha.wasm):
 *   - Provides single authoritative type registry
 *   - Exports all GType system functions
 *   - Manages global type state
 *
 * SIDE_MODULE (gtk-side.wasm, etc.):
 *   - Imports all GType functions from MAIN_MODULE
 *   - No local type registry
 *   - Types registered via host functions
 */

#if GOBJECT_SIDE_MODULE

/* SIDE_MODULE: Import all GType functions from host
 * These declarations tell the linker to expect these symbols
 * to be provided by the MAIN_MODULE at runtime via dlopen()
 */

/* Type registration - imported from MAIN_MODULE */
extern GType g_type_register_static (GType parent_type,
                                      const gchar *type_name,
                                      const GTypeInfo *info,
                                      GTypeFlags flags);

extern GType g_type_register_fundamental (GType type_id,
                                           const gchar *type_name,
                                           const GTypeInfo *info,
                                           const GTypeFundamentalInfo *finfo,
                                           GTypeFlags flags);

/* Class management - imported from MAIN_MODULE */
extern gpointer g_type_class_ref (GType type);
extern void g_type_class_unref (gpointer g_class);
extern gpointer g_type_class_peek (GType type);
extern gpointer g_type_class_peek_static (GType type);

/* Instance management - imported from MAIN_MODULE */
extern GTypeInstance* g_type_create_instance (GType type);
extern void g_type_free_instance (GTypeInstance *instance);

/* Type queries - imported from MAIN_MODULE */
extern GType g_type_from_name (const gchar *name);
extern const gchar* g_type_name (GType type);
extern GType g_type_parent (GType type);
extern guint g_type_depth (GType type);
extern GType* g_type_children (GType type, guint *n_children);
extern GType* g_type_interfaces (GType type, guint *n_interfaces);

/* Type checks - imported from MAIN_MODULE */
extern gboolean g_type_is_a (GType type, GType is_a_type);
extern gboolean g_type_check_instance_is_a (GTypeInstance *instance, GType iface_type);
extern gboolean g_type_check_class_is_a (GTypeClass *g_class, GType is_a_type);

/* Interface management - imported from MAIN_MODULE */
extern void g_type_add_interface_static (GType instance_type,
                                          GType interface_type,
                                          const GInterfaceInfo *info);

extern gpointer g_type_interface_peek (gpointer instance_class,
                                        GType iface_type);

/* Value management - imported from MAIN_MODULE */
extern void g_value_init (GValue *value, GType g_type);
extern void g_value_unset (GValue *value);
extern void g_value_copy (const GValue *src_value, GValue *dest_value);

/* Module tracking - SIDE_MODULE specific */
extern const gchar* g_wasm_get_current_module_name (void);
extern void g_wasm_set_current_module_name (const gchar *name);

#endif /* GOBJECT_SIDE_MODULE */


/* Architecture: Unified Memory Management
 *
 * All GObject allocations must use the same allocator to prevent:
 * - Double frees across module boundaries
 * - Memory leaks from mismatched allocators
 * - Heap corruption from wrong deallocator
 */

#if GOBJECT_SIDE_MODULE

/* Memory functions imported from MAIN_MODULE */
extern void* g_malloc (gsize n_bytes);
extern void* g_malloc0 (gsize n_bytes);
extern void* g_realloc (gpointer mem, gsize n_bytes);
extern void g_free (gpointer mem);
extern gchar* g_strdup (const gchar *str);
extern gchar* g_strndup (const gchar *str, gsize n);
extern gpointer g_memdup (gconstpointer mem, guint byte_size);
extern void* g_try_malloc (gsize n_bytes);
extern void* g_try_realloc (gpointer mem, gsize n_bytes);

/* Slice allocator imported from MAIN_MODULE */
extern gpointer g_slice_alloc (gsize block_size);
extern gpointer g_slice_alloc0 (gsize block_size);
extern void g_slice_free1 (gsize block_size, gpointer mem_block);

#endif /* GOBJECT_SIDE_MODULE */


/* Cross-Module Signal System
 *
 * Signals can be connected across module boundaries:
 * - Callback in app-side.wasm
 * - Object in gtk-side.wasm
 * - Signal emission crosses WASM module boundary
 */

typedef struct _GWasmClosure GWasmClosure;

struct _GWasmClosure {
  GClosure closure;

  GCallback callback;
  gpointer user_data;
  GClosureNotify destroy_notify;

  const gchar *source_module;  /* Module that connected signal */
  const gchar *target_module;  /* Module that owns object */
};

/* Create cross-module closure */
GClosure* g_wasm_closure_new (GCallback callback,
                               gpointer user_data,
                               GClosureNotify destroy_notify,
                               const gchar *source_module,
                               const gchar *target_module);

/* GTypeModule WASM extensions
 *
 * Standard GTypeModule interface enhanced internally for WASM.
 * Public API remains unchanged - WASM support is transparent.
 */

typedef struct _GWasmTypeModule GWasmTypeModule;
typedef struct _GWasmTypeModuleClass GWasmTypeModuleClass;

#define G_TYPE_WASM_TYPE_MODULE            (g_wasm_type_module_get_type ())
#define G_WASM_TYPE_MODULE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_WASM_TYPE_MODULE, GWasmTypeModule))
#define G_WASM_TYPE_MODULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), G_TYPE_WASM_TYPE_MODULE, GWasmTypeModuleClass))
#define G_IS_WASM_TYPE_MODULE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_WASM_TYPE_MODULE))

struct _GWasmTypeModule {
  GTypeModule parent;

  /*< private >*/
  gchar *module_name;
  gchar *module_url;         /* CDN URL or file path */
  void *wasm_instance;       /* WebAssembly.Instance handle */
  GHashTable *exports;       /* Symbol name -> function pointer */
  GHashTable *registered_types;  /* Types registered by this module */
  GPtrArray *dependencies;   /* Array of GWasmTypeModule* */
  guint load_count;
};

struct _GWasmTypeModuleClass {
  GTypeModuleClass parent_class;
};

GType g_wasm_type_module_get_type (void) G_GNUC_CONST;

GWasmTypeModule* g_wasm_type_module_new (const gchar *name,
                                          const gchar *url);

void g_wasm_type_module_add_dependency (GWasmTypeModule *module,
                                         GWasmTypeModule *dependency);

/* GModule WASM dynamic loading
 *
 * Standard GModule API works with both .so and .wasm files.
 * WASM modules detected by URL pattern or .wasm extension.
 */

#if GOBJECT_MAIN_MODULE

/* Export these for use by SIDE_MODULEs */
#define GOBJECT_WASM_EXPORT __attribute__((visibility("default"))) __attribute__((used))

#else

#define GOBJECT_WASM_EXPORT

#endif

/* Validation helpers for testing */

typedef struct {
  gboolean type_identity_preserved;     /* Same GType across modules */
  gboolean memory_allocator_unified;    /* Single allocator */
  gboolean signals_work_cross_module;   /* Cross-module signal emission */
  gboolean no_memory_leaks;             /* Clean shutdown */
  const gchar *error_message;
} GWasmValidationResult;

GWasmValidationResult* g_wasm_validate_architecture (void);

void g_wasm_validation_result_free (GWasmValidationResult *result);

G_END_DECLS

#endif /* __GOBJECT_WASM_H__ */
