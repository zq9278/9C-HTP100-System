"""Create a self-contained Windows x64 ZIP. This never opens a debug probe."""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile
from package_info import inspect

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'tools/firmware_flasher'


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jlink', type=Path, default=Path('C:/Program Files/SEGGER/JLink_V954'))
    parser.add_argument('--skip-build', action='store_true', help='Package existing verified Release artifacts')
    args = parser.parse_args()
    electron = APP / 'node_modules/electron/dist'
    if not (electron / 'electron.exe').is_file():
        raise SystemExit('Run npm ci and npm start once in tools/firmware_flasher to prepare Electron.')
    for name in ('JLink.exe', 'JLinkARM.dll', 'USBDriver/InstDrivers.exe', 'Doc/LicenseIncGUI.txt'):
        if not (args.jlink / name).is_file():
            raise SystemExit('Missing J-Link runtime component: ' + name)
    if not args.skip_build:
        subprocess.run([sys.executable, str(ROOT / 'scripts/build.py'), '--preset', 'Release'], cwd=ROOT, check=True)
    images = {role: inspect(role, 'Release') for role in ('main', 'screen-new', 'screen-old')}
    # One distributable must contain one matched firmware release.
    if len({i['git_commit'] for i in images.values()}) != 1:
        raise SystemExit('Mixed firmware commits; rebuild all Release targets first.')
    package = json.loads((APP / 'package.json').read_text(encoding='utf-8'))
    stamp = datetime.now().strftime('%Y%m%d-%H%M%S')
    destination = ROOT / 'out/distribution' / ('HTP100-Flasher-' + package['version'] + '-' + stamp)
    if destination.exists():
        raise SystemExit('Output already exists; retry with a new timestamp.')
    shutil.copytree(electron, destination)
    (destination / 'electron.exe').rename(destination / 'HTP100-Flasher.exe')
    default_app = destination / 'resources/default_app.asar'
    if default_app.exists():
        default_app.unlink()
    application = destination / 'resources/app'
    application.mkdir()
    for name in ('main.js', 'backend.cjs', 'preload.js', 'renderer.js', 'index.html', 'style.css'):
        shutil.copy2(APP / name, application / name)
    (application / 'package.json').write_text(json.dumps({k: package[k] for k in ('name', 'version', 'main')}) + '\n', encoding='utf-8')
    payload = destination / 'resources/payload'
    firmware = payload / 'firmware'
    firmware.mkdir(parents=True)
    for info in images.values():
        shutil.copy2(info['path'], firmware / info['file'])
        info.pop('path', None)
        # ELF and app-only files are deliberately not distribution inputs.
        info.pop('files', None)
    runtime = payload / 'jlink'
    shutil.copytree(args.jlink, runtime, ignore=shutil.ignore_patterns('Uninstall.exe', '*.log'))
    inventory = {p.relative_to(payload).as_posix(): sha(p) for p in sorted(runtime.rglob('*')) if p.is_file()}
    bundle = {'version': package['version'], 'built_at': stamp, 'images': images, 'runtime': inventory}
    (payload / 'bundle.json').write_text(json.dumps(bundle, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    shutil.copy2(APP / 'DISTRIBUTION.md', destination / '使用说明.txt')
    shutil.copy2(args.jlink / 'Doc/LicenseIncGUI.txt', destination / 'SEGGER-LICENSE.txt')
    # Read every bundled firmware back before distributing the archive.
    for info in images.values():
        if sha(firmware / info['file']) != info['sha256']:
            raise SystemExit('Copied firmware hash mismatch: ' + info['file'])
    archive = destination.parent / (destination.name + '.zip')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as output:
        for file in sorted(destination.rglob('*')):
            if file.is_file():
                output.write(file, file.relative_to(destination.parent))
    archive.with_suffix('.zip.sha256').write_text(sha(archive) + '  ' + archive.name + '\n', encoding='ascii')
    print('APPLICATION:', destination / 'HTP100-Flasher.exe', flush=True)
    print('ARCHIVE:', archive, flush=True)
    print('SIZE_MIB:', round(archive.stat().st_size / 1024**2, 1), flush=True)
    print('SEGGER redistribution authorization is required before external distribution; original license included.', flush=True)


if __name__ == '__main__':
    main()
