/**
 * Type definitions for GLib WASM
 *
 * @module @discere-os/glib.wasm/types
 */

/**
 * GLib WASM Module interface
 */
export interface GLibModule {
  ccall: (funcName: string, returnType: string, argTypes: string[], args: any[]) => any;
  cwrap: (funcName: string, returnType: string, argTypes: string[]) => Function;
  UTF8ToString: (ptr: number) => string;
  HEAPU8: Uint8Array;
  FS: any;
  _malloc: (size: number) => number;
  _free: (ptr: number) => void;
  setValue: (ptr: number, value: number, type: string) => void;
  getValue: (ptr: number, type: string) => number;
}

/**
 * Web-native capabilities detected at runtime
 */
export interface WebCapabilities {
  /** WASM SIMD support (3-5x speedup for strings) */
  has_wasm_simd: boolean;
  /** WebGPU support (10x+ GPU acceleration) */
  has_webgpu: boolean;
  /** SharedArrayBuffer support (required for threading) */
  has_shared_array_buffer: boolean;
  /** Web Crypto API (5-15x crypto speedup) */
  has_web_crypto: boolean;
  /** Origin Private File System (3-4x faster than IDBFS) */
  has_opfs: boolean;
  /** Web Workers support */
  has_web_workers: boolean;
  /** Fetch API support */
  has_fetch_api: boolean;
  /** Intl APIs support */
  has_intl_apis: boolean;
  /** Running in Deno runtime */
  is_deno_runtime: boolean;
  /** Chrome-based browser */
  is_chrome_based: boolean;
  /** Chrome version (if Chrome-based) */
  chrome_version: number;
}

/**
 * GLib configuration options
 */
export interface GLibConfig {
  /** Enable SIMD optimizations (default: true) */
  simdOptimizations?: boolean;
  /** Enable threading (default: true) */
  threading?: boolean;
  /** Maximum memory in MB (default: 128) */
  maxMemoryMB?: number;
  /** Enable verbose logging (default: false) */
  verbose?: boolean;
}

/**
 * Build information
 */
export interface BuildInfo {
  version: string;
  buildType: string;
  simdEnabled: boolean;
  threadingEnabled: boolean;
  webgpuEnabled: boolean;
  buildDate: string;
}

/**
 * Performance metrics
 */
export interface PerformanceMetrics {
  /** SIMD speedup multiplier (target: 3-5x) */
  simdSpeedup: number;
  /** WebGPU speedup multiplier (target: 10x+) */
  webgpuSpeedup: number;
  /** Workers speedup multiplier (target: 10x) */
  workersSpeedup: number;
  /** Crypto speedup multiplier (target: 5-15x) */
  cryptoSpeedup: number;
}

/**
 * Memory statistics
 */
export interface MemoryStats {
  /** Current memory usage in bytes */
  current_usage: number;
  /** Peak memory usage in bytes */
  peak_usage: number;
  /** Total allocated memory in bytes */
  total_allocated: number;
  /** Number of allocations */
  allocation_count: number;
  /** Number of frees */
  free_count: number;
}

/**
 * Network statistics
 */
export interface NetworkStats {
  /** Number of network requests made */
  requests_made: number;
  /** Total bytes downloaded */
  bytes_downloaded: number;
  /** Average request time in ms */
  avg_request_time?: number;
}

/**
 * Threading statistics
 */
export interface ThreadingStats {
  /** Number of active threads */
  active_threads: number;
  /** Total threads created */
  total_created: number;
  /** Average thread creation time in ms */
  avg_creation_time: number;
}

/**
 * Storage statistics
 */
export interface StorageStats {
  /** Memory storage usage in bytes */
  memory_usage: number;
  /** OPFS storage usage in bytes */
  opfs_usage: number;
  /** Cache storage usage in bytes */
  cache_usage: number;
  /** Number of fetch requests */
  fetch_requests: number;
}

/**
 * Test result interface
 */
export interface TestResult {
  success: boolean;
  error?: string;
  [key: string]: any;
}

/**
 * String processing options
 */
export interface StringProcessingOptions {
  uppercase?: boolean;
  trimWhitespace?: boolean;
  validateUTF8?: boolean;
}

/**
 * String processing result
 */
export interface StringProcessingResult {
  isValid: boolean;
  length: number;
  isValidUTF8: boolean;
  processed?: string;
}

/**
 * Web-native benchmark results
 */
export interface WebNativeBenchmarkResults {
  simd: {
    success: boolean;
    time_ms: number;
    throughput_mbps?: number;
  };
  crypto: number;  // MB/s
  networking: number;  // KB/s
  threading: number;  // ms avg
  memory: number;  // MB/s
  storage: number;  // MB/s
  stats: {
    network: NetworkStats;
    threading: ThreadingStats;
    memory: MemoryStats;
    storage: StorageStats;
  };
}

/**
 * Checksum algorithm type
 */
export type ChecksumType = 'SHA1' | 'SHA256' | 'SHA384' | 'SHA512' | 'MD5';

/**
 * Storage tier for multi-tier storage system
 */
export enum StorageTier {
  MEMORY = 'memory',      // Fastest, temporary
  OPFS = 'opfs',         // Fast, persistent (3-4x vs IDBFS)
  CACHE = 'cache',       // Medium, browser cache
  REMOTE = 'remote'      // Slowest, network
}

/**
 * GLib error class
 */
export class GLibError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'GLibError';
  }
}
