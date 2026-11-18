#!/usr/bin/env python3
"""Build the pinned capture bridge outside the source tree, for any frontend."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cargo', default='cargo')
    parser.add_argument('--manifest', required=True)
    parser.add_argument('--target-dir')
    parser.add_argument('--output')
    parser.add_argument('--target', default='')
    parser.add_argument('--crt-static', action='store_true')
    parser.add_argument('--offline', action='store_true')
    parser.add_argument('--sources', action='store_true')
    parser.add_argument('--test', action='store_true', help='Run bridge tests with synthetic backends')
    args = parser.parse_args()
    manifest = Path(args.manifest).resolve()
    if args.sources:
        vendor = manifest.parent.parent.parent / 'third_party' / 'flexaudio'
        for root in (manifest.parent, vendor):
            for path in sorted(root.rglob('*')):
                if path.is_file() and (path.suffix == '.rs' or path.name in ('Cargo.toml', 'Cargo.lock')):
                    print(path)
        return
    env = os.environ.copy()
    # Preserve rustup's proxy executable name when it is a link to rustup.
    cargo = os.path.abspath(args.cargo) if Path(args.cargo).is_file() else args.cargo
    rustc = Path(cargo).with_name('rustc.exe' if os.name == 'nt' else 'rustc')
    if rustc.is_file():
        env.setdefault('RUSTC', str(rustc))
    flags = env.get('RUSTFLAGS', '')
    if args.crt_static:
        flags += ' -C target-feature=+crt-static'
    # Static Rust archives may also be linked into the MediaCap shared library.
    windows = 'windows-msvc' in args.target or (not args.target and os.name == 'nt')
    if not windows:
        flags += ' -C relocation-model=pic'
    env['RUSTFLAGS'] = flags.strip()
    target_dir = Path(args.target_dir).resolve()
    command = [cargo, 'test' if args.test else 'build', '--manifest-path', str(manifest), '--release', '--locked', '--target-dir', str(target_dir)]
    if args.target:
        command += ['--target', args.target]
    if args.offline:
        command += ['--offline']
    # Cargo discovers .cargo/config.toml from its working directory, not the
    # manifest path. This also makes extracted source bundles work offline when
    # the native build directory is outside the source checkout.
    subprocess.run(command, env=env, cwd=manifest.parent, check=True)
    if args.test:
        return
    directory = target_dir / args.target / 'release' if args.target else target_dir / 'release'
    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    source = directory / ('mediacap_flexaudio.lib' if windows else 'libmediacap_flexaudio.a')
    if windows:
        # windows-rs import libraries are transitive native dependencies. Bundle
        # them into the delivered archive so downstream users need only OS libs.
        meta = [cargo, 'metadata', '--manifest-path', str(manifest), '--format-version', '1', '--locked']
        if args.target:
            meta += ['--filter-platform', args.target]
        else:
            host = subprocess.check_output([env.get('RUSTC', 'rustc'), '-vV'], env=env, text=True)
            meta += ['--filter-platform', next(line[6:] for line in host.splitlines() if line.startswith('host: '))]
        if args.offline:
            meta += ['--offline']
        metadata = json.loads(subprocess.check_output(meta, env=env, cwd=manifest.parent, text=True))
        imports = sorted({str(p) for package in metadata['packages']
                          if package['name'].startswith('windows_')
                          for p in Path(package['manifest_path']).parent.glob('lib/*.lib')})
        # Every windows-rs DLL import member supplies the same null descriptor.
        # Ignore only that librarian warning; linker diagnostics stay enabled.
        subprocess.run(['lib.exe', '/NOLOGO', '/IGNORE:4006', '/OUT:' + str(output), str(source), *imports], env=env, check=True)
    else:
        shutil.copy2(source, output)


if __name__ == '__main__':
    main()
