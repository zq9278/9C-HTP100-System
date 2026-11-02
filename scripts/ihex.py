"""Strict Intel HEX reader/merger. Sparse addresses stay sparse; no giant BIN gaps."""
from dataclasses import dataclass
from pathlib import Path

@dataclass
class Image:
    segments: list
    entry: int | None = None

    def require_ranges(self, ranges):
        if not self.segments:
            raise ValueError('Empty HEX image')
        for start, data in self.segments:
            if not any(low <= start and start + len(data) <= high for low, high in ranges):
                raise ValueError(f'HEX data outside target flash: 0x{start:08X}')

    def vector(self, address, ram_ranges, code_range):
        data = next((d[address-a:address-a+8] for a,d in self.segments if a <= address and address+8 <= a+len(d)), None)
        if data is None: raise ValueError(f'Missing vector table at 0x{address:08X}')
        sp = int.from_bytes(data[:4], 'little'); pc = int.from_bytes(data[4:], 'little')
        if sp % 8 or not any(lo < sp <= hi for lo,hi in ram_ranges):
            raise ValueError('Invalid initial stack pointer')
        if not pc & 1 or not code_range[0] <= (pc & ~1) < code_range[1]:
            raise ValueError('Invalid reset handler address')

def coalesce(records):
    result = []
    for address, data in sorted(records):
        if not data: continue
        if result and address < result[-1][0] + len(result[-1][1]):
            raise ValueError(f'Overlapping HEX data at 0x{address:08X}')
        if result and address == result[-1][0] + len(result[-1][1]): result[-1][1].extend(data)
        else: result.append((address, bytearray(data)))
    return [(address, bytes(data)) for address,data in result]

def read_hex(path):
    records = []; base = 0; eof = False; entry = None
    with Path(path).open('r', encoding='ascii') as f:
        for number, line in enumerate(f, 1):
            line = line.strip()
            if not line: continue
            if eof or not line.startswith(':'): raise ValueError(f'HEX framing error at line {number}')
            try: r = bytes.fromhex(line[1:])
            except ValueError: raise ValueError(f'HEX encoding error at line {number}') from None
            if len(r)<5 or len(r)!=r[0]+5 or sum(r)&255: raise ValueError(f'HEX checksum/length error at line {number}')
            size=r[0]; addr=int.from_bytes(r[1:3],'big'); kind=r[3]; data=r[4:-1]
            if kind==0:
                if addr+size>0x10000 or base+addr+size>0x100000000: raise ValueError('HEX address overflow')
                records.append((base+addr,data))
            elif kind==1 and size==0 and addr==0: eof=True
            elif kind in (2,4) and size==2 and addr==0: base=int.from_bytes(data,'big') << (4 if kind==2 else 16)
            elif kind==5 and size==4 and addr==0: entry=int.from_bytes(data,'big')
            elif kind==3 and size==4 and addr==0: entry=(int.from_bytes(data[:2],'big')<<4)+int.from_bytes(data[2:],'big')
            else: raise ValueError(f'Unsupported/malformed HEX record at line {number}')
    if not eof: raise ValueError('HEX EOF missing')
    return Image(coalesce(records), entry)

def record(kind, address, data):
    value=bytes([len(data)])+address.to_bytes(2,'big')+bytes([kind])+data
    return ':'+(value+bytes([(-sum(value))&255])).hex().upper()+'\n'

def write_hex(image, path):
    with Path(path).open('w', encoding='ascii', newline='\n') as f:
        bank=None
        for start,data in image.segments:
            offset=0
            while offset<len(data):
                address=start+offset; upper=address>>16; low=address&0xffff
                if upper!=bank: f.write(record(4,0,upper.to_bytes(2,'big')));bank=upper
                n=min(32,len(data)-offset,0x10000-low)
                f.write(record(0,low,data[offset:offset+n]));offset+=n
        if image.entry is not None: f.write(record(5,0,image.entry.to_bytes(4,'big')))
        f.write(record(1,0,b''))

MAIN_FLASH=(0x08000000,0x08020000)
BOOT_FLASH=(0x08000000,0x08020000)
SCREEN_FLASH=(0x90000000,0x91000000)
SCREEN_RAM=[(0x20000000,0x20020000),(0x24000000,0x24080000)]

def merge_screen(boot_path, app_path, destination):
    boot=read_hex(boot_path);app=read_hex(app_path)
    boot.require_ranges([BOOT_FLASH]);boot.vector(BOOT_FLASH[0],SCREEN_RAM,BOOT_FLASH)
    app.require_ranges([SCREEN_FLASH]);app.vector(SCREEN_FLASH[0],SCREEN_RAM,SCREEN_FLASH)
    image=Image(coalesce(boot.segments+app.segments),boot.entry)
    write_hex(image,destination)
    return image
