glib_wasm_cargs = ['-msimd128', '-DGLIB_WASM=1']

glib_main_link_args = [
  '-sWASM=1',
  '-sMODULARIZE=1',
  '-sEXPORT_ES6=1',
  '-sEXPORT_NAME=GLibModule',
  '-sALLOW_MEMORY_GROWTH=1',
  '-sENVIRONMENT=web,webview,worker',
  '-sNO_FILESYSTEM=1',
  '-sEXPORTED_RUNTIME_METHODS=["cwrap","ccall","UTF8ToString"]',
  '-O3',
  '-flto',
]

# MAIN_MODULE with WASM-specific sources
glib_wasm_main_sources = files(
  'glib_wasm_module.c',
  'gobject_wasm_unified_memory.c',
  'web_native_capabilities.c',
)

executable(
  'glib-main',
  glib_wasm_main_sources,
  dependencies: [libglib_dep, libgobject_dep],
  c_args: glib_wasm_cargs + ['-DGOBJECT_MAIN_MODULE=1'],
  link_args: glib_main_link_args,
  install: true,
  install_dir: get_option('bindir'),
)

glib_side_link_args = [
  '-sSIDE_MODULE=2',
  '-fPIC',
  '-O3',
  '-flto',
  '-sERROR_ON_UNDEFINED_SYMBOLS=0',  # Host provides dependencies
  '-sEXPORTED_FUNCTIONS=["_glib_wasm_version"]',
]

# SIDE_MODULE with minimal sources (imports from MAIN)
glib_wasm_side_sources = files(
  'glib_wasm_side.c',
)

shared_module(
  'glib-side',
  glib_wasm_side_sources,
  dependencies: [libglib_dep],
  name_prefix: '',
  name_suffix: 'wasm',
  c_args: glib_wasm_cargs + ['-DGOBJECT_SIDE_MODULE=1', '-DBUILD_SIDE_MODULE=1'],
  link_args: glib_side_link_args,
  install: true,
  install_dir: get_option('libdir'),
)

