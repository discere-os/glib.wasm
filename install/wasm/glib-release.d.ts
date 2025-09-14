/* GLib WASM - TypeScript declarations for generated module
 * Copyright (C) 2025 Superstruct Ltd, New Zealand
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

import type { GLibWASMModule } from '../../src/lib/types';

declare function createModule(config?: any): Promise<GLibWASMModule>;
export default createModule;