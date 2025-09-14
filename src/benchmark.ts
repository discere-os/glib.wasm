/* GLib WASM - Benchmark script
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

import { detectBrowserCapabilities, getMonotonicTimeNS } from './lib/index.js';

interface BenchmarkResult {
  name: string;
  iterations: number;
  totalTime: number; // in nanoseconds
  avgTime: number; // in nanoseconds
  opsPerSecond: number;
}

async function benchmarkFeatureDetection(): Promise<BenchmarkResult> {
  const iterations = 1000;
  const startTime = await getMonotonicTimeNS();

  for (let i = 0; i < iterations; i++) {
    await detectBrowserCapabilities();
  }

  const endTime = await getMonotonicTimeNS();
  const totalTime = endTime - startTime;
  const avgTime = totalTime / iterations;
  const opsPerSecond = (iterations * 1_000_000_000) / totalTime;

  return {
    name: 'Feature Detection',
    iterations,
    totalTime,
    avgTime,
    opsPerSecond,
  };
}

async function benchmarkHighPrecisionTiming(): Promise<BenchmarkResult> {
  const iterations = 10000;
  const startTime = await getMonotonicTimeNS();

  for (let i = 0; i < iterations; i++) {
    await getMonotonicTimeNS();
  }

  const endTime = await getMonotonicTimeNS();
  const totalTime = endTime - startTime;
  const avgTime = totalTime / iterations;
  const opsPerSecond = (iterations * 1_000_000_000) / totalTime;

  return {
    name: 'High-Precision Timing',
    iterations,
    totalTime,
    avgTime,
    opsPerSecond,
  };
}

function formatNanoseconds(ns: number): string {
  if (ns < 1000) return `${ns.toFixed(2)}ns`;
  if (ns < 1_000_000) return `${(ns / 1000).toFixed(2)}μs`;
  if (ns < 1_000_000_000) return `${(ns / 1_000_000).toFixed(2)}ms`;
  return `${(ns / 1_000_000_000).toFixed(2)}s`;
}

function formatOpsPerSecond(ops: number): string {
  if (ops < 1000) return `${ops.toFixed(0)} ops/sec`;
  if (ops < 1_000_000) return `${(ops / 1000).toFixed(1)}K ops/sec`;
  if (ops < 1_000_000_000) return `${(ops / 1_000_000).toFixed(1)}M ops/sec`;
  return `${(ops / 1_000_000_000).toFixed(1)}B ops/sec`;
}

async function runBenchmarks() {
  console.log('⚡ GLib WASM Performance Benchmarks\n');

  try {
    // 1. Browser Capabilities Detection
    console.log('🔍 Feature Detection Benchmark...');
    const capabilities = await detectBrowserCapabilities();
    console.log(`Using WebGPU timing: ${capabilities.webgpuTimestampQueries ? '✅' : '❌'}\n`);

    // Run benchmarks
    const benchmarks: BenchmarkResult[] = [];

    benchmarks.push(await benchmarkFeatureDetection());
    benchmarks.push(await benchmarkHighPrecisionTiming());

    // Display results
    console.log('📊 Benchmark Results:\n');
    console.table(
      benchmarks.map(result => ({
        Benchmark: result.name,
        Iterations: result.iterations.toLocaleString(),
        'Avg Time': formatNanoseconds(result.avgTime),
        'Total Time': formatNanoseconds(result.totalTime),
        'Ops/Second': formatOpsPerSecond(result.opsPerSecond),
      }))
    );

    // Performance Analysis
    console.log('\n🎯 Performance Analysis:');

    const featureDetection = benchmarks.find(b => b.name === 'Feature Detection');
    const timing = benchmarks.find(b => b.name === 'High-Precision Timing');

    if (featureDetection) {
      console.log(`Feature detection: ${featureDetection.opsPerSecond > 1000 ? '✅' : '⚠️'} ${formatOpsPerSecond(featureDetection.opsPerSecond)}`);
    }

    if (timing) {
      console.log(`High-precision timing: ${timing.opsPerSecond > 100000 ? '✅' : '⚠️'} ${formatOpsPerSecond(timing.opsPerSecond)}`);
    }

    console.log('\n🎉 Benchmarks completed successfully!');

  } catch (error) {
    console.error('❌ Benchmark failed:', error);
    process.exit(1);
  }
}

// Run benchmarks if this is the main module
if (import.meta.url === `file://${process.argv[1]}`) {
  runBenchmarks().catch(console.error);
}

export { runBenchmarks };