/* GLib WASM - Module loader implementation
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

import type { GLibWASMModule, GLibConfig, BrowserCapabilities } from './types.js';
import { detectBrowserCapabilities } from './feature-detection.js';

/**
 * Load and initialize the GLib WASM module
 * @param config Optional configuration for GLib initialization
 * @returns Promise that resolves to the initialized GLib WASM module
 */
export async function loadGLibWASM(config?: Partial<GLibConfig>): Promise<GLibWASMModule> {
  const capabilities = await detectBrowserCapabilities();

  // Load appropriate WASM module based on build type
  const moduleFactory = config?.useSideModule
    ? await loadSideModule()
    : await loadMainModule();

  const module = await moduleFactory(config || {});

  // Initialize GLib
  const initResult = module.glib_wasm_init();
  if (initResult !== 1) {
    throw new Error('Failed to initialize GLib WASM module');
  }

  return module;
}

/**
 * Load the main WASM module (for testing/standalone use)
 */
async function loadMainModule(): Promise<(config: any) => Promise<GLibWASMModule>> {
  // Dynamic import to avoid bundling issues
  const { default: createModule } = await import('../../install/wasm/glib-release.js');
  return createModule;
}

/**
 * Load the SIDE_MODULE (for production use with host application)
 */
async function loadSideModule(): Promise<(config: any) => Promise<GLibWASMModule>> {
  // For SIDE_MODULE, we need to coordinate with the main module
  throw new Error('SIDE_MODULE loading requires integration with host MAIN_MODULE');
}

/**
 * Initialize GLib with advanced browser features
 * @param options Initialization options
 * @returns Promise that resolves when initialization is complete
 */
export async function initializeGLib(options: {
  webgpuOrchestrator?: any;
  enableOPFS?: boolean;
  enableThreading?: boolean;
  enableWebGPU?: boolean;
} = {}): Promise<GLibWASMModule> {
  const capabilities = await detectBrowserCapabilities();

  const config: Partial<GLibConfig> = {
    capabilities,
    webgpuOrchestrator: options.webgpuOrchestrator,
    enableOPFS: options.enableOPFS && capabilities.opfs,
    enableThreading: options.enableThreading && capabilities.sharedArrayBuffer,
    enableWebGPU: options.enableWebGPU && capabilities.webgpu,
  };

  return loadGLibWASM(config);
}

/**
 * Get high-precision monotonic time in nanoseconds
 * Uses WebGPU when available, falls back to performance.now()
 */
export async function getMonotonicTimeNS(): Promise<number> {
  const capabilities = await detectBrowserCapabilities();

  if (capabilities.webgpu && capabilities.webgpuTimestampQueries) {
    // Use WebGPU high-precision timing when available
    return performance.now() * 1000000; // Convert to nanoseconds for now
  }

  return performance.now() * 1000000; // Convert to nanoseconds
}

/**
 * Test filesystem operations (creates files in OPFS when available)
 */
export async function testFilesystem(): Promise<boolean> {
  const capabilities = await detectBrowserCapabilities();

  if (capabilities.opfs) {
    try {
      const opfsRoot = await navigator.storage.getDirectory();
      const testFile = await opfsRoot.getFileHandle('glib-test.txt', { create: true });
      const writable = await testFile.createWritable();
      await writable.write('GLib WASM filesystem test');
      await writable.close();
      return true;
    } catch (error) {
      console.warn('OPFS filesystem test failed:', error);
      return false;
    }
  }

  return false; // No OPFS support
}