/* GLib WASM - Feature detection tests
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

import { describe, it, expect, vi, beforeEach } from 'vitest';
import { detectBrowserCapabilities } from '../src/lib/feature-detection';

describe('Feature Detection', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it('should detect basic browser capabilities', async () => {
    const capabilities = await detectBrowserCapabilities();

    expect(capabilities).toBeDefined();
    expect(typeof capabilities.webWorkers).toBe('boolean');
    expect(typeof capabilities.sharedArrayBuffer).toBe('boolean');
    expect(typeof capabilities.webgpu).toBe('boolean');
    expect(typeof capabilities.webgpuTimestampQueries).toBe('boolean');
    expect(typeof capabilities.opfs).toBe('boolean');
    expect(typeof capabilities.webRTC).toBe('boolean');
    expect(typeof capabilities.dynamicLinking).toBe('boolean');
    expect(typeof capabilities.simd).toBe('boolean');
  });

  it('should detect WebWorker support correctly', async () => {
    const capabilities = await detectBrowserCapabilities();

    // In test environment, Worker should not be available
    expect(capabilities.webWorkers).toBe(false);
  });

  it('should detect SharedArrayBuffer support correctly', async () => {
    const capabilities = await detectBrowserCapabilities();

    // Check if SharedArrayBuffer and Atomics are available
    const expectedSupport = typeof SharedArrayBuffer !== 'undefined' && typeof Atomics !== 'undefined';
    expect(capabilities.sharedArrayBuffer).toBe(expectedSupport);
  });

  it('should handle WebGPU detection gracefully', async () => {
    const capabilities = await detectBrowserCapabilities();

    // In test environment, WebGPU should not be available
    expect(capabilities.webgpuTimestampQueries).toBe(false);
  });

  it('should detect OPFS support correctly', async () => {
    // Mock navigator.storage as undefined (typical in test environment)
    Object.defineProperty(global.navigator, 'storage', {
      value: undefined,
      writable: true,
      configurable: true,
    });

    const capabilities = await detectBrowserCapabilities();

    expect(capabilities.opfs).toBe(false);
  });

  it('should detect WebAssembly support', async () => {
    const capabilities = await detectBrowserCapabilities();

    // WebAssembly should be available in Node.js test environment
    expect(capabilities.dynamicLinking).toBe(typeof WebAssembly.instantiateStreaming !== 'undefined');
  });
});