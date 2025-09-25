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

executable(
  'glib-main',
  files('glib_wasm_module.c'),
  dependencies: [libglib_dep],
  c_args: glib_wasm_cargs,
  link_args: glib_main_link_args,
  install: true,
  install_dir: get_option('bindir'),
)

glib_side_link_args = [
  '-sSIDE_MODULE=2',
  '-fPIC',
  '-O3',
  '-flto',
  '-sEXPORTED_FUNCTIONS=["_glib_wasm_version"]',
]

shared_module(
  'glib-side',
  files('glib_wasm_side.c'),
  dependencies: [libglib_dep],
  name_prefix: '',
  c_args: glib_wasm_cargs,
  link_args: glib_side_link_args,
  install: true,
  install_dir: get_option('libdir'),
)

