/* GLib WASM - Browser feature detection
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

import type { BrowserCapabilities } from './types.js';

/**
 * Detect available browser capabilities for GLib WASM
 */
export async function detectBrowserCapabilities(): Promise<BrowserCapabilities> {
  const capabilities: BrowserCapabilities = {
    webWorkers: typeof Worker !== 'undefined',
    sharedArrayBuffer: typeof SharedArrayBuffer !== 'undefined' && typeof Atomics !== 'undefined',
    webgpu: typeof navigator !== 'undefined' && 'gpu' in navigator,
    webgpuTimestampQueries: await detectWebGPUTimestampQueries(),
    opfs: typeof navigator !== 'undefined' && 'storage' in navigator && navigator.storage !== null && typeof navigator.storage === 'object' && 'getDirectory' in navigator.storage,
    webRTC: typeof RTCPeerConnection !== 'undefined',
    dynamicLinking: typeof WebAssembly !== 'undefined' && typeof WebAssembly.instantiateStreaming !== 'undefined',
    simd: await detectWebAssemblySimd(),
  };

  return capabilities;
}

/**
 * Detect WebGPU timestamp query support
 */
async function detectWebGPUTimestampQueries(): Promise<boolean> {
  if (typeof navigator === 'undefined' || !('gpu' in navigator)) return false;

  try {
    const adapter = await (navigator as any).gpu.requestAdapter();
    if (!adapter) return false;
    return adapter.features.has('timestamp-query');
  } catch {
    return false;
  }
}

/**
 * Detect WebAssembly SIMD support
 */
async function detectWebAssemblySimd(): Promise<boolean> {
  if (typeof WebAssembly === 'undefined') return false;

  try {
    // Test SIMD support with a minimal WASM module
    const simdTest = new Uint8Array([
      0, 97, 115, 109, 1, 0, 0, 0, 1, 5, 1, 96, 0, 1, 123, 3, 2, 1, 0, 10, 10, 1, 8, 0, 65, 0, 253, 15, 253, 98, 11
    ]);
    await WebAssembly.instantiate(simdTest);
    return true;
  } catch {
    return false;
  }
}