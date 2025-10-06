/* GObject WASM - Export Definitions for MAIN_MODULE
 * Copyright © 2025 Superstruct Ltd
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This file defines all symbols that must be exported from the MAIN_MODULE
 * (discere-concha.wasm) for use by SIDE_MODULEs (gtk-side.wasm, etc.)
 *
 * Architecture:
 * - MAIN_MODULE exports ~150 GType functions + memory allocators
 * - SIDE_MODULEs import these symbols via Emscripten dlopen()
 * - Result: Single unified type registry and memory allocator
 */

#ifndef __GOBJECT_WASM_EXPORTS_H__
#define __GOBJECT_WASM_EXPORTS_H__

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

/* Export macro for MAIN_MODULE functions */
#define GOBJECT_EXPORT EMSCRIPTEN_KEEPALIVE

/* GType System Exports - Core type registration and management
 * These are the foundation of the unified type registry
 */

/* Type Registration */
GOBJECT_EXPORT GType g_type_register_static (GType parent_type,
                                               const gchar *type_name,
                                               const GTypeInfo *info,
                                               GTypeFlags flags);

GOBJECT_EXPORT GType g_type_register_fundamental (GType type_id,
                                                    const gchar *type_name,
                                                    const GTypeInfo *info,
                                                    const GTypeFundamentalInfo *finfo,
                                                    GTypeFlags flags);

GOBJECT_EXPORT GType g_type_register_static_simple (GType parent_type,
                                                      const gchar *type_name,
                                                      guint class_size,
                                                      GClassInitFunc class_init,
                                                      guint instance_size,
                                                      GInstanceInitFunc instance_init,
                                                      GTypeFlags flags);

/* Class Management */
GOBJECT_EXPORT gpointer g_type_class_ref (GType type);
GOBJECT_EXPORT void g_type_class_unref (gpointer g_class);
GOBJECT_EXPORT gpointer g_type_class_peek (GType type);
GOBJECT_EXPORT gpointer g_type_class_peek_static (GType type);
GOBJECT_EXPORT gpointer g_type_class_peek_parent (gpointer g_class);
GOBJECT_EXPORT void g_type_class_add_private (gpointer g_class, gsize private_size);

/* Instance Management */
GOBJECT_EXPORT GTypeInstance* g_type_create_instance (GType type);
GOBJECT_EXPORT void g_type_free_instance (GTypeInstance *instance);

/* Type Queries */
GOBJECT_EXPORT GType g_type_from_name (const gchar *name);
GOBJECT_EXPORT const gchar* g_type_name (GType type);
GOBJECT_EXPORT GQuark g_type_qname (GType type);
GOBJECT_EXPORT GType g_type_parent (GType type);
GOBJECT_EXPORT guint g_type_depth (GType type);
GOBJECT_EXPORT GType g_type_next_base (GType leaf_type, GType root_type);
GOBJECT_EXPORT GType* g_type_children (GType type, guint *n_children);
GOBJECT_EXPORT GType* g_type_interfaces (GType type, guint *n_interfaces);

/* Type Checks */
GOBJECT_EXPORT gboolean g_type_is_a (GType type, GType is_a_type);
GOBJECT_EXPORT gboolean g_type_check_instance_is_a (GTypeInstance *instance, GType iface_type);
GOBJECT_EXPORT gboolean g_type_check_class_is_a (GTypeClass *g_class, GType is_a_type);
GOBJECT_EXPORT gboolean g_type_check_instance (GTypeInstance *instance);
GOBJECT_EXPORT gboolean g_type_check_class_cast (GTypeClass *g_class, GType is_a_type);
GOBJECT_EXPORT gboolean g_type_check_instance_cast (GTypeInstance *instance, GType iface_type);
GOBJECT_EXPORT gboolean g_type_check_value (const GValue *value);
GOBJECT_EXPORT gboolean g_type_check_value_holds (const GValue *value, GType type);

/* Interface Management */
GOBJECT_EXPORT void g_type_add_interface_static (GType instance_type,
                                                   GType interface_type,
                                                   const GInterfaceInfo *info);
GOBJECT_EXPORT void g_type_add_interface_dynamic (GType instance_type,
                                                    GType interface_type,
                                                    GTypePlugin *plugin);
GOBJECT_EXPORT gpointer g_type_interface_peek (gpointer instance_class, GType iface_type);
GOBJECT_EXPORT gpointer g_type_interface_peek_parent (gpointer g_iface);
GOBJECT_EXPORT gpointer g_type_default_interface_ref (GType g_type);
GOBJECT_EXPORT void g_type_default_interface_unref (gpointer g_iface);
GOBJECT_EXPORT gpointer g_type_default_interface_peek (GType g_type);

