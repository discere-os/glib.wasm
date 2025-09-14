/* GLib WASM - Demo script
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

import { detectBrowserCapabilities, initializeGLib, testFilesystem } from './lib/index.js';

async function runDemo() {
  console.log('🚀 GLib WASM Demo Starting...\n');

  try {
    // 1. Feature Detection
    console.log('1️⃣ Detecting browser capabilities...');
    const capabilities = await detectBrowserCapabilities();

    console.table({
      'Web Workers': capabilities.webWorkers ? '✅' : '❌',
      'SharedArrayBuffer': capabilities.sharedArrayBuffer ? '✅' : '❌',
      'WebGPU': capabilities.webgpu ? '✅' : '❌',
      'WebGPU Timestamp Queries': capabilities.webgpuTimestampQueries ? '✅' : '❌',
      'OPFS': capabilities.opfs ? '✅' : '❌',
      'WebRTC': capabilities.webRTC ? '✅' : '❌',
      'Dynamic Linking': capabilities.dynamicLinking ? '✅' : '❌',
      'SIMD': capabilities.simd ? '✅' : '❌',
    });

    // 2. Test OPFS filesystem if available
    console.log('\n2️⃣ Testing filesystem operations...');
    const fsResult = await testFilesystem();
    console.log(`OPFS filesystem test: ${fsResult ? '✅ Success' : '❌ Not available'}`);

    // 3. Initialize GLib (would require actual WASM build)
    console.log('\n3️⃣ GLib initialization...');
    console.log('⚠️  GLib WASM module not built yet - would initialize here');

    console.log('\n🎉 Demo completed successfully!');

  } catch (error) {
    console.error('❌ Demo failed:', error);
    process.exit(1);
  }
}

// Run demo if this is the main module
if (import.meta.url === `file://${process.argv[1]}`) {
  runDemo().catch(console.error);
}

export { runDemo };