/**
 * GObject WASM Dynamic Loading Validation Tests
 * Copyright © 2025 Superstruct Ltd
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Validates the GObject WASM architecture:
 * 1. Unified type registry across modules
 * 2. Unified memory management
 * 3. Cross-module signal system
 * 4. Dynamic module loading via GTypeModule
 */

import { assertEquals, assertExists } from "@std/assert";

interface GLibModule {
  ccall: (
    name: string,
    returnType: string,
    argTypes: string[],
    args: unknown[]
  ) => unknown;
  cwrap: (
    name: string,
    returnType: string,
    argTypes: string[]
  ) => (...args: unknown[]) => unknown;
  _malloc: (size: number) => number;
  _free: (ptr: number) => void;
  HEAPU8: Uint8Array;
  UTF8ToString: (ptr: number) => string;
  stringToUTF8: (str: string, ptr: number, maxBytes: number) => void;
}

interface ValidationResult {
  type_identity_preserved: boolean;
  memory_allocator_unified: boolean;
  signals_work_cross_module: boolean;
  no_memory_leaks: boolean;
  error_message: string | null;
}

async function loadGLibWasm(): Promise<GLibModule> {
  const wasmPath = "./install/wasm/glib-main.js";

  try {
    const module = await import(wasmPath);
    const createModule = module.default;
    const instance = await createModule();
    return instance as GLibModule;
  } catch (error) {
    throw new Error(`Failed to load GLib WASM: ${error}`);
  }
}

Deno.test("GObject WASM - Unified Type Registry", async () => {
  const glib = await loadGLibWasm();

  // Test: g_type_from_name returns same GType across calls
  const getType = glib.cwrap("g_type_from_name", "number", ["string"]);

  const type1 = getType("GObject") as number;
  const type2 = getType("GObject") as number;

  assertExists(type1, "First GType lookup should succeed");
  assertExists(type2, "Second GType lookup should succeed");
  assertEquals(type1, type2, "GType should be identical across calls");
  assertEquals(type1 > 0, true, "GType should be valid (> 0)");
});

Deno.test("GObject WASM - Unified Memory Allocator", async () => {
  const glib = await loadGLibWasm();

  // Test: Memory allocated via g_malloc can be freed via g_free
  const malloc = glib.cwrap("g_malloc", "number", ["number"]);
  const free = glib.cwrap("g_free", "null", ["number"]);

  const ptr1 = malloc(1024) as number;
  const ptr2 = malloc(2048) as number;

  assertExists(ptr1, "First allocation should succeed");
  assertExists(ptr2, "Second allocation should succeed");
  assertEquals(ptr1 > 0, true, "First pointer should be valid");
  assertEquals(ptr2 > 0, true, "Second pointer should be valid");
  assertEquals(ptr1 !== ptr2, true, "Pointers should be different");

  // Free memory (should not crash)
  free(ptr1);
  free(ptr2);
});

Deno.test("GObject WASM - Memory Tracking", async () => {
  const glib = await loadGLibWasm();

  // Enable memory tracking
  const enableTracking = glib.cwrap("g_wasm_memory_tracking_enable", "null", []);
  const disableTracking = glib.cwrap("g_wasm_memory_tracking_disable", "null", []);
  const dumpLeaks = glib.cwrap("g_wasm_memory_dump_leaks", "null", []);
  const malloc = glib.cwrap("g_malloc", "number", ["number"]);
  const free = glib.cwrap("g_free", "null", ["number"]);

  enableTracking();

  // Allocate and free - should not leak
  const ptr1 = malloc(100) as number;
  free(ptr1);

  // Allocate but don't free - should leak
  const ptr2 = malloc(200) as number;

  // Dump leaks (should show ptr2)
  dumpLeaks();

  // Cleanup
  free(ptr2);
  disableTracking();
});

Deno.test("GObject WASM - String Operations", async () => {
  const glib = await loadGLibWasm();

  const strdup = glib.cwrap("g_strdup", "number", ["string"]);
  const free = glib.cwrap("g_free", "null", ["number"]);

  const testStr = "Hello, WASM GObject!";
  const ptr = strdup(testStr) as number;

  assertExists(ptr, "g_strdup should succeed");
  assertEquals(ptr > 0, true, "Pointer should be valid");

  const result = glib.UTF8ToString(ptr);
  assertEquals(result, testStr, "String should be preserved");

  free(ptr);
});

