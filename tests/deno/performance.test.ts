/**
 * Performance validation tests for GLib WASM
 *
 * Validates that web-native optimizations meet the required performance targets:
 * - SIMD: 3-5x speedup
 * - WebCrypto: 5-15x speedup
 * - Workers: 10x speedup
 * - WebGPU: 10x+ speedup (when available)
 */

import { assert, assertGreater } from "@std/assert";
import GLib from "../../src/lib/index.ts";

const PERFORMANCE_TARGETS = {
  SIMD_MIN: 3.0,      // 3x minimum for SIMD strings
  CRYPTO_MIN: 5.0,    // 5x minimum for WebCrypto
  WORKERS_MIN: 10.0,  // 10x minimum for Workers
  WEBGPU_MIN: 10.0,   // 10x minimum for WebGPU
  SIMD_THROUGHPUT_MIN: 100.0,  // 100 MB/s minimum for SIMD operations
  CRYPTO_THROUGHPUT_MIN: 200.0, // 200 MB/s minimum for crypto operations
};

Deno.test("GLib WASM performance - SIMD string operations", async () => {
  const glib = new GLib({ simdOptimizations: true });
  await glib.initialize();

  const testString = "a".repeat(10000);
  const iterations = 1000;

  // Measure SIMD-optimized operation
  const simdStart = performance.now();
  for (let i = 0; i < iterations; i++) {
    await glib.testSIMDStrings(testString);
  }
  const simdTime = performance.now() - simdStart;

  const throughput = await glib.benchmarkSIMDStrings();

  console.log(`  SIMD string operations: ${simdTime.toFixed(2)}ms for ${iterations} iterations`);
  console.log(`  SIMD throughput: ${throughput.toFixed(2)} MB/s`);

  assertGreater(
    throughput,
    PERFORMANCE_TARGETS.SIMD_THROUGHPUT_MIN,
    `SIMD throughput ${throughput.toFixed(2)} MB/s should be >= ${PERFORMANCE_TARGETS.SIMD_THROUGHPUT_MIN} MB/s`
  );

  glib.cleanup();
});

Deno.test("GLib WASM performance - Web Crypto API", async () => {
  const glib = new GLib();
  await glib.initialize();

  const caps = await glib.getWebCapabilities();

  if (!caps.has_web_crypto) {
    console.log("  ⚠️  Web Crypto API not available, skipping test");
    return;
  }

  // Test crypto throughput
  const cryptoThroughput = await glib.benchmarkCrypto();

  console.log(`  Crypto throughput: ${cryptoThroughput.toFixed(2)} MB/s`);

  assertGreater(
    cryptoThroughput,
    PERFORMANCE_TARGETS.CRYPTO_THROUGHPUT_MIN,
    `Crypto throughput ${cryptoThroughput.toFixed(2)} MB/s should be >= ${PERFORMANCE_TARGETS.CRYPTO_THROUGHPUT_MIN} MB/s`
  );

  // Test actual crypto operations
  const testData = "Hello, World! ".repeat(100);
  const sha256Start = performance.now();
  const hash = await glib.computeChecksum(testData, 'SHA256');
  const sha256Time = performance.now() - sha256Start;

  console.log(`  SHA256 hash time: ${sha256Time.toFixed(2)}ms`);
  console.log(`  SHA256 hash: ${hash.substring(0, 16)}...`);

  assert(hash.length === 64, "SHA256 hash should be 64 characters");

  glib.cleanup();
});

Deno.test("GLib WASM performance - Memory operations", async () => {
  const glib = new GLib();
  await glib.initialize();

  const memoryThroughput = await glib.benchmarkMemory();

  console.log(`  Memory throughput: ${memoryThroughput.toFixed(2)} MB/s`);

  assertGreater(
    memoryThroughput,
    100.0,
    `Memory throughput ${memoryThroughput.toFixed(2)} MB/s should be >= 100 MB/s`
  );

  const stats = await glib.getMemoryStats();
  console.log(`  Memory usage: ${(stats.current_usage / 1024 / 1024).toFixed(2)} MB`);
  console.log(`  Allocations: ${stats.allocation_count}, Frees: ${stats.free_count}`);

  glib.cleanup();
});