/* GObject Exports - Object lifecycle and properties */

GOBJECT_EXPORT gpointer g_object_new (GType object_type, const gchar *first_property_name, ...);
GOBJECT_EXPORT gpointer g_object_newv (GType object_type, guint n_parameters, GParameter *parameters);
GOBJECT_EXPORT gpointer g_object_ref (gpointer object);
GOBJECT_EXPORT void g_object_unref (gpointer object);
GOBJECT_EXPORT gpointer g_object_ref_sink (gpointer object);

GOBJECT_EXPORT void g_object_set (gpointer object, const gchar *first_property_name, ...);
GOBJECT_EXPORT void g_object_get (gpointer object, const gchar *first_property_name, ...);
GOBJECT_EXPORT void g_object_set_property (GObject *object, const gchar *property_name, const GValue *value);
GOBJECT_EXPORT void g_object_get_property (GObject *object, const gchar *property_name, GValue *value);
GOBJECT_EXPORT void g_object_notify (GObject *object, const gchar *property_name);

GOBJECT_EXPORT void g_object_class_install_property (GObjectClass *oclass, guint property_id, GParamSpec *pspec);
GOBJECT_EXPORT GParamSpec* g_object_class_find_property (GObjectClass *oclass, const gchar *property_name);
GOBJECT_EXPORT GParamSpec** g_object_class_list_properties (GObjectClass *oclass, guint *n_properties);

/* Signal System Exports */

GOBJECT_EXPORT guint g_signal_new (const gchar *signal_name,
                                    GType itype,
                                    GSignalFlags signal_flags,
                                    guint class_offset,
                                    GSignalAccumulator accumulator,
                                    gpointer accu_data,
                                    GSignalCMarshaller c_marshaller,
                                    GType return_type,
                                    guint n_params,
                                    ...);

GOBJECT_EXPORT gulong g_signal_connect_data (gpointer instance,
                                               const gchar *detailed_signal,
                                               GCallback c_handler,
                                               gpointer data,
                                               GClosureNotify destroy_data,
                                               GConnectFlags connect_flags);

GOBJECT_EXPORT gulong g_signal_connect_closure (gpointer instance,
                                                  const gchar *detailed_signal,
                                                  GClosure *closure,
                                                  gboolean after);

GOBJECT_EXPORT void g_signal_emit (gpointer instance, guint signal_id, GQuark detail, ...);
GOBJECT_EXPORT void g_signal_emit_by_name (gpointer instance, const gchar *detailed_signal, ...);
GOBJECT_EXPORT void g_signal_emit_valist (gpointer instance, guint signal_id, GQuark detail, va_list var_args);

GOBJECT_EXPORT void g_signal_handler_disconnect (gpointer instance, gulong handler_id);
GOBJECT_EXPORT void g_signal_handler_block (gpointer instance, gulong handler_id);
GOBJECT_EXPORT void g_signal_handler_unblock (gpointer instance, gulong handler_id);
GOBJECT_EXPORT guint g_signal_handlers_disconnect_matched (gpointer instance,
                                                            GSignalMatchType mask,
                                                            guint signal_id,
                                                            GQuark detail,
                                                            GClosure *closure,
                                                            gpointer func,
                                                            gpointer data);

GOBJECT_EXPORT guint g_signal_lookup (const gchar *name, GType itype);
GOBJECT_EXPORT const gchar* g_signal_name (guint signal_id);

/* GValue Exports */

GOBJECT_EXPORT void g_value_init (GValue *value, GType g_type);
GOBJECT_EXPORT void g_value_unset (GValue *value);
GOBJECT_EXPORT void g_value_reset (GValue *value);
GOBJECT_EXPORT void g_value_copy (const GValue *src_value, GValue *dest_value);
GOBJECT_EXPORT gboolean g_value_transform (const GValue *src_value, GValue *dest_value);

