"""Build paired images and emit a traceable SHA-256 manifest. Never flashes."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--preset',choices=['Debug','Release','Tuning'],default='Debug');args=parser.parse_args()
env=os.environ.copy()
if not shutil.which('arm-none-eabi-gcc'):
    roots=[Path(env.get('LOCALAPPDATA',''))/'stm32cube/bundles',Path(env.get('STM32CUBE_BUNDLE_PATH','/nonexistent'))]
    candidates=[p for base in roots for p in base.glob('gnu-tools-for-stm32/*/bin')]
    if candidates:env['PATH']=str(sorted(candidates)[-1])+os.pathsep+env['PATH']
manifest={'protocol':1,'git_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
          'dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),'images':{}}
dest=ROOT/'out'/args.preset;dest.mkdir(parents=True,exist_ok=True)
for board,name in [('main','9C-HTP100-main-v2'),('screen','9C-HTP100-screen')]:
    preset='Debug' if board=='screen' and args.preset=='Tuning' else args.preset
    cwd=ROOT/'firmware'/board
    subprocess.run(['cmake','--preset',preset],cwd=cwd,env=env,check=True)
    subprocess.run(['cmake','--build','build/'+preset,'--parallel','8'],cwd=cwd,env=env,check=True)
    files={}
    for suffix in ('.elf','.hex','.bin'):
        src=cwd/'build'/preset/(name+suffix)
        if not src.exists():continue
        target=dest/(board+suffix);shutil.copy2(src,target)
        files[target.name]={'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'bytes':target.stat().st_size}
    manifest['images'][board]={'preset':preset,'files':files}
(dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print('Manifest:',dest/'manifest.json')
