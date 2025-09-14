/* GLib WASM - WebGPU integration implementation
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for WebGPU types
typedef void* WGPUDevice;
typedef void* WGPUQueue;
typedef void* WGPUBuffer;
typedef void* WGPUCommandEncoder;
typedef void* WGPUComputePipeline;
typedef uint32_t WGPUBufferUsage;

// Generic WebGPU context structure - compatible with any orchestrator
typedef struct {
    WGPUDevice device;
    WGPUQueue queue;
    bool has_timestamp_queries;
    bool has_shared_context;
    uint32_t max_buffer_size;
    uint32_t max_compute_workgroup_size;
    void* external_context; // Opaque pointer for orchestrator integration
} GLibWebGPUContext;

static GLibWebGPUContext* g_webgpu_context = NULL;

// JavaScript integration using EM_JS - direct W3C WebGPU API access
EM_JS(bool, webgpu_is_available, (), {
    return typeof navigator.gpu !== 'undefined';
});

EM_JS(void*, webgpu_request_adapter, (), {
    return navigator.gpu ? navigator.gpu.requestAdapter({
        powerPreference: 'high-performance',
        forceFallbackAdapter: false
    }) : null;
});

EM_JS(void*, webgpu_request_device, (void* adapter), {
    if (!adapter) return null;

    // Standard WebGPU device features - any orchestrator can provide these
    const requiredFeatures = [];
    const optionalFeatures = ['timestamp-query', 'shader-f16'];

    const availableFeatures = Array.from(adapter.features);
    const supportedOptionalFeatures = optionalFeatures.filter(feature =>
        availableFeatures.includes(feature));

    return adapter.requestDevice({
        requiredFeatures,
        requiredLimits: {
            maxBufferSize: 268435456, // 256MB
            maxComputeWorkgroupStorageSize: 16384,
            maxComputeInvocationsPerWorkgroup: 256
        }
    }).catch(() => null);
});

EM_JS(void*, webgpu_get_queue, (void* device), {
    return device ? device.queue : null;
});

EM_JS(bool, webgpu_has_feature, (void* device, const char* feature), {
    if (!device) return false;
    const featureName = UTF8ToString(feature);
    return device.features.has(featureName);
});

EM_JS(void*, webgpu_create_buffer, (void* device, uint32_t size, uint32_t usage), {
    if (!device) return null;

    return device.createBuffer({
        size: size,
        usage: usage,
        mappedAtCreation: false
    });
});

EM_JS(void, webgpu_write_buffer, (void* queue, void* buffer, const void* data, uint32_t size), {
    if (!queue || !buffer || !data) return;

    // Copy data from WASM memory to JavaScript
    const dataArray = new Uint8Array(Module.HEAPU8.buffer, data, size);
    queue.writeBuffer(buffer, 0, dataArray);
});

EM_JS(double, webgpu_get_timestamp, (void* device), {
    // Use high-precision timing if available, fallback to performance.now()
    if (device && device.features && device.features.has('timestamp-query')) {
        // This would typically require a more complex implementation
        // with timestamp queries, but for compatibility we use performance API
        return performance.now() * 1000000; // Convert to nanoseconds
    }
    return performance.now() * 1000000; // Microseconds precision fallback
});

// External orchestrator integration interface - generic design
EM_JS(void, webgpu_register_external_context, (void* external_context), {
    // Allow external orchestrators to register their context
    // This enables integration without explicit dependencies
    if (typeof Module.externalWebGPUContext === 'undefined') {
        Module.externalWebGPUContext = external_context;
    }
});

EM_JS(void*, webgpu_get_external_context, (), {
    return Module.externalWebGPUContext || null;
});

EM_JS(bool, webgpu_can_share_context, (), {
    // Check if external orchestrator supports context sharing
    return typeof Module.externalWebGPUContext !== 'undefined' &&
           typeof Module.externalWebGPUContext.shareDevice === 'function';
});

// Public API for GLib WASM - generic WebGPU integration
bool g_webgpu_initialize(void* external_orchestrator) {
    if (!webgpu_is_available()) {
        return false;
    }

    g_webgpu_context = malloc(sizeof(GLibWebGPUContext));
    if (!g_webgpu_context) {
        return false;
    }

    memset(g_webgpu_context, 0, sizeof(GLibWebGPUContext));

    // Try to use external orchestrator context if available
    if (external_orchestrator) {
        webgpu_register_external_context(external_orchestrator);

        if (webgpu_can_share_context()) {
            // Use shared context from orchestrator
            g_webgpu_context->external_context = external_orchestrator;
            g_webgpu_context->has_shared_context = true;

            // Get device and queue from orchestrator via JavaScript
            EM_ASM({
                if (Module.externalWebGPUContext && Module.externalWebGPUContext.shareDevice) {
                    const sharedDevice = Module.externalWebGPUContext.shareDevice();
                    Module._g_webgpu_set_shared_device(sharedDevice.device, sharedDevice.queue);
                }
            });

            return true;
        }
    }

    // Fallback: create own WebGPU context
    void* adapter = webgpu_request_adapter();
    if (!adapter) {
        free(g_webgpu_context);
        g_webgpu_context = NULL;
        return false;
    }

    void* device = webgpu_request_device(adapter);
    if (!device) {
        free(g_webgpu_context);
        g_webgpu_context = NULL;
        return false;
    }

    g_webgpu_context->device = device;
    g_webgpu_context->queue = webgpu_get_queue(device);
    g_webgpu_context->has_timestamp_queries = webgpu_has_feature(device, "timestamp-query");
    g_webgpu_context->has_shared_context = false;

    return true;
}

// Allow external orchestrator to provide shared device/queue
EMSCRIPTEN_KEEPALIVE
void g_webgpu_set_shared_device(void* device, void* queue) {
    if (g_webgpu_context && device && queue) {
        g_webgpu_context->device = device;
        g_webgpu_context->queue = queue;
        g_webgpu_context->has_shared_context = true;
        g_webgpu_context->has_timestamp_queries = webgpu_has_feature(device, "timestamp-query");
    }
}

bool g_webgpu_is_initialized(void) {
    return g_webgpu_context != NULL;
}

bool g_webgpu_has_shared_context(void) {
    return g_webgpu_context && g_webgpu_context->has_shared_context;
}

bool g_webgpu_has_timestamp_queries(void) {
    return g_webgpu_context && g_webgpu_context->has_timestamp_queries;
}

// High-precision timing using WebGPU when available
double g_get_monotonic_time_ns(void) {
    if (!g_webgpu_context) {
        // Fallback to JavaScript performance.now()
        return EM_ASM_DOUBLE({
            return performance.now() * 1000000; // Convert to nanoseconds
        });
    }

    return webgpu_get_timestamp(g_webgpu_context->device);
}

// Memory allocation with WebGPU buffers for shared compute
void* g_webgpu_allocate_shared_buffer(size_t size, uint32_t usage) {
    if (!g_webgpu_context || !g_webgpu_context->device) {
        return NULL;
    }

    return webgpu_create_buffer(g_webgpu_context->device, size, usage);
}

// Write data to WebGPU buffer
bool g_webgpu_write_buffer(void* buffer, const void* data, size_t size) {
    if (!g_webgpu_context || !g_webgpu_context->queue || !buffer || !data) {
        return false;
    }

    webgpu_write_buffer(g_webgpu_context->queue, buffer, data, size);
    return true;
}

// Coordinate with external orchestrator for resource sharing
bool g_webgpu_coordinate_resource(const char* resource_type, size_t size, void** resource_out) {
    if (!g_webgpu_context || !g_webgpu_context->external_context) {
        return false;
    }

    // Generic resource coordination via JavaScript
    return EM_ASM_INT({
        if (Module.externalWebGPUContext &&
            Module.externalWebGPUContext.allocateResource) {

            const resourceType = UTF8ToString($0);
            const size = $1;

            try {
                const resource = Module.externalWebGPUContext.allocateResource(resourceType, size);
                if (resource) {
                    setValue($2, resource, '*');
                    return 1;
                }
            } catch (e) {
                console.warn('Resource coordination failed:', e);
            }
        }
        return 0;
    }, resource_type, size, resource_out);
}

void g_webgpu_cleanup(void) {
    if (g_webgpu_context) {
        // Notify external orchestrator of cleanup if needed
        if (g_webgpu_context->external_context) {
            EM_ASM({
                if (Module.externalWebGPUContext &&
                    Module.externalWebGPUContext.notifyCleanup) {
                    Module.externalWebGPUContext.notifyCleanup('glib-wasm');
                }
            });
        }

        free(g_webgpu_context);
        g_webgpu_context = NULL;
    }
}