/* GLib WASM - Test setup
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

import { vi } from 'vitest';

// Mock browser APIs that may not be available in happy-dom
if (!global.navigator) {
  // @ts-ignore
  global.navigator = {};
}

Object.assign(global.navigator, {
  gpu: undefined,
  storage: undefined,
});

// Mock performance if not available
global.performance = global.performance || {
  now: vi.fn(() => Date.now()),
};

// Mock WebAssembly if not available
global.WebAssembly = global.WebAssembly || {
  instantiate: vi.fn(),
  instantiateStreaming: vi.fn(),
};

console.log('GLib WASM test setup completed');