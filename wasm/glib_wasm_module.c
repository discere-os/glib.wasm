// Minimal GLib MAIN_MODULE wrapper for demos/tests
#include <emscripten.h>
#include <glib.h>

EMSCRIPTEN_KEEPALIVE
const char* glib_wasm_version(void) {
  static char buf[32];
  g_snprintf(buf, sizeof(buf), "%d.%d.%d", glib_major_version, glib_minor_version, glib_micro_version);
  return buf;
}

EMSCRIPTEN_KEEPALIVE
gboolean glib_wasm_str_has_prefix(const char* s, const char* prefix) {
  return g_str_has_prefix(s, prefix);
}

