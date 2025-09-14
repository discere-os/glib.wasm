/* GLib WASM - WebGPU integration header
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

#ifndef GLIB_WEBGPU_INTEGRATION_H
#define GLIB_WEBGPU_INTEGRATION_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// WebGPU buffer usage flags - standard W3C WebGPU constants
#define GLIB_WEBGPU_BUFFER_USAGE_UNIFORM     (1 << 6)
#define GLIB_WEBGPU_BUFFER_USAGE_STORAGE     (1 << 7)
#define GLIB_WEBGPU_BUFFER_USAGE_COPY_SRC    (1 << 2)
#define GLIB_WEBGPU_BUFFER_USAGE_COPY_DST    (1 << 3)

/**
 * Initialize WebGPU integration for GLib WASM
 * @param external_orchestrator Optional pointer to external orchestrator context
 * @return true if initialization successful, false otherwise
 *
 * This function can work in two modes:
 * 1. Standalone: Creates own WebGPU context if external_orchestrator is NULL
 * 2. Orchestrated: Integrates with external orchestrator if provided
 */
bool g_webgpu_initialize(void* external_orchestrator);

/**
 * Check if WebGPU integration is initialized
 * @return true if WebGPU is available and initialized
 */
bool g_webgpu_is_initialized(void);

/**
 * Check if using shared context from external orchestrator
 * @return true if sharing WebGPU context with external system
 */
bool g_webgpu_has_shared_context(void);

/**
 * Check if timestamp queries are available
 * @return true if high-precision timing is supported
 */
bool g_webgpu_has_timestamp_queries(void);

/**
 * Get high-precision monotonic time in nanoseconds
 * Uses WebGPU timestamp queries when available, falls back to performance.now()
 * @return time in nanoseconds
 */
double g_get_monotonic_time_ns(void);

/**
 * Allocate shared WebGPU buffer
 * @param size Buffer size in bytes
 * @param usage WebGPU buffer usage flags
 * @return Opaque buffer pointer, NULL on failure
 */
void* g_webgpu_allocate_shared_buffer(size_t size, uint32_t usage);

/**
 * Write data to WebGPU buffer
 * @param buffer Buffer allocated with g_webgpu_allocate_shared_buffer
 * @param data Data to write
 * @param size Size of data in bytes
 * @return true on success, false on failure
 */
bool g_webgpu_write_buffer(void* buffer, const void* data, size_t size);

/**
 * Coordinate resource allocation with external orchestrator
 * @param resource_type String identifier for resource type
 * @param size Requested resource size
 * @param resource_out Output pointer for allocated resource
 * @return true if external orchestrator provided resource, false otherwise
 */
bool g_webgpu_coordinate_resource(const char* resource_type, size_t size, void** resource_out);

/**
 * Clean up WebGPU resources
 * Notifies external orchestrator if in shared mode
 */
void g_webgpu_cleanup(void);

/**
 * Internal function called by external orchestrator to provide shared device
 * @param device WebGPU device from orchestrator
 * @param queue WebGPU queue from orchestrator
 */
void g_webgpu_set_shared_device(void* device, void* queue);

#ifdef __cplusplus
}
#endif

#endif /* GLIB_WEBGPU_INTEGRATION_H */