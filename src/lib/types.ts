/* GLib WASM - TypeScript type definitions
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

// Browser feature detection
export interface GLibFeatures {
  webWorkers: boolean;
  sharedArrayBuffer: boolean;
  webGPU: boolean;
  opfs: boolean;
  webRTC: boolean;
  dynamicLinking: boolean;
  simd: boolean;
}

// Core GLib types (minimal set for WASM integration)
export type gboolean = boolean;
export type gchar = string;
export type guchar = number;
export type gint = number;
export type guint = number;
export type glong = number;
export type gulong = number;
export type gint64 = bigint;
export type guint64 = bigint;
export type gfloat = number;
export type gdouble = number;
export type gsize = number;
export type gssize = number;
export type gpointer = number; // WASM pointer
export type gconstpointer = number; // WASM pointer

// Essential GLib enums
export enum GFileTest {
  IS_REGULAR = 1 << 0,
  IS_SYMLINK = 1 << 1,
  IS_DIR = 1 << 2,
  IS_EXECUTABLE = 1 << 3,
  EXISTS = 1 << 4
}

export enum GLogLevel {
  FLAG_RECURSION = 1 << 0,
  FLAG_FATAL = 1 << 1,
  LEVEL_ERROR = 1 << 2,
  LEVEL_CRITICAL = 1 << 3,
  LEVEL_WARNING = 1 << 4,
  LEVEL_MESSAGE = 1 << 5,
  LEVEL_INFO = 1 << 6,
  LEVEL_DEBUG = 1 << 7
}

// Callback function types
export type GSourceFunc = (userData?: any) => boolean;
export type GThreadFunc = (data?: any) => any;
export type GCallback = (...args: any[]) => any;
export type GLogFunc = (logDomain: string, logLevel: GLogLevel, message: string, userData?: any) => void;

// Error handling
export interface GError {
  domain: number;
  code: number;
  message: string;
}

// Main GLib WASM module interface
export interface GLibWASMModule {
  // Core initialization and information
  glib_wasm_init(): number;
  glib_wasm_get_version(): string;
  glib_wasm_get_build_info(): string;
  glib_wasm_cleanup(): void;

  // Test functions
  glib_wasm_test_basic(): number;
  glib_wasm_test_filesystem(): number;

  // Main loop functions (exported from C)
  g_main_loop_process_frame(timestamp: number): void;
  g_main_loop_process_immediate_sources(): void;
  g_main_loop_execute_source(sourceId: number): boolean;
  g_timeout_source_execute(sourceId: number): boolean;

  // WebGPU integration (when enabled)
  g_webgpu_set_shared_device?(deviceHandle: number): void;

  // Standard Emscripten module properties
  ready: Promise<GLibWASMModule>;
  ccall: (ident: string, returnType: string | null, argTypes: string[], args: any[]) => any;
  cwrap: (ident: string, returnType: string | null, argTypes: string[]) => (...args: any[]) => any;

  // Memory access
  HEAPU8: Uint8Array;
  HEAPU32: Uint32Array;
  HEAP32: Int32Array;

  // String utilities
  UTF8ToString(ptr: number): string;
  stringToUTF8(str: string, ptr: number, maxBytesToWrite: number): void;

  // Function manipulation
  addFunction(fn: Function, sig: string): number;
  removeFunction(funcPtr: number): void;

  // Memory utilities
  getValue(ptr: number, type: string): number;
  setValue(ptr: number, value: number, type: string): void;
}

// Configuration for GLib WASM initialization
export interface GLibConfig {
  capabilities?: BrowserCapabilities;
  webgpuOrchestrator?: any;
  enableOPFS?: boolean;
  enableThreading?: boolean;
  enableWebGPU?: boolean;
  useSideModule?: boolean;
}

// Browser capability detection results
export interface BrowserCapabilities {
  webWorkers: boolean;
  sharedArrayBuffer: boolean;
  webgpu: boolean;
  webgpuTimestampQueries: boolean;
  opfs: boolean;
  webRTC: boolean;
  dynamicLinking: boolean;
  simd: boolean;
}