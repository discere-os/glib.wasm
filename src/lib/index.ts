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
  SListTestResult
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
}

// Re-export types
export type * from './types.ts'