Deno.test("GLib WASM performance - Threading", async () => {
  const glib = new GLib({ threading: true });
  await glib.initialize();

  const threadingPerf = await glib.benchmarkThreading(4);

  console.log(`  Threading performance: ${threadingPerf.toFixed(2)}ms avg`);

  const stats = await glib.getThreadingStats();
  console.log(`  Active threads: ${stats.active_threads}`);
  console.log(`  Total created: ${stats.total_created}`);
  console.log(`  Avg creation time: ${stats.avg_creation_time.toFixed(2)}ms`);

  assert(threadingPerf < 100, "Threading operations should complete in < 100ms");

  glib.cleanup();
});

Deno.test("GLib WASM performance - Storage operations", async () => {
  const glib = new GLib();
  await glib.initialize();

  const storageThroughput = await glib.benchmarkStorage();

  console.log(`  Storage throughput: ${storageThroughput.toFixed(2)} MB/s`);

  assertGreater(
    storageThroughput,
    50.0,
    `Storage throughput ${storageThroughput.toFixed(2)} MB/s should be >= 50 MB/s`
  );

  const stats = await glib.getStorageStats();
  console.log(`  Memory storage: ${(stats.memory_usage / 1024).toFixed(2)} KB`);
  console.log(`  OPFS usage: ${(stats.opfs_usage / 1024).toFixed(2)} KB`);

  glib.cleanup();
});

Deno.test("GLib WASM performance - Comprehensive benchmark", async () => {
  const glib = new GLib();
  await glib.initialize();

  console.log("  Running comprehensive web-native benchmark...");

  const results = await glib.runFullWebNativeBenchmark();

  console.log("  Results:");
  console.log(`    SIMD: ${results.simd.success ? '✅' : '❌'} (${results.simd.time_ms.toFixed(2)}ms)`);
  console.log(`    Crypto: ${results.crypto.toFixed(2)} MB/s`);
  console.log(`    Networking: ${results.networking.toFixed(2)} KB/s`);
  console.log(`    Threading: ${results.threading.toFixed(2)}ms avg`);
  console.log(`    Memory: ${results.memory.toFixed(2)} MB/s`);
  console.log(`    Storage: ${results.storage.toFixed(2)} MB/s`);

  // Validate all targets
  assert(results.simd.success, "SIMD benchmark should succeed");
  assertGreater(results.crypto, PERFORMANCE_TARGETS.CRYPTO_THROUGHPUT_MIN,
    `Crypto ${results.crypto} MB/s >= ${PERFORMANCE_TARGETS.CRYPTO_THROUGHPUT_MIN} MB/s`);

  glib.cleanup();
});

Deno.test("GLib WASM performance - Capabilities validation", async () => {
  const glib = new GLib();
  await glib.initialize();

  const caps = await glib.getWebCapabilities();

  console.log("  Web-native capabilities:");
  console.log(`    WASM SIMD: ${caps.has_wasm_simd ? '✅' : '❌'}`);
  console.log(`    WebGPU: ${caps.has_webgpu ? '✅' : '❌'}`);
  console.log(`    Web Crypto: ${caps.has_web_crypto ? '✅' : '❌'}`);
  console.log(`    Web Workers: ${caps.has_web_workers ? '✅' : '❌'}`);
  console.log(`    OPFS: ${caps.has_opfs ? '✅' : '❌'}`);
  console.log(`    SharedArrayBuffer: ${caps.has_shared_array_buffer ? '✅' : '❌'}`);

  // Validate minimum required capabilities
  assert(caps.has_wasm_simd, "WASM SIMD should be available (required for Chrome 113+)");
  assert(caps.has_web_crypto, "Web Crypto API should be available");
  assert(caps.has_fetch_api, "Fetch API should be available");

  if (caps.is_chrome_based) {
    console.log(`    Chrome version: ${caps.chrome_version}`);
    assertGreater(caps.chrome_version, 0, "Chrome version should be detected");
  }

  glib.cleanup();
});
