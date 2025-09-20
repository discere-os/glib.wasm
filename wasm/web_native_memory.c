/* GLib WASM - Web-Native Memory Management
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This implements enhanced memory management using WeakRef + FinalizationRegistry
 * to eliminate reference counting bugs and integrate with browser GC.
 */

#include <glib.h>
#include <emscripten/emscripten.h>
#include <stdatomic.h>

// Forward declarations
extern const GWebCapabilities* g_web_get_capabilities(void);

// Web-native memory management state
typedef struct {
    gpointer object_ptr;
    gchar *debug_info;
    guint64 allocation_time;
    gsize allocation_size;
    atomic_int ref_count;
    gboolean in_finalization;
    gint js_weak_ref_id;
} GWebMemoryEntry;

// Global memory tracking
static GHashTable *web_memory_registry = NULL;
static GMutex memory_registry_mutex;
static atomic_int weak_ref_counter = 1;
static gboolean web_memory_initialized = FALSE;

// Memory statistics
static atomic_long total_allocations = 0;
static atomic_long active_objects = 0;
static atomic_long bytes_allocated = 0;
static atomic_long bytes_freed = 0;
static atomic_long gc_collections = 0;

// JavaScript WeakRef and FinalizationRegistry integration
EM_JS(int, create_weak_ref, (void* ptr, const char* debug_info), {
    // Initialize global memory management objects
    if (!Module.glibMemory) {
        Module.glibMemory = {
            weakRefs: new Map(),
            finalizers: new FinalizationRegistry((id) => {
                // Call C callback when object is garbage collected
                Module._g_web_memory_js_finalized(id);
            }),
            nextId: 1
        };

        console.log('[GLib.wasm] Memory management with WeakRef initialized');
    }

    const id = Module.glibMemory.nextId++;
    const debugStr = UTF8ToString(debug_info);

    // Create WeakRef
    const weakRef = new WeakRef({ ptr: ptr, debug: debugStr });
    Module.glibMemory.weakRefs.set(id, weakRef);

    // Register for finalization
    Module.glibMemory.finalizers.register({ ptr: ptr }, id);

    console.debug(`[Memory] Created WeakRef ${id} for ${debugStr}`);
    return id;
});

EM_JS(void, remove_weak_ref, (int id), {
    if (Module.glibMemory) {
        const weakRef = Module.glibMemory.weakRefs.get(id);
        if (weakRef) {
            const obj = weakRef.deref();
            if (obj) {
                console.debug(`[Memory] Removing WeakRef ${id} for ${obj.debug}`);
            }
            Module.glibMemory.weakRefs.delete(id);
        }
    }
});

EM_JS(int, check_object_reachable, (int id), {
    if (!Module.glibMemory) return 0;

    const weakRef = Module.glibMemory.weakRefs.get(id);
    if (!weakRef) return 0;

    const obj = weakRef.deref();
    return obj ? 1 : 0;
});

EM_JS(void, trigger_gc_hint, (), {
    // Hint to the browser that GC would be helpful
    if (typeof gc !== 'undefined') {
        gc(); // Available in V8 with --expose-gc
    } else {
        // Create memory pressure to encourage GC
        const temp = new ArrayBuffer(1024 * 1024); // 1MB
        setTimeout(() => { temp; }, 0); // Keep reference briefly
    }
});

EM_JS(void, setup_memory_pressure_handler, (), {
    // Set up memory pressure event handling
    if ('memory' in performance && 'addEventListener' in performance.memory) {
        performance.memory.addEventListener('memorypressure', () => {
            console.log('[GLib.wasm] Memory pressure detected, cleaning up');
            Module._g_web_memory_handle_pressure();
        });
    }

    // Periodic cleanup
    setInterval(() => {
        Module._g_web_memory_periodic_cleanup();
    }, 30000); // Every 30 seconds

    console.log('[GLib.wasm] Memory pressure handlers installed');
});

/**
 * JavaScript finalization callback
 */
