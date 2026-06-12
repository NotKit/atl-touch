#!/usr/bin/env python3
"""Drive Skia's GN+Ninja build and copy the resulting shared library to Meson's output path."""
import sys
import os
import subprocess
import shutil


def main():
    _, gn, ninja, src_dir, out_dir, output_lib, *gn_args = sys.argv

    os.makedirs(out_dir, exist_ok=True)

    # fetch third-party deps (skcms, wuffs, ...) once; they live in separate repos
    deps_marker = os.path.join(out_dir, '.git-sync-deps-done')
    if not os.path.exists(deps_marker):
        subprocess.run(
            [sys.executable, os.path.join(src_dir, 'tools', 'git-sync-deps')],
            cwd=src_dir, check=True,
        )
        open(deps_marker, 'w').close()

    subprocess.run(
        [gn, 'gen', out_dir, f'--root={src_dir}', '--args=' + ' '.join(gn_args)],
        check=True,
    )

    subprocess.run([ninja, '-C', out_dir, 'skia'], check=True)

    shutil.copy2(os.path.join(out_dir, 'libskia.so'), output_lib)


if __name__ == '__main__':
    main()
