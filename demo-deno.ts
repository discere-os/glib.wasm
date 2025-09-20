#!/usr/bin/env -S deno run --allow-read --allow-write

/* GLib WASM - Deno Demo
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

import GLib from "./src/lib/index.ts"

async function runDemo() {
  console.log("📦 GLib.wasm - Comprehensive Demo")
  console.log("=" + "=".repeat(50))
  console.log("")

  try {
    // Initialize GLib WASM module
    console.log("1️⃣ Initializing GLib WASM module...")
    const glib = new GLib({
      simdOptimizations: true,
      maxMemoryMB: 128
    })

    const startTime = performance.now()
    await glib.initialize()
    const initTime = performance.now() - startTime
    console.log(`✅ GLib initialized in ${initTime.toFixed(2)}ms`)

    // Display build information
    console.log(`📋 Build Info:`)
    console.log(await glib.getBuildInfo())
    console.log("")

    // Test basic functionality
    console.log("2️⃣ Testing basic GLib functionality...")
    const basicTestStart = performance.now()
    const basicResult = await glib.testBasicFunctionality()
    const basicTestTime = performance.now() - basicTestStart

    if (basicResult.success) {
      console.log(`✅ Basic tests passed (${basicTestTime.toFixed(2)}ms)`)
      console.log(`   - String operations: ${basicResult.stringOps ? '✅' : '❌'}`)
      console.log(`   - Array operations: ${basicResult.arrayOps ? '✅' : '❌'}`)
      console.log(`   - Hash tables: ${basicResult.hashOps ? '✅' : '❌'}`)
    } else {
      console.log(`❌ Basic tests failed: ${basicResult.error}`)
    }
    console.log("")

    // Test filesystem operations
    console.log("3️⃣ Testing filesystem operations...")
    const fsTestStart = performance.now()
    const fsResult = await glib.testFilesystemOperations()
    const fsTestTime = performance.now() - fsTestStart

    if (fsResult.success) {
      console.log(`✅ Filesystem tests passed (${fsTestTime.toFixed(2)}ms)`)
      console.log(`   - File write/read: ${fsResult.fileOps ? '✅' : '❌'}`)
      console.log(`   - Directory operations: ${fsResult.dirOps ? '✅' : '❌'}`)
    } else {
      console.log(`❌ Filesystem tests failed: ${fsResult.error}`)
    }
    console.log("")

    // Test SIMD functionality
    console.log("3️⃣.5️⃣ Testing SIMD functionality...")
    const simdTestStart = performance.now()
    const simdResult = await glib.testSIMDFunctionality()
    const simdTestTime = performance.now() - simdTestStart

    if (simdResult.success) {
      console.log(`✅ SIMD tests passed (${simdTestTime.toFixed(2)}ms)`)
      console.log(`   - SIMD operations: ${simdResult.simdOps ? '✅' : '❌'}`)

      // Benchmark SIMD performance
      const simdThroughput = await glib.benchmarkSIMD()
      if (simdThroughput > 0) {
        console.log(`   - SIMD memcmp throughput: ${simdThroughput.toFixed(2)} MB/s`)
      }
    } else {
      console.log(`❌ SIMD tests failed: ${simdResult.error}`)
    }
    console.log("")

    // Test string utilities
    console.log("4️⃣ Testing GLib string utilities...")
    const stringTestStart = performance.now()
    const testStrings = [
      "Hello, GLib WASM!",
      "UTF-8: café, naïve, 中文",
      "Mixed: ASCII + émojis 🚀🔥✨"
    ]

    for (const testStr of testStrings) {
      const result = await glib.processString(testStr, {
        uppercase: false,
        trimWhitespace: true,
        validateUTF8: true
      })

      if (result.isValid) {
        console.log(`✅ "${testStr}" → length: ${result.length}, UTF-8: ${result.isValidUTF8 ? '✅' : '❌'}`)
      } else {
        console.log(`❌ String processing failed for: "${testStr}"`)
      }
    }
    const stringTestTime = performance.now() - stringTestStart
    console.log(`   String tests completed in ${stringTestTime.toFixed(2)}ms`)
    console.log("")

    // Test collections (arrays, hash tables, lists)
    console.log("5️⃣ Testing GLib collections...")
    const collectionsTestStart = performance.now()

    // Test GPtrArray
    const arrayResult = await glib.testPtrArray([
      "item1", "item2", "item3", "item4", "item5"
    ])
    console.log(`   GPtrArray: ${arrayResult.success ? '✅' : '❌'} (${arrayResult.count} items)`)

    // Test GHashTable
    const hashResult = await glib.testHashTable({
      "key1": "value1",
      "key2": "value2",
      "number": "42",
      "unicode": "café"
    })
    console.log(`   GHashTable: ${hashResult.success ? '✅' : '❌'} (${hashResult.count} pairs)`)

    // Test GSList
    const slistResult = await glib.testSList([10, 20, 30, 40, 50])
    console.log(`   GSList: ${slistResult.success ? '✅' : '❌'} (${slistResult.count} nodes)`)

    const collectionsTestTime = performance.now() - collectionsTestStart
    console.log(`   Collections tests completed in ${collectionsTestTime.toFixed(2)}ms`)
    console.log("")

    // Performance benchmarks
    console.log("6️⃣ Running performance benchmarks...")
    const benchmarkStart = performance.now()

    // String processing benchmark
    const largeText = "Lorem ipsum ".repeat(1000) // ~11KB text
    const stringBenchStart = performance.now()
    await glib.processString(largeText, {
      uppercase: true,
      trimWhitespace: true,
      validateUTF8: true
    })
    const stringBenchTime = performance.now() - stringBenchStart
    const stringThroughput = (largeText.length / stringBenchTime * 1000 / 1024 / 1024).toFixed(2)
    console.log(`   String processing: ${stringBenchTime.toFixed(2)}ms (${stringThroughput} MB/s)`)

    // Hash table benchmark
    const hashBenchStart = performance.now()
    const largeHash: Record<string, string> = {}
    for (let i = 0; i < 1000; i++) {
      largeHash[`key${i}`] = `value${i}`
    }
    await glib.testHashTable(largeHash)
    const hashBenchTime = performance.now() - hashBenchStart
    console.log(`   Hash operations: ${hashBenchTime.toFixed(2)}ms (${(1000 / hashBenchTime * 1000).toFixed(0)} ops/sec)`)

    const benchmarkTime = performance.now() - benchmarkStart
    console.log(`   Benchmarks completed in ${benchmarkTime.toFixed(2)}ms`)
    console.log("")

    // Display final statistics
    const totalTime = performance.now() - startTime
    console.log("📊 Demo Statistics:")
    console.log("=" + "=".repeat(30))
    console.log(`Total runtime: ${totalTime.toFixed(2)}ms`)
    console.log(`Memory usage: ${await glib.getMemoryUsage()} bytes`)
    console.log(`SIMD enabled: ${glib.isSIMDEnabled() ? '✅' : '❌'}`)
    console.log(`Threading: ${glib.isThreadingEnabled() ? '✅' : '❌'}`)
    console.log("")

    // Cleanup
    console.log("7️⃣ Cleaning up...")
    glib.cleanup()
    console.log("✅ Cleanup complete")
    console.log("")

    console.log("🎉 GLib WASM demo completed successfully!")
    console.log("=" + "=".repeat(50))

  } catch (error) {
    console.error("❌ Demo failed with error:")
    console.error(error)

    if (error instanceof Error) {
      console.error("Stack trace:")
      console.error(error.stack)
    }

    Deno.exit(1)
  }
}

// Self-executing demo
if (import.meta.main) {
  await runDemo()
}

export { runDemo }