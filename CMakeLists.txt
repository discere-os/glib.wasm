cmake_minimum_required(VERSION 3.24)
project(glib-wasm VERSION 2.85.4 LANGUAGES C)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Validate Emscripten toolchain
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "This project requires Emscripten toolchain. Use: emcmake cmake ..")
endif()

# Configure build options
option(ENABLE_SIMD "Enable WASM SIMD optimizations" ON)
option(ENABLE_THREADING "Enable SharedArrayBuffer threading" ON)
option(ENABLE_WEBGPU "Enable WebGPU integration" ON)
option(ENABLE_OPFS "Enable OPFS filesystem" ON)
option(ENABLE_BROWSER_MAINLOOP "Enable browser-native main loop" ON)
option(BUILD_SIDE_MODULE "Build as SIDE_MODULE for dynamic linking" ON)
option(BUILD_TESTS "Build test suite" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)

# Set Emscripten-specific configuration
set(CMAKE_EXECUTABLE_SUFFIX ".js")

# Base compiler flags
set(WASM_COMPILE_FLAGS
    -DGLIB_STATIC_COMPILATION=1
    -DGLIB_WASM_NATIVE=1
    -DGLIB_COMPILATION=1
    -include${CMAKE_CURRENT_BINARY_DIR}/config.h
)

# Base linker flags
set(WASM_LINK_FLAGS
    "-s WASM=1"
    "-s MODULARIZE=1"
    "-s EXPORT_ES6=1"
    "-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','HEAPU8','HEAPU32','HEAP32','addFunction','removeFunction','getValue','setValue','UTF8ToString','stringToUTF8']"
    "-s EXPORTED_FUNCTIONS=['_g_main_loop_process_frame','_g_main_loop_process_immediate_sources','_g_main_loop_execute_source','_g_timeout_source_execute','_g_webgpu_set_shared_device']"
    "-s INITIAL_MEMORY=128MB"
    "-s MAXIMUM_MEMORY=1GB"
    "-s ALLOW_MEMORY_GROWTH=1"
    "-s STACK_SIZE=5MB"
    "-s ASSERTIONS=0"
    "-s FORCE_FILESYSTEM=1"
    "-s USE_WEBGPU=1"
    -lidbfs.js
)

# SIDE_MODULE configuration for dynamic linking
if(BUILD_SIDE_MODULE)
    # SIDE_MODULE flags per Emscripten docs - no system libraries, rely on MAIN_MODULE
    set(WASM_LINK_FLAGS
        "-s SIDE_MODULE=1"
        # System libraries will be provided by host MAIN_MODULE
        "-s DISABLE_EXCEPTION_CATCHING=1"
    )
    # Add position-independent code for SIDE_MODULE
    list(APPEND WASM_COMPILE_FLAGS -fPIC)
    add_compile_definitions(GLIB_WASM_SIDE_MODULE=1)
    message(STATUS "Building as SIDE_MODULE for dynamic linking - hosted by host application")
endif()

# Threading support with SharedArrayBuffer - Emscripten production config
if(ENABLE_THREADING AND NOT BUILD_SIDE_MODULE)
    # MAIN_MODULE: Full threading support
    list(APPEND WASM_LINK_FLAGS
        "-s USE_PTHREADS=1"
        "-s SHARED_MEMORY=1"
        "-s PTHREAD_POOL_SIZE=4"
        "-s PROXY_TO_PTHREAD=1"
        "-s ASYNCIFY=1"
        "-s MALLOC=mimalloc"
    )
    list(APPEND WASM_COMPILE_FLAGS -pthread)
    add_compile_definitions(GLIB_WASM_THREADING_ENABLED=1)
    add_compile_definitions(G_THREADS_ENABLED=1)
    add_compile_definitions(__EMSCRIPTEN_PTHREADS__=1)
    message(STATUS "Threading enabled with SharedArrayBuffer + pthreads (production config)")
