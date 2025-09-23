/*
 * GLib Enum Types - Minimal WASM implementation
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 * Licensed under LGPL-2.1-or-later
 *
 * Minimal enumtypes implementation for WebAssembly build
 */

#include "gobject/glib-enumtypes.h"
#include "gobject/gbinding.h"
#include "gobject/gsignal.h"

/* GBindingFlags */
GType
g_binding_flags_get_type (void)
{
  static GType etype = 0;
  if (G_UNLIKELY (etype == 0)) {
    static const GFlagsValue values[] = {
      { G_BINDING_DEFAULT, "G_BINDING_DEFAULT", "default" },
      { G_BINDING_BIDIRECTIONAL, "G_BINDING_BIDIRECTIONAL", "bidirectional" },
      { G_BINDING_SYNC_CREATE, "G_BINDING_SYNC_CREATE", "sync-create" },
      { G_BINDING_INVERT_BOOLEAN, "G_BINDING_INVERT_BOOLEAN", "invert-boolean" },
      { 0, NULL, NULL }
    };
    etype = g_flags_register_static ("GBindingFlags", values);
  }
  return etype;
}

/* GSignalFlags */
GType
g_signal_flags_get_type (void)
{
  static GType etype = 0;
  if (G_UNLIKELY (etype == 0)) {
    static const GFlagsValue values[] = {
      { G_SIGNAL_RUN_FIRST, "G_SIGNAL_RUN_FIRST", "run-first" },
      { G_SIGNAL_RUN_LAST, "G_SIGNAL_RUN_LAST", "run-last" },
      { G_SIGNAL_RUN_CLEANUP, "G_SIGNAL_RUN_CLEANUP", "run-cleanup" },
      { G_SIGNAL_NO_RECURSE, "G_SIGNAL_NO_RECURSE", "no-recurse" },
      { G_SIGNAL_DETAILED, "G_SIGNAL_DETAILED", "detailed" },
      { G_SIGNAL_ACTION, "G_SIGNAL_ACTION", "action" },
      { G_SIGNAL_NO_HOOKS, "G_SIGNAL_NO_HOOKS", "no-hooks" },
      { G_SIGNAL_MUST_COLLECT, "G_SIGNAL_MUST_COLLECT", "must-collect" },
      { G_SIGNAL_DEPRECATED, "G_SIGNAL_DEPRECATED", "deprecated" },
      { G_SIGNAL_ACCUMULATOR_FIRST_RUN, "G_SIGNAL_ACCUMULATOR_FIRST_RUN", "accumulator-first-run" },
      { 0, NULL, NULL }
    };
    etype = g_flags_register_static ("GSignalFlags", values);
  }
  return etype;
}

/* GConnectFlags */
GType
g_connect_flags_get_type (void)
{
  static GType etype = 0;
  if (G_UNLIKELY (etype == 0)) {
    static const GFlagsValue values[] = {
      { G_CONNECT_AFTER, "G_CONNECT_AFTER", "after" },
      { G_CONNECT_SWAPPED, "G_CONNECT_SWAPPED", "swapped" },
      { 0, NULL, NULL }
    };
    etype = g_flags_register_static ("GConnectFlags", values);
  }
  return etype;
}

/* GSignalMatchType */
GType
g_signal_match_type_get_type (void)
{
  static GType etype = 0;
  if (G_UNLIKELY (etype == 0)) {
    static const GFlagsValue values[] = {
      { G_SIGNAL_MATCH_ID, "G_SIGNAL_MATCH_ID", "id" },
      { G_SIGNAL_MATCH_DETAIL, "G_SIGNAL_MATCH_DETAIL", "detail" },
      { G_SIGNAL_MATCH_CLOSURE, "G_SIGNAL_MATCH_CLOSURE", "closure" },
      { G_SIGNAL_MATCH_FUNC, "G_SIGNAL_MATCH_FUNC", "func" },
      { G_SIGNAL_MATCH_DATA, "G_SIGNAL_MATCH_DATA", "data" },
      { G_SIGNAL_MATCH_UNBLOCKED, "G_SIGNAL_MATCH_UNBLOCKED", "unblocked" },
      { 0, NULL, NULL }
    };
    etype = g_flags_register_static ("GSignalMatchType", values);
  }
  return etype;
}