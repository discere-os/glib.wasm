# GLib WASM Build Configuration
# Unified Meson-based build system following GTK.wasm pattern

# Base WASM compilation flags
wasm_common_flags = [
  '-fPIC',
  '-sWASM_BIGINT=1',
  '-DGLIB_WASM=1',
]

# SIMD flags (3-5x speedup for string operations)
wasm_simd_flags = get_option('wasm_simd') ? ['-msimd128'] : []

# Threading flags (10x faster than pthread emulation)
wasm_threading_flags = get_option('wasm_threading') ? [
  '-pthread',
  '-sPTHREAD_POOL_SIZE=4',
] : []

# Optimization flags based on strategy
if get_option('wasm_optimize') == 'size'
  wasm_opt_flags = ['-Os', '-flto']
elif get_option('wasm_optimize') == 'speed'
  wasm_opt_flags = ['-O3', '-flto', '-ffast-math']
else  # balanced
  wasm_opt_flags = ['-O3', '-flto']
endif

# Build type specific flags
if get_option('wasm_build_type') == 'minimal'
  wasm_memory_flags = [
    '-sINITIAL_MEMORY=67108864',  # 64MB
    '-sSTACK_SIZE=2097152',       # 2MB
  ]
  wasm_features = []
elif get_option('wasm_build_type') == 'webgpu'
  wasm_memory_flags = [
    '-sINITIAL_MEMORY=134217728',  # 128MB
    '-sMAXIMUM_MEMORY=1073741824', # 1GB
    '-sSTACK_SIZE=5242880',        # 5MB
  ]
  wasm_features = [
    '-sUSE_WEBGPU=1',
    '-sASYNCIFY=1',
  ]
else  # standard
  wasm_memory_flags = [
    '-sINITIAL_MEMORY=134217728',  # 128MB
    '-sMAXIMUM_MEMORY=1073741824', # 1GB
    '-sSTACK_SIZE=5242880',        # 5MB
  ]
  wasm_features = []
endif

# Combine all C args for WASM builds
glib_wasm_cargs = wasm_common_flags + wasm_simd_flags + wasm_threading_flags

# Web-native component sources
web_native_sources = files(
  'web_native_capabilities.c',
  'web_native_simd_strings.c',
  'web_native_crypto.c',
  'web_native_threading.c',
  'web_native_networking.c',
  'web_native_filesystem.c',
  'web_native_memory.c',
  'web_native_mainloop.c',
)

# WASM wrapper sources
wasm_wrapper_sources = files(
  'glib_wasm_wrapper.c',
  'wasm_compat.c',
)

# MAIN_MODULE build (testing/NPM distribution)
glib_main_link_args = [
  '-sWASM=1',
  '-sMODULARIZE=1',
  '-sEXPORT_ES6=1',
  '-sEXPORT_NAME=GLibModule',
  '-sALLOW_MEMORY_GROWTH=1',
  '-sENVIRONMENT=web,webview,worker',
  '-sNO_FILESYSTEM=1',
  '-sEXPORTED_RUNTIME_METHODS=["cwrap","ccall","UTF8ToString","HEAPU8","FS"]',
] + wasm_opt_flags + wasm_memory_flags + wasm_features + wasm_threading_flags

# MAIN_MODULE with WASM-specific sources
glib_wasm_main_sources = files(
  'glib_wasm_module.c',
  'gobject_wasm_unified_memory.c',
) + web_native_sources + wasm_wrapper_sources

executable(
  'glib-main',
  glib_wasm_main_sources,
  dependencies: [libglib_dep, libgobject_dep],
  c_args: glib_wasm_cargs + ['-DGOBJECT_MAIN_MODULE=1'],
  link_args: glib_main_link_args,
  install: true,
  install_dir: get_option('bindir'),
)

# SIDE_MODULE build (production, dynamic loading)
glib_side_link_args = [
  '-sSIDE_MODULE=2',
  '-fPIC',
  '-sERROR_ON_UNDEFINED_SYMBOLS=0',  # Host provides dependencies
  '-sEXPORTED_FUNCTIONS=["_glib_wasm_version","_g_malloc","_g_free","_g_strdup"]',
  '-sWASM_BIGINT=1',
] + wasm_opt_flags + wasm_threading_flags

# SIDE_MODULE with minimal sources (imports from MAIN)
glib_wasm_side_sources = files(
  'glib_wasm_side.c',
  'wasm_compat_side.c',
) + web_native_sources

shared_module(
  'glib-side',
  glib_wasm_side_sources,
  dependencies: [libglib_dep],
  name_prefix: '',
  name_suffix: 'so',  # Will be renamed to .wasm in install script
  c_args: glib_wasm_cargs + ['-DGOBJECT_SIDE_MODULE=1', '-DBUILD_SIDE_MODULE=1'],
  link_args: glib_side_link_args,
  install: true,
  install_dir: get_option('libdir'),
)