elseif(ENABLE_THREADING AND BUILD_SIDE_MODULE)
    # SIDE_MODULE: Threading provided by host application
    add_compile_definitions(GLIB_WASM_THREADING_ENABLED=1)
    add_compile_definitions(G_THREADS_ENABLED=1)
    add_compile_definitions(__EMSCRIPTEN_PTHREADS__=1)
    message(STATUS "Threading enabled - provided by host application")
endif()

# WebGPU integration
if(ENABLE_WEBGPU AND NOT BUILD_SIDE_MODULE)
    # MAIN_MODULE: Include WebGPU port
    list(APPEND WASM_LINK_FLAGS --use-port=emdawnwebgpu)
    add_compile_definitions(GLIB_WASM_WEBGPU_ENABLED=1)
    message(STATUS "WebGPU integration enabled")
elseif(ENABLE_WEBGPU AND BUILD_SIDE_MODULE)
    # SIDE_MODULE: WebGPU provided by host application
    add_compile_definitions(GLIB_WASM_WEBGPU_ENABLED=1)
    message(STATUS "WebGPU integration enabled - provided by host application")
endif()

# OPFS filesystem
if(ENABLE_OPFS AND NOT BUILD_SIDE_MODULE)
    # MAIN_MODULE: Include OPFS support
    list(APPEND WASM_LINK_FLAGS -s ASYNCIFY=1)
    add_compile_definitions(GLIB_WASM_OPFS_ENABLED=1)
    message(STATUS "OPFS filesystem enabled")
elseif(ENABLE_OPFS AND BUILD_SIDE_MODULE)
    # SIDE_MODULE: OPFS provided by host application
    add_compile_definitions(GLIB_WASM_OPFS_ENABLED=1)
    message(STATUS "OPFS filesystem enabled - provided by host application")
endif()

# Browser main loop
if(ENABLE_BROWSER_MAINLOOP)
    add_compile_definitions(GLIB_WASM_BROWSER_MAINLOOP_ENABLED=1)
    if(BUILD_SIDE_MODULE)
        message(STATUS "Browser-native main loop enabled - provided by host application")
    else()
        message(STATUS "Browser-native main loop enabled")
    endif()
endif()

# Performance flags
set(WASM_PERF_COMPILE_FLAGS -O3)
set(WASM_PERF_LINK_FLAGS -O3 -flto --closure=1)

# SIMD support
if(ENABLE_SIMD)
    list(APPEND WASM_COMPILE_FLAGS -msimd128)
    add_compile_definitions(GLIB_WASM_SIMD_ENABLED=1)
    message(STATUS "WASM SIMD optimizations enabled")
endif()

# Apply flags
add_compile_options(${WASM_COMPILE_FLAGS} ${WASM_PERF_COMPILE_FLAGS})
add_link_options(${WASM_LINK_FLAGS} ${WASM_PERF_LINK_FLAGS})

# Disable problematic components for WASM
add_compile_definitions(
    G_DISABLE_DEPRECATED=1
    GLIB_DISABLE_DEPRECATION_WARNINGS=1
    G_DISABLE_ASSERT=1
    NO_XVFB=1
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/glib
    ${CMAKE_CURRENT_SOURCE_DIR}/gmodule
    ${CMAKE_CURRENT_SOURCE_DIR}/gobject
    ${CMAKE_CURRENT_SOURCE_DIR}/gio
)

# Configure config.h for WASM
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
    @ONLY
)

# Configure glibconfig.h for WASM (already created manually for WASM-specific values)
# The glibconfig.h file is already provided in glib/glibconfig.h with WASM-specific values

