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

// Web-native capability detection
export interface GWebCapabilities {
  has_opfs: boolean;
  has_shared_array_buffer: boolean;
  has_wasm_simd: boolean;
  has_web_crypto: boolean;
  has_web_workers: boolean;
  has_fetch_api: boolean;
  has_intl_apis: boolean;
  has_request_animation_frame: boolean;
  is_deno_runtime: boolean;
  is_chrome_based: boolean;
  chrome_version: number;
}

// Legacy alias for compatibility
export interface GLibFeatures {
  webWorkers: boolean;
  sharedArrayBuffer: boolean;
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
  enableOPFS?: boolean;
  enableThreading?: boolean;
  useSideModule?: boolean;
}

// Browser capability detection results
export interface BrowserCapabilities {
  webWorkers: boolean;
  sharedArrayBuffer: boolean;
  opfs: boolean;
  webRTC: boolean;
  dynamicLinking: boolean;
  simd: boolean;
}

// GLib WASM initialization options
export interface GLibOptions {
  simdOptimizations?: boolean;
  maxMemoryMB?: number;
}

// Web-native performance and feature interfaces
export interface GWebNetworkStats {
  requests_made: number;
  bytes_downloaded: number;
  bytes_uploaded: number;
}

export interface GWebThreadingStats {
  active_threads: number;
  total_created: number;
  avg_creation_time: number;
}

export interface GWebMemoryStats {
  total_allocated: number;
  total_freed: number;
  current_usage: number;
  peak_usage: number;
  allocation_count: number;
  free_count: number;
}

export interface GWebStorageStats {
  memory_usage: number;
  opfs_usage: number;
  cache_usage: number;
  fetch_requests: number;
}

// Test result interfaces
export interface GLibTestResult {
  success: boolean;
  stringOps?: boolean;
  arrayOps?: boolean;
  hashOps?: boolean;
  fileOps?: boolean;
  dirOps?: boolean;
  simdOps?: boolean;
  cryptoOps?: boolean;
  networkOps?: boolean;
  threadingOps?: boolean;
  memoryOps?: boolean;
  error?: string;
}

export interface StringProcessResult {
  result: string;
  length: number;
  isValid: boolean;
  isValidUTF8: boolean;
  byteLength: number;
  error?: string;
}

export interface ArrayTestResult {
  success: boolean;
  count: number;
  items: string[];
  error?: string;
}

export interface HashTestResult {
  success: boolean;
  count: number;
  keys: string[];
  testLookup: boolean;
  error?: string;
}

export interface SListTestResult {
  success: boolean;
  count: number;
  items: number[];
  error?: string;
}

// Web-native specific test results
export interface WebNativeTestResult {
  success: boolean;
  capabilities?: GWebCapabilities;
  simd_performance?: number;
  crypto_performance?: number;
  network_performance?: number;
  threading_performance?: number;
  memory_performance?: number;
  storage_performance?: number;
  error?: string;
}

// Performance benchmark results
export interface BenchmarkResult {
  operation: string;
  throughput_mbps?: number;
  operations_per_sec?: number;
  time_ms: number;
  success: boolean;
  error?: string;
}

// Network request options
export interface NetworkRequestOptions {
  method?: string;
  headers?: Record<string, string>;
  body?: Uint8Array;
  timeout_ms?: number;
}

// Network response
export interface NetworkResponse {
  status_code: number;
  data?: Uint8Array;
  headers?: Record<string, string>;
  success: boolean;
  error?: string;
}

// Threading options
export interface ThreadOptions {
  name?: string;
  stack_size?: number;
  priority?: number;
}

// Memory allocation options
export interface MemoryOptions {
  track_allocations?: boolean;
  debug_info?: string;
  use_weak_refs?: boolean;
}