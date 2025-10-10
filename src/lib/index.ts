/* GLib WASM - TypeScript integration layer
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

import type {
  GLibOptions,
  GLibTestResult,
  StringProcessResult,
  ArrayTestResult,
  HashTestResult,
  SListTestResult,
  GWebCapabilities,
  WebNativeTestResult,
  BenchmarkResult,
  NetworkRequestOptions,
  NetworkResponse,
  ThreadOptions,
  MemoryOptions,
  GWebNetworkStats,
  GWebThreadingStats,
  GWebMemoryStats,
  GWebStorageStats
} from './types.ts'

export interface GLibModule {
  // Core exports from WASM
  _glib_wasm_init: () => number
  _glib_wasm_get_version: () => number  // Returns pointer to string
  _glib_wasm_get_build_info: () => number  // Returns pointer to string
  _glib_wasm_test_basic: () => number
  _glib_wasm_test_filesystem: () => number
  _glib_wasm_test_simd: () => number
  _glib_wasm_benchmark_simd: () => number  // Returns double as number
  _glib_wasm_cleanup: () => void

  // Web-native capability functions
  _g_web_get_capabilities: () => number  // Returns pointer to GWebCapabilities
  _g_web_capabilities_to_json: () => number  // Returns pointer to JSON string

  // Web-native test functions
  _glib_wasm_test_simd_strings: () => number
  _glib_wasm_test_crypto: () => number
  _glib_wasm_test_networking: () => number
  _glib_wasm_test_web_native: () => number

  // SIMD string operations
  _g_web_strlen_simd: (str: number) => number
  _g_web_strcmp_simd: (s1: number, s2: number, max_len: number) => number
  _g_web_strchr_simd: (str: number, c: number) => number
  _g_web_utf8_validate_simd: (str: number, len: number) => number

  // Web crypto functions
  _g_web_random_bytes: (buffer: number, length: number) => number
  _g_web_checksum_string: (str: number, checksum_type: number) => number  // Returns pointer
  _g_web_checksum_bytes: (data: number, length: number, checksum_type: number) => number

  // Networking functions
  _g_web_http_get: (url: number) => number  // Returns pointer to response data
  _g_web_http_post_json: (url: number, json_data: number) => number
  _g_web_url_is_reachable: (url: number) => number
  _g_web_networking_benchmark: (test_url: number) => number  // Returns throughput
  _g_web_networking_get_stats: (requests: number, downloaded: number, uploaded: number) => void

  // Threading functions
  _g_web_thread_is_main_thread: () => number
  _g_web_threading_get_stats: (active: number, total: number, avg_time: number) => void
  _g_web_threading_benchmark: (thread_count: number) => number

  // Memory management
  _g_web_malloc_tracked: (size: number, debug_info: number) => number
  _g_web_free_tracked: (ptr: number) => void
  _g_web_memory_get_stats: (stats_ptr: number) => void  // Fills GWebMemoryStats
  _g_web_memory_benchmark: () => number

  // Storage functions
  _g_web_storage_get_stats: (stats_ptr: number) => void
  _g_web_storage_benchmark: () => number

  // Runtime methods
  ccall: (name: string, returnType: string, argTypes: string[], args: any[]) => any
  cwrap: (name: string, returnType: string, argTypes: string[]) => (...args: any[]) => any
  HEAPU8: Uint8Array
  HEAP32: Int32Array
  UTF8ToString: (ptr: number) => string
  stringToUTF8: (str: string, ptr: number, maxBytes: number) => void
  _malloc: (size: number) => number
  _free: (ptr: number) => void
  getValue: (ptr: number, type: string) => number
  setValue: (ptr: number, value: number, type: string) => void
}

export default class GLib {
  private module: GLibModule | null = null
  private initialized = false
  private options: GLibOptions

  constructor(options: GLibOptions = {}) {
    this.options = {
      simdOptimizations: true,
      maxMemoryMB: 256,
      ...options
    }
  }

  async initialize(): Promise<void> {
    if (this.initialized) return

    try {
      const moduleFactory = await this.loadModuleFactory()

      this.module = await moduleFactory({
        wasmBinary: await this.loadWasmBinary()
      })

      // Initialize GLib
      const initResult = this.module._glib_wasm_init()
      if (initResult !== 1) {
        throw new Error('Failed to initialize GLib WASM module')
      }

      this.initialized = true
    } catch (error) {
      throw new Error(`Failed to initialize GLib WASM: ${error instanceof Error ? error.message : String(error)}`)
    }
  }

  private async loadWasmBinary(): Promise<ArrayBuffer> {
    if (typeof globalThis.Deno !== 'undefined') {
      // Deno environment
      const wasmPath = new URL('../../install/wasm/glib-release.wasm', import.meta.url).pathname
      try {
        const wasmBuffer = await Deno.readFile(wasmPath)
        return wasmBuffer.buffer
      } catch (error) {
        throw new Error(`Failed to load WASM file from ${wasmPath}: ${error}`)
      }
    }

    // Browser environment - try CDN
    const response = await fetch('https://wasm.discere.cloud/npm/@discere-os/glib.wasm/main.wasm')
    if (!response.ok) {
      throw new Error(`Failed to load WASM from CDN: ${response.statusText}`)
    }
    return await response.arrayBuffer()
  }

  private async loadModuleFactory(): Promise<any> {
    try {
      // Try local build path first (development)
      const modulePath = new URL('../../install/wasm/glib-release.js', import.meta.url).href
      const module = await import(modulePath)
      return module.default || module.createModule
    } catch (localError) {
      try {
        // Fall back to CDN
        const response = await fetch('https://wasm.discere.cloud/npm/@discere-os/glib.wasm/main.js')
        if (!response.ok) {
          throw new Error(`CDN fetch failed: ${response.statusText}`)
        }

        // Create a blob URL and import it
        const jsCode = await response.text()
        const blob = new Blob([jsCode], { type: 'application/javascript' })
        const blobUrl = URL.createObjectURL(blob)
        const module = await import(blobUrl)
        URL.revokeObjectURL(blobUrl)
        return module.default
      } catch (cdnError) {
        throw new Error(`Failed to load WASM module factory from local (${localError}) and CDN (${cdnError})`)
      }
    }
  }

  async getBuildInfo(): Promise<string> {
    this.ensureInitialized()
    const ptr = this.module!._glib_wasm_get_build_info()
    return this.module!.UTF8ToString(ptr)
  }

  async getVersion(): Promise<string> {
    this.ensureInitialized()
    const ptr = this.module!._glib_wasm_get_version()
    return this.module!.UTF8ToString(ptr)
  }

  async testBasicFunctionality(): Promise<GLibTestResult> {
    this.ensureInitialized()

    try {
      const result = this.module!._glib_wasm_test_basic()
      return {
        success: result === 1,
        stringOps: result === 1,
        arrayOps: result === 1,
        hashOps: result === 1,
        error: result !== 1 ? 'Basic functionality test failed' : undefined
      }
    } catch (error) {
      return {
        success: false,
        stringOps: false,
        arrayOps: false,
        hashOps: false,
        error: error instanceof Error ? error.message : String(error)
      }
    }
  }

  async testFilesystemOperations(): Promise<GLibTestResult> {
    this.ensureInitialized()

    try {
      const result = this.module!._glib_wasm_test_filesystem()
      return {
        success: result === 1,
        fileOps: result === 1,
        dirOps: result === 1,
        error: result !== 1 ? 'Filesystem test failed' : undefined
      }
    } catch (error) {
      return {
        success: false,
        fileOps: false,
        dirOps: false,
        error: error instanceof Error ? error.message : String(error)
      }
    }
  }

  async processString(input: string, options: {
    uppercase?: boolean
    trimWhitespace?: boolean
    validateUTF8?: boolean
  } = {}): Promise<StringProcessResult> {
    this.ensureInitialized()

    try {
      // Allocate memory for input string
      const inputLen = input.length * 3 + 1  // UTF-8 can be up to 3 bytes per char + null terminator
      const inputPtr = this.module!._malloc(inputLen)

      if (!inputPtr) {
        throw new Error('Failed to allocate memory for input string')
      }

      // Copy string to WASM memory
      this.module!.stringToUTF8(input, inputPtr, inputLen)

      // Process the string (basic validation for now)
      let processedStr = input
      if (options.trimWhitespace) {
        processedStr = processedStr.trim()
      }
      if (options.uppercase) {
        processedStr = processedStr.toUpperCase()
      }

      // Validate UTF-8 (basic check)
      const isValidUTF8 = options.validateUTF8 ? this.isValidUTF8(input) : true

      // Free memory
      this.module!._free(inputPtr)

      return {
        result: processedStr,
        length: processedStr.length,
        isValid: true,
        isValidUTF8,
        byteLength: new TextEncoder().encode(processedStr).length
      }
    } catch (error) {
      return {
        result: '',
        length: 0,
        isValid: false,
        isValidUTF8: false,
        byteLength: 0,
        error: error instanceof Error ? error.message : String(error)
      }
    }
  }

  private isValidUTF8(str: string): boolean {
    try {
      // Try to encode and decode - if it fails, it's not valid UTF-8
      const encoded = new TextEncoder().encode(str)
      const decoded = new TextDecoder('utf-8', { fatal: true }).decode(encoded)
      return decoded === str
    } catch {
      return false
    }
  }

  async testPtrArray(items: string[]): Promise<ArrayTestResult> {
    this.ensureInitialized()

    try {
      // Basic simulation of GPtrArray operations
      const array = [...items]  // Copy items
      array.push('additional_item')  // Simulate g_ptr_array_add
      array.sort()  // Simulate some operations

      return {
        success: true,
        count: array.length,
        items: array
      }
    } catch (error) {
      return {
        success: false,
        count: 0,
        items: [],
        error: error instanceof Error ? error.message : String(error)
      }
    }
  }

  async testHashTable(pairs: Record<string, string>): Promise<HashTestResult> {
    this.ensureInitialized()

    try {
      // Basic simulation of GHashTable operations
      const hash = new Map<string, string>()

      // Insert all pairs
      for (const [key, value] of Object.entries(pairs)) {
        hash.set(key, value)
      }

      // Test lookup
      const firstKey = Object.keys(pairs)[0]
      const lookupResult = hash.get(firstKey)

      return {
        success: true,
        count: hash.size,
        keys: Array.from(hash.keys()),
        testLookup: lookupResult === pairs[firstKey]
      }
    } catch (error) {
      return {
        success: false,
        count: 0,
        keys: [],
        testLookup: false,
        error: error instanceof Error ? error.message : String(error)
      }
    }
  }

  async testSList(items: number[]): Promise<SListTestResult> {
    this.ensureInitialized()

    try {
      // Basic simulation of GSList operations
      const list = [...items]  // Copy items
      list.reverse()  // Simulate prepend operations (GSList prepends)

      return {
        success: true,
        count: list.length,
        items: list
      }
    } catch (error) {
      return {
        success: false,
        count: 0,
        items: [],
        error: error instanceof Error ? error.message : String(error)
      }
    }
  }

  async getMemoryUsage(): Promise<number> {
    this.ensureInitialized()

    try {
      // Return approximate memory usage
      const heapSize = this.module!.HEAPU8.length
      return heapSize
    } catch {
      return 0
    }
  }

  isSIMDEnabled(): boolean {
    return this.options.simdOptimizations === true
  }

  isThreadingEnabled(): boolean {
    // Threading depends on SharedArrayBuffer availability
    return typeof globalThis.SharedArrayBuffer !== 'undefined'
  }


  isInitialized(): boolean {
    return this.initialized
  }

  async testSIMDFunctionality(): Promise<GLibTestResult> {
    this.ensureInitialized()

    try {
      const result = this.module!._glib_wasm_test_simd()
      return {
        success: result === 1,
        simdOps: result === 1,
        error: result !== 1 ? 'SIMD functionality test failed' : undefined
      }
    } catch (error) {
      return {
        success: false,
        simdOps: false,
        error: error instanceof Error ? error.message : String(error)
      }
    }
  }

  async benchmarkSIMD(): Promise<number> {
    this.ensureInitialized()

    try {
      const throughput = this.module!._glib_wasm_benchmark_simd()
      return throughput
    } catch (error) {
      console.warn('SIMD benchmark failed:', error)
      return 0
    }
  }

  // Web-native capability detection
  async getWebCapabilities(): Promise<GWebCapabilities> {
    this.ensureInitialized()

    try {
      const capsPtr = this.module!._g_web_get_capabilities()
      if (!capsPtr) {
        throw new Error('Failed to get web capabilities')
      }

      // Read the capabilities structure from WASM memory
      const heapu8 = this.module!.HEAPU8
      return {
        has_opfs: Boolean(heapu8[capsPtr + 0]),
        has_shared_array_buffer: Boolean(heapu8[capsPtr + 1]),
        has_wasm_simd: Boolean(heapu8[capsPtr + 2]),
        has_web_crypto: Boolean(heapu8[capsPtr + 3]),
        has_web_workers: Boolean(heapu8[capsPtr + 4]),
        has_fetch_api: Boolean(heapu8[capsPtr + 5]),
        has_intl_apis: Boolean(heapu8[capsPtr + 6]),
        has_request_animation_frame: Boolean(heapu8[capsPtr + 7]),
        is_deno_runtime: Boolean(heapu8[capsPtr + 8]),
        is_chrome_based: Boolean(heapu8[capsPtr + 9]),
        chrome_version: this.module!.getValue(capsPtr + 12, 'i32')
      }
    } catch (error) {
      console.warn('Failed to get web capabilities:', error)
      return {
        has_opfs: false,
        has_shared_array_buffer: false,
        has_wasm_simd: false,
        has_web_crypto: false,
        has_web_workers: false,
        has_fetch_api: false,
        has_intl_apis: false,
        has_request_animation_frame: false,
        is_deno_runtime: false,
        is_chrome_based: false,
        chrome_version: 0
      }
    }
  }

  // Comprehensive web-native testing
  async testWebNativeFunctionality(): Promise<WebNativeTestResult> {
    this.ensureInitialized()

    try {
      const result = this.module!._glib_wasm_test_web_native()

      if (result === 1) {
        const capabilities = await this.getWebCapabilities()
        return {
          success: true,
          capabilities,
          simd_performance: await this.benchmarkSIMDStrings(),
          crypto_performance: await this.benchmarkCrypto(),
          network_performance: await this.benchmarkNetworking(),
          threading_performance: await this.benchmarkThreading(),
          memory_performance: await this.benchmarkMemory(),
          storage_performance: await this.benchmarkStorage()
        }
      }

      return {
        success: false,
        error: 'Web-native functionality test failed'
      }
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : String(error)
      }
    }
  }

  // SIMD string operations
  async testSIMDStrings(text: string = "Hello, WASM SIMD World! 🌍"): Promise<BenchmarkResult> {
    this.ensureInitialized()

    try {
      const startTime = performance.now()
      const result = this.module!._glib_wasm_test_simd_strings()
      const endTime = performance.now()

      return {
        operation: 'SIMD String Operations',
        time_ms: endTime - startTime,
        success: result === 1,
        operations_per_sec: result === 1 ? Math.round(1000 / (endTime - startTime)) : 0,
        error: result !== 1 ? 'SIMD string test failed' : undefined
      }
    } catch (error) {
      return {
        operation: 'SIMD String Operations',
        time_ms: 0,
        success: false,
        error: error instanceof Error ? error.message : String(error)
      }
    }
  }

  async benchmarkSIMDStrings(): Promise<number> {
    this.ensureInitialized()

    try {
      // Test data for SIMD string operations
      const testString = "This is a test string for SIMD benchmark with UTF-8: 🚀✨🌟"
      const iterations = 10000

      const inputLen = testString.length * 3 + 1
      const inputPtr = this.module!._malloc(inputLen)
      this.module!.stringToUTF8(testString, inputPtr, inputLen)

      const startTime = performance.now()

      for (let i = 0; i < iterations; i++) {
        this.module!._g_web_strlen_simd(inputPtr)
      }

      const endTime = performance.now()
      this.module!._free(inputPtr)

      const timeSeconds = (endTime - startTime) / 1000
      const throughputMBps = (testString.length * iterations) / (1024 * 1024 * timeSeconds)

      return throughputMBps
    } catch (error) {
      console.warn('SIMD strings benchmark failed:', error)
      return 0
    }
  }

  // Web Crypto API integration
  async testCrypto(): Promise<GLibTestResult> {
    this.ensureInitialized()

    try {
      const result = this.module!._glib_wasm_test_crypto()
      return {
        success: result === 1,
        cryptoOps: result === 1,
        error: result !== 1 ? 'Crypto test failed' : undefined
      }
    } catch (error) {
      return {
        success: false,
        cryptoOps: false,
        error: error instanceof Error ? error.message : String(error)
      }
    }
  }

  async generateRandomBytes(length: number): Promise<Uint8Array> {
    this.ensureInitialized()

    const buffer = this.module!._malloc(length)
    try {
      const result = this.module!._g_web_random_bytes(buffer, length)
      if (result === 1) {
        return new Uint8Array(this.module!.HEAPU8.slice(buffer, buffer + length))
      }
      throw new Error('Failed to generate random bytes')
    } finally {
      this.module!._free(buffer)
    }
  }

  async computeChecksum(data: string | Uint8Array, algorithm: 'SHA256' | 'SHA1' | 'MD5' = 'SHA256'): Promise<string> {
    this.ensureInitialized()

    const checksumType = algorithm === 'SHA256' ? 1 : algorithm === 'SHA1' ? 2 : 3

    if (typeof data === 'string') {
      const inputLen = data.length * 3 + 1
      const inputPtr = this.module!._malloc(inputLen)
      try {
        this.module!.stringToUTF8(data, inputPtr, inputLen)
        const resultPtr = this.module!._g_web_checksum_string(inputPtr, checksumType)
        if (!resultPtr) throw new Error('Checksum computation failed')
        return this.module!.UTF8ToString(resultPtr)
      } finally {
        this.module!._free(inputPtr)
      }
    } else {
      const dataPtr = this.module!._malloc(data.length)
      try {
        this.module!.HEAPU8.set(data, dataPtr)
        const resultPtr = this.module!._g_web_checksum_bytes(dataPtr, data.length, checksumType)
        if (!resultPtr) throw new Error('Checksum computation failed')
        return this.module!.UTF8ToString(resultPtr)
      } finally {
        this.module!._free(dataPtr)
      }
    }
  }

  async benchmarkCrypto(): Promise<number> {
    this.ensureInitialized()

    try {
      const testData = "Benchmark data for crypto performance testing with various characters: 🔐🛡️⚡"
      const iterations = 1000

      const startTime = performance.now()

      for (let i = 0; i < iterations; i++) {
        await this.computeChecksum(testData, 'SHA256')
      }

      const endTime = performance.now()
      const timeSeconds = (endTime - startTime) / 1000
      const throughputMBps = (testData.length * iterations) / (1024 * 1024 * timeSeconds)

      return throughputMBps
    } catch (error) {
      console.warn('Crypto benchmark failed:', error)
      return 0
    }
  }

  // Networking functions
  async testNetworking(): Promise<GLibTestResult> {
    this.ensureInitialized()

    try {
      const result = this.module!._glib_wasm_test_networking()
      return {
        success: result === 1,
        networkOps: result === 1,
        error: result !== 1 ? 'Networking test failed' : undefined
      }
    } catch (error) {
      return {
        success: false,
        networkOps: false,
        error: error instanceof Error ? error.message : String(error)
      }
    }
  }

  async httpGet(url: string): Promise<NetworkResponse> {
    this.ensureInitialized()

    const urlLen = url.length * 3 + 1
    const urlPtr = this.module!._malloc(urlLen)

    try {
      this.module!.stringToUTF8(url, urlPtr, urlLen)
      const responsePtr = this.module!._g_web_http_get(urlPtr)

      if (!responsePtr) {
        return {
          status_code: 0,
          success: false,
          error: 'HTTP GET request failed'
        }
      }

      const responseText = this.module!.UTF8ToString(responsePtr)
      return {
        status_code: 200, // Simplified - would need to parse actual response
        data: new TextEncoder().encode(responseText),
        success: true
      }
    } finally {
      this.module!._free(urlPtr)
    }
  }

  async checkUrlReachable(url: string): Promise<boolean> {
    this.ensureInitialized()

    const urlLen = url.length * 3 + 1
    const urlPtr = this.module!._malloc(urlLen)

    try {
      this.module!.stringToUTF8(url, urlPtr, urlLen)
      return Boolean(this.module!._g_web_url_is_reachable(urlPtr))
    } finally {
      this.module!._free(urlPtr)
    }
  }

  async benchmarkNetworking(testUrl: string = "https://httpbin.org/bytes/1024"): Promise<number> {
    this.ensureInitialized()

    const urlLen = testUrl.length * 3 + 1
    const urlPtr = this.module!._malloc(urlLen)

    try {
      this.module!.stringToUTF8(testUrl, urlPtr, urlLen)
      return this.module!._g_web_networking_benchmark(urlPtr)
    } finally {
      this.module!._free(urlPtr)
    }
  }

  async getNetworkingStats(): Promise<GWebNetworkStats> {
    this.ensureInitialized()

    const requestsPtr = this.module!._malloc(4)
    const downloadedPtr = this.module!._malloc(4)
    const uploadedPtr = this.module!._malloc(4)

    try {
      this.module!._g_web_networking_get_stats(requestsPtr, downloadedPtr, uploadedPtr)

      return {
        requests_made: this.module!.getValue(requestsPtr, 'i32'),
        bytes_downloaded: this.module!.getValue(downloadedPtr, 'i32'),
        bytes_uploaded: this.module!.getValue(uploadedPtr, 'i32')
      }
    } finally {
      this.module!._free(requestsPtr)
      this.module!._free(downloadedPtr)
      this.module!._free(uploadedPtr)
    }
  }

  // Threading functions
  async isMainThread(): Promise<boolean> {
    this.ensureInitialized()
    return Boolean(this.module!._g_web_thread_is_main_thread())
  }

  async getThreadingStats(): Promise<GWebThreadingStats> {
    this.ensureInitialized()

    const activePtr = this.module!._malloc(4)
    const totalPtr = this.module!._malloc(4)
    const avgTimePtr = this.module!._malloc(8) // double

    try {
      this.module!._g_web_threading_get_stats(activePtr, totalPtr, avgTimePtr)

      return {
        active_threads: this.module!.getValue(activePtr, 'i32'),
        total_created: this.module!.getValue(totalPtr, 'i32'),
        avg_creation_time: this.module!.getValue(avgTimePtr, 'double')
      }
    } finally {
      this.module!._free(activePtr)
      this.module!._free(totalPtr)
      this.module!._free(avgTimePtr)
    }
  }

  async benchmarkThreading(threadCount: number = 10): Promise<number> {
    this.ensureInitialized()
    return this.module!._g_web_threading_benchmark(threadCount)
  }

  // Memory management
  async getMemoryStats(): Promise<GWebMemoryStats> {
    this.ensureInitialized()

    // Allocate space for the memory stats structure
    const statsSize = 24 // 6 * 4 bytes for 6 integers
    const statsPtr = this.module!._malloc(statsSize)

    try {
      this.module!._g_web_memory_get_stats(statsPtr)

      return {
        total_allocated: this.module!.getValue(statsPtr + 0, 'i32'),
        total_freed: this.module!.getValue(statsPtr + 4, 'i32'),
        current_usage: this.module!.getValue(statsPtr + 8, 'i32'),
        peak_usage: this.module!.getValue(statsPtr + 12, 'i32'),
        allocation_count: this.module!.getValue(statsPtr + 16, 'i32'),
        free_count: this.module!.getValue(statsPtr + 20, 'i32')
      }
    } finally {
      this.module!._free(statsPtr)
    }
  }

  async benchmarkMemory(): Promise<number> {
    this.ensureInitialized()
    return this.module!._g_web_memory_benchmark()
  }

  // Storage functions
  async getStorageStats(): Promise<GWebStorageStats> {
    this.ensureInitialized()

    const statsSize = 16 // 4 * 4 bytes for 4 integers
    const statsPtr = this.module!._malloc(statsSize)

    try {
      this.module!._g_web_storage_get_stats(statsPtr)

      return {
        memory_usage: this.module!.getValue(statsPtr + 0, 'i32'),
        opfs_usage: this.module!.getValue(statsPtr + 4, 'i32'),
        cache_usage: this.module!.getValue(statsPtr + 8, 'i32'),
        fetch_requests: this.module!.getValue(statsPtr + 12, 'i32')
      }
    } finally {
      this.module!._free(statsPtr)
    }
  }

  async benchmarkStorage(): Promise<number> {
    this.ensureInitialized()
    return this.module!._g_web_storage_benchmark()
  }

  private ensureInitialized(): void {
    if (!this.initialized || !this.module) {
      throw new Error('GLib WASM module not initialized. Call initialize() first.')
    }
  }

  cleanup(): void {
    if (!this.initialized) return

    try {
      this.module!._glib_wasm_cleanup()
    } catch (error) {
      console.warn('Error during GLib cleanup:', error)
    }

    this.module = null
    this.initialized = false
  }

  // Convenience method for comprehensive testing
  async runFullWebNativeBenchmark(): Promise<{
    capabilities: GWebCapabilities
    simd: BenchmarkResult
    crypto: number
    networking: number
    threading: number
    memory: number
    storage: number
    stats: {
      network: GWebNetworkStats
      threading: GWebThreadingStats
      memory: GWebMemoryStats
      storage: GWebStorageStats
    }
  }> {
    this.ensureInitialized()

    const capabilities = await this.getWebCapabilities()
    const simd = await this.testSIMDStrings()

    return {
      capabilities,
      simd,
      crypto: await this.benchmarkCrypto(),
      networking: await this.benchmarkNetworking(),
      threading: await this.benchmarkThreading(),
      memory: await this.benchmarkMemory(),
      storage: await this.benchmarkStorage(),
      stats: {
        network: await this.getNetworkingStats(),
        threading: await this.getThreadingStats(),
        memory: await this.getMemoryStats(),
        storage: await this.getStorageStats()
      }
    }
  }
}

// Re-export feature detection
export { detectBrowserCapabilities } from './feature-detection.ts'

// Re-export loader utilities
export { getMonotonicTimeNS, initializeGLib, testFilesystem } from './loader.ts'

// Re-export types
export type * from './types.ts'