# Dependencies - use ecosystem libs when available
if(BUILD_SIDE_MODULE)
    # SIDE_MODULE: Use ecosystem dependencies via dynamic linking
    # Dependencies will be resolved at runtime by host application

    # Check for PCRE2 SIDE_MODULE
    find_file(PCRE2_SIDE_MODULE NAMES pcre2-side.wasm PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/../pcre2.wasm/install/wasm
        ${CMAKE_CURRENT_SOURCE_DIR}/../pcre2.wasm/build-dual-side)

    if(PCRE2_SIDE_MODULE)
        message(STATUS "PCRE2 SIDE_MODULE found: ${PCRE2_SIDE_MODULE}")
        set(HAVE_PCRE2 1)
    else()
        message(STATUS "PCRE2 SIDE_MODULE not found - using internal regex")
        set(HAVE_PCRE2 0)
    endif()

    # Check for libffi SIDE_MODULE
    find_file(FFI_SIDE_MODULE NAMES libffi-side.wasm PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/../libffi.wasm/install/wasm
        ${CMAKE_CURRENT_SOURCE_DIR}/../libffi.wasm/build-dual-side)

    if(FFI_SIDE_MODULE)
        message(STATUS "libffi SIDE_MODULE found: ${FFI_SIDE_MODULE}")
        set(HAVE_LIBFFI 1)
    else()
        message(STATUS "libffi SIDE_MODULE not found - disabling GObject marshalling")
        set(HAVE_LIBFFI 0)
    endif()
else()
    # MAIN_MODULE: Traditional static linking
    set(HAVE_PCRE2 0)
    message(STATUS "PCRE2 disabled for MAIN_MODULE build - using internal regex")

    find_library(FFI_LIBRARY NAMES ffi PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/../libffi.wasm/install/wasm
        ${CMAKE_CURRENT_SOURCE_DIR}/../libffi.wasm/lib)

    if(FFI_LIBRARY)
        message(STATUS "Using ecosystem libffi.wasm: ${FFI_LIBRARY}")
        set(HAVE_LIBFFI 1)
    else()
        message(STATUS "libffi not found - disabling GObject marshalling")
        set(HAVE_LIBFFI 0)
    endif()
endif()

# Core GLib sources (minimal subset for WASM)
set(GLIB_CORE_SOURCES
    glib/garray.c
    glib/gasyncqueue.c
    glib/gatomic.c
    glib/gbacktrace.c
    glib/gbase64.c
    glib/gbitlock.c
    glib/gbytes.c
    glib/gcharset.c
    glib/gchecksum.c
    glib/gconvert.c
    glib/gdataset.c
    glib/gdate.c
    glib/gdatetime.c
    glib/gdir.c
    glib/gerror.c
    glib/gfileutils.c
    glib/ggettext.c
    glib/ghash.c
    glib/ghmac.c
    glib/ghook.c
    glib/giochannel.c
    glib/gkeyfile.c
    glib/glist.c
    glib/gmain.c
    glib/gmappedfile.c
    glib/gmarkup.c
    glib/gmem.c
    glib/gmessages.c
    glib/gnode.c
    glib/goption.c
    glib/gpattern.c
    glib/gpoll.c
    glib/gprintf.c
    glib/gqsort.c
    glib/gquark.c
    glib/gqueue.c
    glib/grand.c
    glib/grcbox.c
    glib/grefcount.c
    glib/grefstring.c
    glib/gscanner.c
    glib/gsequence.c
    glib/gshell.c
    glib/gslice.c
    glib/gslist.c
    glib/gstdio.c
    glib/gstrfuncs.c
    glib/gstring.c
    glib/gstringchunk.c
    glib/gtestutils.c
    glib/gthread.c
    glib/gthreadpool.c
    glib/gtimer.c
    glib/gtimezone.c
    glib/gtrace.c
    glib/gtranslit.c
    glib/gtrashstack.c
    glib/gtree.c
    glib/gunibreak.c
    glib/gunicollate.c
    glib/gunidecomp.c
    glib/guniprop.c
    glib/guri.c
    glib/gutf8.c
    glib/gutils.c
    glib/guuid.c
    glib/gvariant.c
    glib/gvariant-core.c
    glib/gvariant-parser.c
    glib/gvariant-serialiser.c
    glib/gvarianttype.c
    glib/gvarianttypeinfo.c
    glib/gversion.c
)