EMSCRIPTEN_KEEPALIVE
void g_web_memory_js_finalized(gint weak_ref_id) {
    atomic_fetch_add(&gc_collections, 1);

    g_mutex_lock(&memory_registry_mutex);

    // Find and clean up the memory entry
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, web_memory_registry);

    while (g_hash_table_iter_next(&iter, &key, &value)) {
        GWebMemoryEntry *entry = (GWebMemoryEntry*)value;
        if (entry->js_weak_ref_id == weak_ref_id) {
            g_debug("JS GC finalized object: %s (id=%d)", entry->debug_info, weak_ref_id);
            atomic_fetch_sub(&active_objects, 1);
            atomic_fetch_add(&bytes_freed, (long)entry->allocation_size);

            g_hash_table_iter_remove(&iter);
            g_free(entry->debug_info);
            g_free(entry);
            break;
        }
    }

    g_mutex_unlock(&memory_registry_mutex);
}

/**
 * Memory pressure event handler
 */
EMSCRIPTEN_KEEPALIVE
void g_web_memory_handle_pressure(void) {
    g_message("Handling memory pressure - cleaning up unreachable objects");

    g_mutex_lock(&memory_registry_mutex);

    GHashTableIter iter;
    gpointer key, value;
    GPtrArray *to_remove = g_ptr_array_new();

    g_hash_table_iter_init(&iter, web_memory_registry);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        GWebMemoryEntry *entry = (GWebMemoryEntry*)value;

        if (!check_object_reachable(entry->js_weak_ref_id)) {
            g_ptr_array_add(to_remove, entry);
        }
    }

    // Remove unreachable objects
    for (guint i = 0; i < to_remove->len; i++) {
        GWebMemoryEntry *entry = g_ptr_array_index(to_remove, i);
        g_hash_table_remove(web_memory_registry, entry->object_ptr);
        g_free(entry->debug_info);
        g_free(entry);
        atomic_fetch_sub(&active_objects, 1);
    }

    g_ptr_array_free(to_remove, TRUE);
    g_mutex_unlock(&memory_registry_mutex);

    g_message("Memory cleanup removed %u unreachable objects", to_remove->len);
}

/**
 * Periodic memory cleanup
 */
EMSCRIPTEN_KEEPALIVE
void g_web_memory_periodic_cleanup(void) {
    static gint cleanup_counter = 0;
    cleanup_counter++;

    // Run full cleanup every 10 cycles (5 minutes)
    if (cleanup_counter % 10 == 0) {
        g_web_memory_handle_pressure();
    }

    glong active = atomic_load(&active_objects);
    if (active > 1000) {  // Threshold for aggressive cleanup
        g_debug("High object count (%ld), suggesting GC", active);
        trigger_gc_hint();
    }
}

/**
 * Initialize web-native memory management
 */
static void g_web_memory_init(void) {
    if (web_memory_initialized) return;

    web_memory_registry = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
    g_mutex_init(&memory_registry_mutex);

    setup_memory_pressure_handler();
    web_memory_initialized = TRUE;

    g_message("Web-native memory management initialized");
    g_message("  Features: WeakRef, FinalizationRegistry, automatic cleanup");
}

/**
 * Enhanced memory allocation with WeakRef tracking
 */
EMSCRIPTEN_KEEPALIVE
gpointer g_web_malloc_tracked(gsize size, const gchar *debug_info) {
    if (!web_memory_initialized) {
        g_web_memory_init();
    }

    gpointer ptr = g_malloc(size);
    if (!ptr) return NULL;

    // Create memory tracking entry
    GWebMemoryEntry *entry = g_new0(GWebMemoryEntry, 1);
    entry->object_ptr = ptr;
    entry->debug_info = g_strdup(debug_info ? debug_info : "unknown");
    entry->allocation_time = g_get_monotonic_time();
    entry->allocation_size = size;
    atomic_init(&entry->ref_count, 1);
    entry->in_finalization = FALSE;

    // Create JavaScript WeakRef
    entry->js_weak_ref_id = create_weak_ref(ptr, entry->debug_info);

    // Register in memory registry
    g_mutex_lock(&memory_registry_mutex);
    g_hash_table_insert(web_memory_registry, ptr, entry);
    g_mutex_unlock(&memory_registry_mutex);

    // Update statistics
    atomic_fetch_add(&total_allocations, 1);
    atomic_fetch_add(&active_objects, 1);
    atomic_fetch_add(&bytes_allocated, (long)size);

    g_debug("Allocated %zu bytes at %p (%s)", size, ptr, entry->debug_info);
    return ptr;
}

