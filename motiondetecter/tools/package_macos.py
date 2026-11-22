#!/usr/bin/env python3
"""Package the Widgets-only app with just Cocoa/offscreen platform plugins.

This avoids deploying unrelated Qt PDF, QML and virtual keyboard plugins.
The destination is replaced only with --replace, after a verified staging build.
"""
import argparse
import shutil
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--build', type=Path, default=root / 'motiondetecter/build-ui-qt611')
parser.add_argument('--out', type=Path, default=root / 'output/PoseLab-UI.app')
parser.add_argument('--plugins', type=Path, default=Path('/opt/homebrew/share/qt/plugins'))
parser.add_argument('--replace', action='store_true')
args = parser.parse_args()
source = args.build / 'motiondetecter.app'
if not source.is_dir():
    raise SystemExit(f'Build the app first: {source}')
if args.out.exists() and not args.replace:
    raise SystemExit('Destination exists; use --replace to update this generated bundle.')
args.out.parent.mkdir(parents=True, exist_ok=True)
staging = args.out.with_name('.' + args.out.stem + '.staging.app')
if staging.exists():
    raise SystemExit(f'Remove the previous generated staging bundle first: {staging}')
shutil.copytree(source, staging, symlinks=True)
plugins = staging / 'Contents/PlugIns/platforms'
plugins.mkdir(parents=True, exist_ok=True)
for name in ['libqcocoa.dylib', 'libqoffscreen.dylib']:
    shutil.copy2(args.plugins / 'platforms' / name, plugins / name)
cmd = [shutil.which('macdeployqt') or '/opt/homebrew/bin/macdeployqt', str(staging),
       '-no-plugins', '-no-codesign', '-libpath=/opt/homebrew/lib',
       '-libpath=/opt/homebrew/opt/qt/lib']
cmd += [f'-executable={plugins/name}' for name in ['libqcocoa.dylib', 'libqoffscreen.dylib']]
result = subprocess.run(cmd, capture_output=True, text=True)
print(result.stdout, end='')
print(result.stderr, end='')
if result.returncode or 'ERROR:' in result.stdout + result.stderr:
    raise SystemExit('Qt deployment failed; staging retained for inspection.')
resources = staging / 'Contents/Resources'
resources.mkdir(exist_ok=True)
(resources / 'qt.conf').write_text('[Paths]\nPlugins = PlugIns\n')
# No package binary may retain a developer's Homebrew dylib/framework dependency.
for path in staging.rglob('*'):
    if path.is_file() and not path.is_symlink():
        kind = subprocess.run(['file', '-b', str(path)], capture_output=True, text=True).stdout
        if 'Mach-O' not in kind:
            continue
        identity=subprocess.run(['otool','-D',str(path)],capture_output=True,text=True).stdout.splitlines()
        if len(identity)>1 and identity[1].strip():
            frameworks=staging/'Contents/Frameworks'
            rel=path.relative_to(frameworks) if path.is_relative_to(frameworks) else Path(path.name)
            subprocess.run(['install_name_tool','-id','@rpath/'+str(rel),str(path)],check=True)
        output = subprocess.run(['otool', '-L', str(path)], check=True,
                                capture_output=True, text=True).stdout
        if '/opt/homebrew/' in output or '/Users/' in '\n'.join(output.splitlines()[1:]):
            raise SystemExit(f'External dylib dependency remains: {path}\n{output}')
subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(staging)], check=True)
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(staging)], check=True)
if args.out.exists():
    shutil.rmtree(args.out)
staging.rename(args.out)
print('Packaged and ad-hoc signature verified:', args.out)