# Add regex support if PCRE2 is available
if(HAVE_PCRE2)
    list(APPEND GLIB_CORE_SOURCES glib/gregex.c)
endif()

# GObject sources (if libffi available)
if(HAVE_LIBFFI)
    set(GOBJECT_SOURCES
        gobject/gboxed.c
        gobject/gclosure.c
        gobject/genums.c
        gobject/gmarshal.c
        gobject/gobject.c
        gobject/gparam.c
        gobject/gparamspecs.c
        gobject/gsignal.c
        gobject/gsourceclosure.c
        gobject/gtype.c
        gobject/gtypemodule.c
        gobject/gtypeplugin.c
        gobject/gvalue.c
        gobject/gvaluetypes.c
    )
endif()

# Create main library target
add_library(glib-wasm STATIC
    ${GLIB_CORE_SOURCES}
    ${GOBJECT_SOURCES}
)

# Link dependencies
if(PCRE2_LIBRARY)
    target_link_libraries(glib-wasm ${PCRE2_LIBRARY})
endif()

if(FFI_LIBRARY)
    target_link_libraries(glib-wasm ${FFI_LIBRARY})
endif()

# Create WASM module
if(BUILD_SIDE_MODULE)
    # For SIDE_MODULE, include ALL GLib sources but rely on host application for system libraries
    # GLib implementation + wrapper, but no system compatibility layer
    add_library(glib-side STATIC
        wasm/glib_wasm_wrapper.c
        wasm/wasm_compat_side.c
        ${GLIB_CORE_SOURCES}
        ${GOBJECT_SOURCES}
    )

    # Don't link external dependencies - they will be resolved by host application
    # PCRE2 and libffi SIDE_MODULEs will be loaded separately by the host application

    # Convert static library to SIDE_MODULE .wasm using emcc directly
    # Per Emscripten docs: SIDE_MODULE excludes system libs, relies on MAIN_MODULE
    add_custom_command(
        OUTPUT glib-side.wasm
        COMMAND emcc
            -sSIDE_MODULE=1
            -fPIC
            $<TARGET_FILE:glib-side>
            -o glib-side.wasm
        DEPENDS glib-side
        COMMENT "Creating GLib SIDE_MODULE (all GLib sources, no system libraries)"
    )

    add_custom_target(glib-module ALL DEPENDS glib-side.wasm)
else()
    # For MAIN_MODULE, create executable with main() function
    add_executable(glib-module
        wasm/glib_wasm_wrapper.c
        wasm/wasm_compat.c
    )

    target_link_libraries(glib-module glib-wasm)

    set_target_properties(glib-module PROPERTIES
        OUTPUT_NAME "glib-release"
        SUFFIX ".js"
    )
endif()

# Install targets
install(TARGETS glib-wasm DESTINATION lib)

if(BUILD_SIDE_MODULE)
    # For SIDE_MODULE, install the generated .wasm file
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/glib-side.wasm DESTINATION dist)
else()
    # For MAIN_MODULE, install the executable target
    install(TARGETS glib-module DESTINATION dist)
endif()
install(DIRECTORY glib/ DESTINATION include/glib-2.0/glib
    FILES_MATCHING PATTERN "*.h")

if(HAVE_LIBFFI)
    install(DIRECTORY gobject/ DESTINATION include/glib-2.0/gobject
        FILES_MATCHING PATTERN "*.h")
endif()

# Tests
if(BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks
if(BUILD_BENCHMARKS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks")
    add_subdirectory(benchmarks)
endif()

# Generate package config
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/glib-2.0.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/glib-2.0.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/glib-2.0.pc
    DESTINATION lib/pkgconfig)