Deno.test("GObject WASM - Architecture Validation", async () => {
  const glib = await loadGLibWasm();

  // Run comprehensive architecture validation
  const validate = glib.cwrap("g_wasm_validate_architecture", "number", []);
  const freeResult = glib.cwrap("g_wasm_validation_result_free", "null", ["number"]);

  const resultPtr = validate() as number;
  assertExists(resultPtr, "Validation should return result");

  // Read validation result structure
  const view = new DataView(glib.HEAPU8.buffer, resultPtr);

  const typeIdentityPreserved = view.getUint8(0) !== 0;
  const memoryAllocatorUnified = view.getUint8(1) !== 0;
  const signalsWorkCrossModule = view.getUint8(2) !== 0;
  const noMemoryLeaks = view.getUint8(3) !== 0;

  assertEquals(typeIdentityPreserved, true, "Type identity should be preserved");
  assertEquals(memoryAllocatorUnified, true, "Memory allocator should be unified");
  assertEquals(signalsWorkCrossModule, true, "Signals should work cross-module");
  assertEquals(noMemoryLeaks, true, "Should have no memory leaks");

  freeResult(resultPtr);
});

Deno.test("GObject WASM - GWasmTypeModule Creation", async () => {
  const glib = await loadGLibWasm();

  const newModule = glib.cwrap("g_wasm_type_module_new", "number", ["string", "string"]);
  const objectUnref = glib.cwrap("g_object_unref", "null", ["number"]);

  const modulePtr = newModule(
    "test-module",
    "https://wasm.discere.cloud/test/test-side.wasm"
  ) as number;

  assertExists(modulePtr, "GWasmTypeModule creation should succeed");
  assertEquals(modulePtr > 0, true, "Module pointer should be valid");

  // Cleanup
  objectUnref(modulePtr);
});

Deno.test("GObject WASM - Cross-Module Closure Creation", async () => {
  const glib = await loadGLibWasm();

  const newClosure = glib.cwrap("g_wasm_closure_new", "number", [
    "number",  // callback
    "number",  // user_data
    "number",  // destroy_notify
    "string",  // source_module
    "string"   // target_module
  ]);
  const closureUnref = glib.cwrap("g_closure_unref", "null", ["number"]);

  // Create a dummy callback function pointer (0 for test)
  const closurePtr = newClosure(
    0,           // callback (dummy)
    0,           // user_data
    0,           // destroy_notify
    "app-side",  // source module
    "gtk-side"   // target module
  ) as number;

  assertExists(closurePtr, "Closure creation should succeed");
  assertEquals(closurePtr > 0, true, "Closure pointer should be valid");

  // Cleanup
  closureUnref(closurePtr);
});

Deno.test("GObject WASM - Module Name Tracking", async () => {
  const glib = await loadGLibWasm();

  const getName = glib.cwrap("g_wasm_get_current_module_name", "string", []);
  const setName = glib.cwrap("g_wasm_set_current_module_name", "null", ["string"]);

  // Get initial name (should be "main")
  const initialName = getName() as string;
  assertEquals(initialName, "main", "Initial module should be 'main'");

  // Set new name
  setName("test-module");
  const newName = getName() as string;
  assertEquals(newName, "test-module", "Module name should update");

  // Reset to main
  setName("main");
});

Deno.test("GObject WASM - Performance Baseline", async () => {
  const glib = await loadGLibWasm();

  const malloc = glib.cwrap("g_malloc", "number", ["number"]);
  const free = glib.cwrap("g_free", "null", ["number"]);
  const strdup = glib.cwrap("g_strdup", "number", ["string"]);

  const iterations = 10000;

  // Benchmark: malloc/free
  const mallocStart = performance.now();
  for (let i = 0; i < iterations; i++) {
    const ptr = malloc(64) as number;
    free(ptr);
  }
  const mallocTime = performance.now() - mallocStart;

  // Benchmark: string duplication
  const strdupStart = performance.now();
  for (let i = 0; i < iterations; i++) {
    const ptr = strdup("test string") as number;
    free(ptr);
  }
  const strdupTime = performance.now() - strdupStart;

  console.log(`\nPerformance Results (${iterations} iterations):`);
  console.log(`  malloc/free: ${(mallocTime).toFixed(2)}ms (${(iterations/mallocTime*1000).toFixed(0)} ops/sec)`);
  console.log(`  g_strdup: ${(strdupTime).toFixed(2)}ms (${(iterations/strdupTime*1000).toFixed(0)} ops/sec)`);

  // Sanity checks - should complete reasonably fast
  assertEquals(mallocTime < 1000, true, "malloc/free should be fast");
  assertEquals(strdupTime < 1000, true, "g_strdup should be fast");
});
