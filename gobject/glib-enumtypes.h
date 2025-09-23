/*
 * GLib Enum Types - Minimal WASM implementation
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under LGPL-2.1-or-later
 *
 * Minimal enumtypes file for WebAssembly build
 */

#ifndef __GOBJECT_ENUM_TYPES_H__
#define __GOBJECT_ENUM_TYPES_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* GBindingFlags from gbinding.h */
GOBJECT_AVAILABLE_IN_ALL GType g_binding_flags_get_type (void) G_GNUC_CONST;
#define G_TYPE_BINDING_FLAGS (g_binding_flags_get_type ())

/* GSignalFlags from gsignal.h */
GOBJECT_AVAILABLE_IN_ALL GType g_signal_flags_get_type (void) G_GNUC_CONST;
#define G_TYPE_SIGNAL_FLAGS (g_signal_flags_get_type ())

/* GConnectFlags from gsignal.h */
GOBJECT_AVAILABLE_IN_ALL GType g_connect_flags_get_type (void) G_GNUC_CONST;
#define G_TYPE_CONNECT_FLAGS (g_connect_flags_get_type ())

/* GSignalMatchType from gsignal.h */
GOBJECT_AVAILABLE_IN_ALL GType g_signal_match_type_get_type (void) G_GNUC_CONST;
#define G_TYPE_SIGNAL_MATCH_TYPE (g_signal_match_type_get_type ())

G_END_DECLS

#endif /* __GOBJECT_ENUM_TYPES_H__ */