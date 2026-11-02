"""Build the SDL simulator and check real UI code without connecting hardware."""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
build = ROOT / '.cache/screen-sim'
env = os.environ.copy()
gcc = shutil.which('gcc')
if os.name == 'nt' and not gcc:
    candidates = sorted(Path('C:/Program Files/JetBrains').glob('CLion */bin/mingw/bin/gcc.exe'))
    if candidates:
        gcc = str(candidates[-1])
if not gcc:
    raise SystemExit('Install GCC / MinGW and put gcc on PATH before running the SDL test.')
env['PATH'] = str(Path(gcc).parent) + os.pathsep + env['PATH']
subprocess.run(['cmake', '-S', str(ROOT / 'firmware/screen/lvgl'), '-B', str(build),
                '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Debug', '-DCMAKE_C_COMPILER=' + gcc], env=env, check=True)
subprocess.run(['cmake', '--build', str(build), '--parallel', '8'], env=env, check=True)
executable = build / ('slk_sdl_sim.exe' if os.name == 'nt' else 'slk_sdl_sim')
for key in list(env):
    if key.startswith('SLK_'):
        del env[key]
env.update(SLK_HEADLESS='1', SLK_SELF_TEST='1', SLK_LANG='en', SLK_CAPTURE_MS='100')
for popup in (False, True):
    env['SLK_CAPTURE_BMP'] = str(ROOT / '.cache' / ('shield-en.bmp' if popup else 'preset-en.bmp'))
    if popup:
        env['SLK_INVALID_SHIELD'] = '1'
    subprocess.run([str(executable)], cwd=build, env=env, check=True, timeout=20)
print('Screen UI checks passed; captures are in .cache/')
