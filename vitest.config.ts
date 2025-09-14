/**
 * @file Vitest Configuration for WASM-Native GLib
 * @description Test configuration for comprehensive WASM-native GLib testing
 */

import { defineConfig } from 'vitest/config';
import path from 'path';

export default defineConfig({
  test: {
    // Test environment setup
    environment: 'happy-dom', // Browser-like environment for Web API testing
    globals: true, // Enable global test APIs (describe, it, expect)

    // Coverage configuration (Vitest v3 updated)
    coverage: {
      provider: 'v8',
      reporter: ['text', 'json', 'html'],
      // Include covered and uncovered files matching this pattern
      include: ['src/**/*.ts'],
      // Exclusion applied to files that match include pattern above
      exclude: [
        'tests/',
        '**/*.d.ts',
        'vitest.config.ts',
        'eslint.config.js'
      ],
      // Aim for high coverage of critical WASM-native features
      thresholds: {
        global: {
          branches: 80,
          functions: 85,
          lines: 85,
          statements: 85
        }
      }
    },

    // Test file patterns
    include: ['tests/**/*.{test,spec}.{ts,js}'],
    exclude: [
      'node_modules/',
      'dist/',
      'build/',
      '**/*.d.ts'
    ],

    // Test timeout configuration
    testTimeout: 10000, // 10 seconds for complex WASM operations
    hookTimeout: 10000, // 10 seconds for setup/teardown

    // Sequence configuration for Jest-like behavior
    sequence: {
      hooks: 'list', // Run hooks sequentially like Jest
    },

    // Setup files
    setupFiles: ['./tests/setup.ts'],

    // Dependencies configuration
    server: {
      deps: {
        inline: [
          // Inline dependencies that need to be transformed
          '@discere-os/libffi.wasm',
          '@discere-os/zlib.wasm'
        ]
      }
    },

    // Threading simulation for Web Workers
    threads: false, // Disable Vitest threads to avoid conflicts with Worker mocking
    isolate: true, // Isolate tests for clean state

    // Reporter configuration
    reporter: ['verbose', 'json', 'html'],

    // Output configuration
    outputFile: {
      json: './test-results.json',
      html: './coverage/index.html'
    }
  },

  // Resolve configuration for TypeScript modules
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
      '@tests': path.resolve(__dirname, './tests')
    }
  },

  // Define global constants for testing
  define: {
    __WASM_NATIVE_BUILD__: true,
    __TEST_ENVIRONMENT__: true,
    __BROWSER_BUILD__: true
  },

  // ESBuild configuration for TypeScript
  esbuild: {
    target: 'es2022',
    format: 'esm'
  }
});