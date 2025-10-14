/**
 * Glib WASM Benchmarks
 */

import GlibWASM from "../src/lib/index.ts"

Deno.bench("glib initialization", {
  baseline: true
}, async () => {
  const lib = new GlibWASM()
  await lib.initialize()
})