GOBJECT_EXPORT void g_value_set_boolean (GValue *value, gboolean v_boolean);
GOBJECT_EXPORT gboolean g_value_get_boolean (const GValue *value);
GOBJECT_EXPORT void g_value_set_int (GValue *value, gint v_int);
GOBJECT_EXPORT gint g_value_get_int (const GValue *value);
GOBJECT_EXPORT void g_value_set_uint (GValue *value, guint v_uint);
GOBJECT_EXPORT guint g_value_get_uint (const GValue *value);
GOBJECT_EXPORT void g_value_set_long (GValue *value, glong v_long);
GOBJECT_EXPORT glong g_value_get_long (const GValue *value);
GOBJECT_EXPORT void g_value_set_ulong (GValue *value, gulong v_ulong);
GOBJECT_EXPORT gulong g_value_get_ulong (const GValue *value);
GOBJECT_EXPORT void g_value_set_int64 (GValue *value, gint64 v_int64);
GOBJECT_EXPORT gint64 g_value_get_int64 (const GValue *value);
GOBJECT_EXPORT void g_value_set_uint64 (GValue *value, guint64 v_uint64);
GOBJECT_EXPORT guint64 g_value_get_uint64 (const GValue *value);
GOBJECT_EXPORT void g_value_set_float (GValue *value, gfloat v_float);
GOBJECT_EXPORT gfloat g_value_get_float (const GValue *value);
GOBJECT_EXPORT void g_value_set_double (GValue *value, gdouble v_double);
GOBJECT_EXPORT gdouble g_value_get_double (const GValue *value);
GOBJECT_EXPORT void g_value_set_string (GValue *value, const gchar *v_string);
GOBJECT_EXPORT const gchar* g_value_get_string (const GValue *value);
GOBJECT_EXPORT void g_value_set_pointer (GValue *value, gpointer v_pointer);
GOBJECT_EXPORT gpointer g_value_get_pointer (const GValue *value);
GOBJECT_EXPORT void g_value_set_object (GValue *value, gpointer v_object);
GOBJECT_EXPORT gpointer g_value_get_object (const GValue *value);

/* Memory Management Exports - Unified allocator */

GOBJECT_EXPORT void* g_malloc (gsize n_bytes);
GOBJECT_EXPORT void* g_malloc0 (gsize n_bytes);
GOBJECT_EXPORT void* g_realloc (gpointer mem, gsize n_bytes);
GOBJECT_EXPORT void g_free (gpointer mem);
GOBJECT_EXPORT gchar* g_strdup (const gchar *str);
GOBJECT_EXPORT gchar* g_strndup (const gchar *str, gsize n);
GOBJECT_EXPORT gpointer g_memdup (gconstpointer mem, guint byte_size);
GOBJECT_EXPORT void* g_try_malloc (gsize n_bytes);
GOBJECT_EXPORT void* g_try_realloc (gpointer mem, gsize n_bytes);

/* Slice allocator */
GOBJECT_EXPORT gpointer g_slice_alloc (gsize block_size);
GOBJECT_EXPORT gpointer g_slice_alloc0 (gsize block_size);
GOBJECT_EXPORT void g_slice_free1 (gsize block_size, gpointer mem_block);

/* WASM-specific exports for module tracking */
GOBJECT_EXPORT const gchar* g_wasm_get_current_module_name (void);
GOBJECT_EXPORT void g_wasm_set_current_module_name (const gchar *name);

/* Unified memory tracking exports */
GOBJECT_EXPORT void g_wasm_memory_tracking_enable (void);
GOBJECT_EXPORT void g_wasm_memory_tracking_disable (void);
GOBJECT_EXPORT void g_wasm_memory_dump_leaks (void);

/* Closure system exports */
GOBJECT_EXPORT GClosure* g_closure_new_simple (guint sizeof_closure, gpointer data);
GOBJECT_EXPORT void g_closure_set_marshal (GClosure *closure, GClosureMarshal marshal);
GOBJECT_EXPORT void g_closure_add_finalize_notifier (GClosure *closure, gpointer notify_data, GClosureNotify notify_func);
GOBJECT_EXPORT void g_closure_invalidate (GClosure *closure);
GOBJECT_EXPORT void g_closure_ref (GClosure *closure);
GOBJECT_EXPORT void g_closure_unref (GClosure *closure);
GOBJECT_EXPORT void g_closure_sink (GClosure *closure);
GOBJECT_EXPORT void g_closure_invoke (GClosure *closure,
                                       GValue *return_value,
                                       guint n_param_values,
                                       const GValue *param_values,
                                       gpointer invocation_hint);

/* Generic marshaller (uses libffi) */
GOBJECT_EXPORT void g_cclosure_marshal_generic (GClosure *closure,
                                                 GValue *return_gvalue,
                                                 guint n_param_values,
                                                 const GValue *param_values,
                                                 gpointer invocation_hint,
                                                 gpointer marshal_data);

/* GModule dynamic loading exports */
GOBJECT_EXPORT GModule* g_module_open (const gchar *file_name, GModuleFlags flags);
GOBJECT_EXPORT gboolean g_module_close (GModule *module);
GOBJECT_EXPORT gboolean g_module_symbol (GModule *module, const gchar *symbol_name, gpointer *symbol);
GOBJECT_EXPORT const gchar* g_module_error (void);
GOBJECT_EXPORT gboolean g_module_supported (void);

#endif /* __EMSCRIPTEN__ */

#endif /* __GOBJECT_WASM_EXPORTS_H__ */
