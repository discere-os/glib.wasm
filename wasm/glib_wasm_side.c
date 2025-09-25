// Minimal GLib SIDE_MODULE wrapper
#include <glib.h>

const char* glib_wasm_version(void) {
  static char buf[32];
  g_snprintf(buf, sizeof(buf), "%d.%d.%d", glib_major_version, glib_minor_version, glib_micro_version);
  return buf;
}