/**
 * Enhanced memory deallocation with tracking cleanup
 */
EMSCRIPTEN_KEEPALIVE
void g_web_free_tracked(gpointer ptr) {
    if (!ptr || !web_memory_initialized) {
        g_free(ptr);
        return;
    }

    g_mutex_lock(&memory_registry_mutex);

    GWebMemoryEntry *entry = g_hash_table_lookup(web_memory_registry, ptr);
    if (entry) {
        // Remove WeakRef
        remove_weak_ref(entry->js_weak_ref_id);

        // Update statistics
        atomic_fetch_sub(&active_objects, 1);
        atomic_fetch_add(&bytes_freed, (long)entry->allocation_size);

        g_debug("Freed %zu bytes at %p (%s)", entry->allocation_size, ptr, entry->debug_info);

        // Clean up entry
        g_hash_table_remove(web_memory_registry, ptr);
        g_free(entry->debug_info);
        g_free(entry);
    }

    g_mutex_unlock(&memory_registry_mutex);

    g_free(ptr);
}

/**
 * Enhanced GObject creation with WeakRef integration
 */
EMSCRIPTEN_KEEPALIVE
GObject* g_web_object_new_tracked(GType object_type, const gchar *debug_info) {
    if (!web_memory_initialized) {
        g_web_memory_init();
    }

    // Create object using standard GObject system
    GObject *object = (GObject*)g_type_create_instance(object_type);
    if (!object) return NULL;

    // Create enhanced tracking
    gchar *full_debug_info = g_strdup_printf("GObject:%s:%s",
                                            g_type_name(object_type),
                                            debug_info ? debug_info : "unknown");

    GWebMemoryEntry *entry = g_new0(GWebMemoryEntry, 1);
    entry->object_ptr = object;
    entry->debug_info = full_debug_info;
    entry->allocation_time = g_get_monotonic_time();
    entry->allocation_size = g_type_instance_size(object_type);
    atomic_init(&entry->ref_count, 1);
    entry->in_finalization = FALSE;

    // Create JavaScript WeakRef
    entry->js_weak_ref_id = create_weak_ref(object, entry->debug_info);

    // Register in memory registry
    g_mutex_lock(&memory_registry_mutex);
    g_hash_table_insert(web_memory_registry, object, entry);
    g_mutex_unlock(&memory_registry_mutex);

    // Update statistics
    atomic_fetch_add(&total_allocations, 1);
    atomic_fetch_add(&active_objects, 1);
    atomic_fetch_add(&bytes_allocated, (long)entry->allocation_size);

    g_debug("Created GObject %s at %p", g_type_name(object_type), object);
    return object;
}

/**
 * Enhanced reference counting with browser integration
 */
EMSCRIPTEN_KEEPALIVE
gpointer g_web_object_ref(gpointer object) {
    if (!object || !web_memory_initialized) {
        return g_object_ref(object);
    }

    g_mutex_lock(&memory_registry_mutex);
    GWebMemoryEntry *entry = g_hash_table_lookup(web_memory_registry, object);

    if (entry) {
        gint old_count = atomic_fetch_add(&entry->ref_count, 1);
        g_debug("Ref %s: %d → %d", entry->debug_info, old_count, old_count + 1);
    }
    g_mutex_unlock(&memory_registry_mutex);

    return g_object_ref(object);
}

/**
 * Enhanced reference counting with automatic cleanup
 */
EMSCRIPTEN_KEEPALIVE
void g_web_object_unref(gpointer object) {
    if (!object || !web_memory_initialized) {
        g_object_unref(object);
        return;
    }

    g_mutex_lock(&memory_registry_mutex);
    GWebMemoryEntry *entry = g_hash_table_lookup(web_memory_registry, object);

    if (entry && !entry->in_finalization) {
        gint old_count = atomic_fetch_sub(&entry->ref_count, 1);
        g_debug("Unref %s: %d → %d", entry->debug_info, old_count, old_count - 1);

        if (old_count == 1) {
            // Last reference - clean up tracking
            entry->in_finalization = TRUE;
            remove_weak_ref(entry->js_weak_ref_id);

            g_hash_table_remove(web_memory_registry, object);
            atomic_fetch_sub(&active_objects, 1);

            g_free(entry->debug_info);
            g_free(entry);
        }
    }
    g_mutex_unlock(&memory_registry_mutex);

    g_object_unref(object);
}

