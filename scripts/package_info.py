"""Validate the selected build package before any probe is opened."""
import argparse, hashlib, json
from pathlib import Path
from ihex import read_hex, MAIN_FLASH, BOOT_FLASH, SCREEN_FLASH, SCREEN_RAM
ROOT=Path(__file__).resolve().parents[1]
ROLES=('main','screen-new','screen-old','screen-nov11')
PRESETS=('Debug','Release','Tuning')

def inspect(role,preset,root=ROOT):
    if role not in ROLES or preset not in PRESETS: raise ValueError('Invalid build target')
    folder=root/'out'/preset
    info=json.loads((folder/(role+'.json')).read_text(encoding='utf-8'))
    if info['role']!=role or info['preset']!=preset or info['file']!=role+'.hex': raise ValueError('Package target mismatch')
    path=folder/info['file']
    if hashlib.sha256(path.read_bytes()).hexdigest()!=info['sha256']: raise ValueError('Package SHA-256 mismatch; rebuild required')
    image=read_hex(path)
    if role=='main':
        if info['lcd_panel'] is not None: raise ValueError('Invalid main package')
        image.require_ranges([MAIN_FLASH]);image.vector(MAIN_FLASH[0],[(0x20000000,0x20009000)],MAIN_FLASH)
    else:
        if info['lcd_panel']!=role.split('-')[1].upper(): raise ValueError('LCD variant mismatch')
        image.require_ranges([BOOT_FLASH,SCREEN_FLASH])
        image.vector(BOOT_FLASH[0],SCREEN_RAM,BOOT_FLASH);image.vector(SCREEN_FLASH[0],SCREEN_RAM,SCREEN_FLASH)
        boot=root/'firmware/screen/boot/BOOT.hex'
        if info['boot_sha256']!=hashlib.sha256(boot.read_bytes()).hexdigest(): raise ValueError('BOOT version mismatch')
        expected=read_hex(boot)
        actual=[(a,d) for a,d in image.segments if BOOT_FLASH[0]<=a<BOOT_FLASH[1]]
        if actual!=expected.segments or image.entry!=expected.entry: raise ValueError('Merged BOOT content mismatch')
    return dict(info,path=str(path),bytes=path.stat().st_size)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--role',choices=ROLES,required=True);p.add_argument('--preset',choices=PRESETS,required=True)
    args=p.parse_args()
    print(json.dumps(inspect(args.role,args.preset),ensure_ascii=True))
