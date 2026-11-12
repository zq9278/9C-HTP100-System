"""Build main / NEW LCD / OLD LCD; both LCD packages include the pinned BOOT."""
import argparse, hashlib, json, os, shutil, subprocess
from datetime import datetime, timezone
from pathlib import Path
from ihex import read_hex, merge_screen, MAIN_FLASH

ROOT=Path(__file__).resolve().parents[1]

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def tool_env():
    env=os.environ.copy()
    if not shutil.which('arm-none-eabi-gcc'):
        roots=[Path(env.get('LOCALAPPDATA',''))/'stm32cube/bundles',Path(env.get('STM32CUBE_BUNDLE_PATH','/nonexistent'))]
        candidates=[p for base in roots for p in base.glob('gnu-tools-for-stm32/*/bin')]
        if candidates: env['PATH']=str(sorted(candidates)[-1])+os.pathsep+env['PATH']
    return env

def build(role, preset):
    is_main=role=='main';board='main' if is_main else 'screen'
    name='9C-HTP100-main-v2' if is_main else '9C-HTP100-screen'
    variant=None if is_main else role.split('-')[1].upper()
    actual=preset if is_main else ('Debug' if preset=='Tuning' else preset)+variant.title()
    cwd=ROOT/'firmware'/board;dest=ROOT/'out'/preset;dest.mkdir(parents=True,exist_ok=True)
    env=tool_env()
    subprocess.run(['cmake','--preset',actual],cwd=cwd,env=env,check=True)
    subprocess.run(['cmake','--build','build/'+actual,'--parallel','8'],cwd=cwd,env=env,check=True)
    built=cwd/'build'/actual
    source_hex=built/(name+'.hex')
    # Complete validation before replacing a previously usable package.
    image=read_hex(source_hex)
    if is_main:
        image.require_ranges([MAIN_FLASH]);image.vector(MAIN_FLASH[0],[(0x20000000,0x20009000)],MAIN_FLASH)
    package=dest/(role+'.hex');temporary=dest/(role+'.hex.tmp')
    boot=ROOT/'firmware/screen/boot/BOOT.hex'
    if is_main: shutil.copy2(source_hex,temporary)
    else: image=merge_screen(boot,source_hex,temporary)
    os.replace(temporary,package)
    prefix=role if is_main else role+'-app'
    files={package.name:{'sha256':digest(package),'bytes':package.stat().st_size}}
    for suffix in ('.elf','.bin','.hex'):
        source=built/(name+suffix);target=dest/(prefix+suffix)
        if target==package: continue
        shutil.copy2(source,target);files[target.name]={'sha256':digest(target),'bytes':target.stat().st_size}
    info={'protocol':1,'role':role,'lcd_panel':variant,'preset':preset,'cmake_preset':actual,
          'git_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
          'dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),
          'built_at':datetime.now(timezone.utc).isoformat(),'file':package.name,'sha256':digest(package),
          'boot_sha256':None if is_main else digest(boot),
          'regions':[{'start':f'0x{a:08X}','bytes':len(d)} for a,d in image.segments], 'files':files}
    descriptor=dest/(role+'.json');temp=descriptor.with_suffix('.json.tmp')
    temp.write_text(json.dumps(info,indent=2)+'\n',encoding='utf-8');os.replace(temp,descriptor)
    print('PACKAGE: '+str(package),flush=True)
    return info

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--preset',choices=['Debug','Release','Tuning'],default='Debug')
    parser.add_argument('--board',choices=['all','main','screen-new','screen-old','screen-nov11'],default='all')
    args=parser.parse_args()
    roles=('main','screen-new','screen-old','screen-nov11') if args.board=='all' else (args.board,)
    images={role:build(role,args.preset) for role in roles}
    if args.board=='all':
        dest=ROOT/'out'/args.preset/'manifest.json'
        dest.write_text(json.dumps({'protocol':1,'images':images},indent=2)+'\n',encoding='utf-8')
        print('Manifest:',dest)

if __name__=='__main__':main()