/**
 * Get memory management statistics
 */
EMSCRIPTEN_KEEPALIVE
void g_web_memory_get_stats(gint64 *total_allocs, gint64 *active_objs,
                           gint64 *bytes_alloc, gint64 *bytes_free,
                           gint64 *gc_collections_count) {
    if (total_allocs) *total_allocs = atomic_load(&total_allocations);
    if (active_objs) *active_objs = atomic_load(&active_objects);
    if (bytes_alloc) *bytes_alloc = atomic_load(&bytes_allocated);
    if (bytes_free) *bytes_free = atomic_load(&bytes_freed);
    if (gc_collections_count) *gc_collections_count = atomic_load(&gc_collections);
}

/**
 * Memory leak detection
 */
EMSCRIPTEN_KEEPALIVE
void g_web_memory_dump_leaks(void) {
    if (!web_memory_initialized) return;

    g_mutex_lock(&memory_registry_mutex);

    gint leak_count = g_hash_table_size(web_memory_registry);
    if (leak_count == 0) {
        g_message("No memory leaks detected");
        g_mutex_unlock(&memory_registry_mutex);
        return;
    }

    g_warning("Potential memory leaks detected: %d objects", leak_count);

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, web_memory_registry);

    while (g_hash_table_iter_next(&iter, &key, &value)) {
        GWebMemoryEntry *entry = (GWebMemoryEntry*)value;
        guint64 age_ms = (g_get_monotonic_time() - entry->allocation_time) / 1000;

        g_warning("  Leak: %p (%s) - %zu bytes, age: %lu ms, refs: %d",
                  entry->object_ptr, entry->debug_info, entry->allocation_size,
                  (unsigned long)age_ms, atomic_load(&entry->ref_count));
    }

    g_mutex_unlock(&memory_registry_mutex);
}

/**
 * Force garbage collection cycle
 */
EMSCRIPTEN_KEEPALIVE
void g_web_memory_force_gc(void) {
    g_message("Forcing garbage collection cycle");
    trigger_gc_hint();

    // Give GC time to run
    emscripten_sleep(10);

    g_web_memory_handle_pressure();

    gint64 active = atomic_load(&active_objects);
    gint64 gc_count = atomic_load(&gc_collections);

    g_message("Post-GC: %ld active objects, %ld GC cycles", active, gc_count);
}

/**
 * Initialize web-native memory management system
 */
void g_web_memory_system_init(void) {
    g_web_memory_init();

    g_message("Web-native memory system ready");
    g_message("  WeakRef tracking: enabled");
    g_message("  Automatic finalization: enabled");
    g_message("  Memory pressure handling: enabled");
    g_message("  Leak detection: enabled");
}

/**
 * Cleanup web memory management
 */
void g_web_memory_cleanup(void) {
    if (!web_memory_initialized) return;

    g_message("Cleaning up web-native memory management");

    // Dump any remaining leaks
    g_web_memory_dump_leaks();

    g_mutex_lock(&memory_registry_mutex);

    if (web_memory_registry) {
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, web_memory_registry);

        while (g_hash_table_iter_next(&iter, &key, &value)) {
            GWebMemoryEntry *entry = (GWebMemoryEntry*)value;
            remove_weak_ref(entry->js_weak_ref_id);
            g_free(entry->debug_info);
            g_free(entry);
        }

        g_hash_table_destroy(web_memory_registry);
        web_memory_registry = NULL;
    }

    g_mutex_unlock(&memory_registry_mutex);
    g_mutex_clear(&memory_registry_mutex);

    web_memory_initialized = FALSE;

    gint64 total_allocs = atomic_load(&total_allocations);
    gint64 bytes_alloc = atomic_load(&bytes_allocated);
    gint64 bytes_free = atomic_load(&bytes_freed);
    gint64 gc_count = atomic_load(&gc_collections);

    g_message("Final memory stats: %ld allocations, %ld bytes allocated, %ld bytes freed, %ld GC cycles",
              total_allocs, bytes_alloc, bytes_free, gc_count);
}