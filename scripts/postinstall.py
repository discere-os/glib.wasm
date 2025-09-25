#!/usr/bin/env python3
import os, sys, shutil

def main():
    prefix = os.environ.get('MESON_INSTALL_DESTDIR_PREFIX') or (sys.argv[1] if len(sys.argv) > 1 else None)
    if not prefix:
        prefix = os.getcwd()
    wasm_dir = os.path.join(prefix, 'wasm')
    if not os.path.isdir(wasm_dir):
        return 0
    # Normalize any wasm artifact; glib produces multiple libs, we pick libglib-2.0 by preference
    candidates = [
        'libglib-2.0.wasm', 'glib-2.0.wasm', 'libglib-2.0.so',
    ]
    target = os.path.join(wasm_dir, 'glib-side.wasm')
    if os.path.exists(target):
        return 0
    for c in candidates:
        p = os.path.join(wasm_dir, c)
        if os.path.exists(p):
            shutil.copy2(p, target)
            break
    else:
        for f in os.listdir(wasm_dir):
            if f.endswith('.wasm'):
                shutil.copy2(os.path.join(wasm_dir,f), target)
                break
    return 0

if __name__ == '__main__':
    raise SystemExit(main())

