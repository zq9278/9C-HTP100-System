"""JSON-lines IPC to J-Link RTT. Never halt, reset or patch target variables."""
import codecs, json, math, os, queue, secrets, struct, sys, threading, time
from pathlib import Path
def crc(data):
    c=0xffff
    for x in data:
        c^=x
        for _ in range(8): c=(c>>1)^(0xa001 if c&1 else 0)
    return c
def encode(kind,seq,payload):
    data=b'\xb5\x5b'+struct.pack('<BBHB',1,kind,seq,len(payload))+payload
    return data+struct.pack('<H',crc(data))
class Frames:
    def __init__(self): self.buffer=bytearray()
    def feed(self,data):
        self.buffer.extend(data);frames=[]
        while len(self.buffer)>=7:
            b=self.buffer
            if b[:2]!=b'\xb5\x5b' or b[2]!=1 or b[6]>48: del b[0];continue
            size=9+b[6]
            if len(b)<size: break
            if struct.unpack_from('<H',b,size-2)[0]!=crc(b[:size-2]):del b[0];continue
            frames.append((b[3],struct.unpack_from('<H',b,4)[0],bytes(b[7:size-2])));del b[:size]
        return frames
class RttMemory:
    """Single-owner RTT rings over background SWD, without DLL RTT worker."""
    START, END = 0x20000000, 0x20009000
    def __init__(self, link):
        self.link=link;self.base=None;self.up_count=0
        data=bytes(link.memory_read8(self.START,self.END-self.START))
        for offset in range(0,len(data)-24,4):
            if data[offset:offset+16]!=b'SEGGER RTT'+b'\0'*6:continue
            up,down=struct.unpack_from('<II',data,offset+16)
            if not (2<=up<=16 and 1<=down<=16):continue
            if offset+24+24*(up+down)>len(data):continue
            self.base=self.START+offset;self.up_count=up
            try:
                for index,direction in [(0,True),(1,True),(0,False)]:self.descriptor(index,direction)
                return
            except RuntimeError:self.base=None
        raise RuntimeError('Valid RTT control block not found. Use Tuning firmware; restart an idle board if its RTT pointers are damaged.')
    def descriptor(self,index,up):
        address=self.base+24+24*(index if up else self.up_count+index)
        name,buffer,size,wr,rd,flags=self.link.memory_read32(address,6)
        if not (self.START<=buffer<self.END and 2<=size<=self.END-buffer and wr<size and rd<size):
            raise RuntimeError('RTT ring pointer invalid; connection stopped. Restart the idle board or use updated Tuning firmware.')
        return address,buffer,size,wr,rd
    def read(self,index,limit):
        address,buffer,size,wr,rd=self.descriptor(index,True)
        count=min(limit,(wr-rd)%size)
        if not count:return []
        first=min(count,size-rd)
        data=self.link.memory_read8(buffer+rd,first)
        if first<count:data+=self.link.memory_read8(buffer,count-first)
        self.link.memory_write32(address+16,[(rd+count)%size])
        return data
    def write(self,index,data):
        address,buffer,size,wr,rd=self.descriptor(index,False)
        count=min(len(data),(rd-wr-1)%size)
        if not count:return 0
        first=min(count,size-wr)
        self.link.memory_write8(buffer+wr,data[:first])
        if first<count:self.link.memory_write8(buffer,data[first:count])
        self.link.memory_write32(address+12,[(wr+count)%size])
        return count

class Bridge:
    def __init__(self): self.link=None;self.seq=0;self.token=secrets.randbits(32) or 1;self.frames=Frames();self.decoder=codecs.getincrementaldecoder("utf-8")("replace")
    def transact(self,kind,extra=b''):
        self.seq=(self.seq+1)&0xffff;packet=encode(kind,self.seq,struct.pack('<I',self.token)+extra)
        for attempt in range(3):
            offset=0;deadline=time.monotonic()+.7
            while time.monotonic()<deadline:
                if offset<len(packet): offset+=self.transport.write(0,list(packet[offset:]))
                for response,seq,p in self.frames.feed(bytes(self.transport.read(0,256))):
                    if response!=0x15 or seq!=self.seq or len(p)!=28 or p[0]!=kind or struct.unpack_from('<I',p,4)[0]!=self.token:continue
                    if p[1]:raise RuntimeError('Firmware rejected request, status='+str(p[1]))
                    return p
                time.sleep(.01)
        raise TimeoutError('RTT timeout. Check Tuning firmware and close other J-Link sessions.')
    def logs(self):
        if self.link:
            data=bytes(self.transport.read(1,2048))
            if data: print(json.dumps({'event':'log','text':self.decoder.decode(data)}),flush=True)
    def connect(self, options=None):
        options=options or {}
        serial=str(options.get('serial',''))
        speed=int(options.get('speed',1000))
        if (serial and (not serial.isdigit() or len(serial)>12)) or speed not in (1000,4000,8000):
            raise ValueError('Invalid probe settings')
        if self.link: raise RuntimeError('Already connected')
        import pylink
        dll=os.environ.get('JLINK_DLL')
        if not dll and os.name=='nt':
            base=Path(os.environ.get('LOCALAPPDATA',''))/'stm32cube/bundles/jlink-gdbserver'
            candidates=sorted(base.glob('*/bin/JLink_x64.dll'),reverse=True)
            if candidates:dll=str(candidates[0])
        self.link=pylink.JLink(**({'lib':pylink.library.Library(dllpath=dll)} if dll else {}))
        try:
            self.link.open(serial_no=int(serial) if serial else None);self.link.set_tif(pylink.enums.JLinkInterfaces.SWD)
            # RTT only: generic core attach avoids the STM32 device-specific
            # work-RAM initialization/reset seen with J-Link V9.54. Flashing
            # still uses the exact STM32 part in the separate programmer.
            self.link.exec_command('InhibitConnectRetries = 1')
            self.link.connect('Cortex-M0',speed=1000)  # RTT stability is independent of flash speed.
            if self.link.halted():raise RuntimeError('Target halted. Resume it using the debugger first.')
            self.frames=Frames()
            self.decoder=codecs.getincrementaldecoder("utf-8")("replace")
            self.transport=RttMemory(self.link)
            response=self.transact(0x10)
            return {'firmware':struct.unpack_from('<I',response,24)[0]}
        except Exception:self.release();raise
    def release(self):
        if self.link:
            try:self.link.close()
            finally:self.link=None
    def close(self):
        if self.link:
            try:self.transact(0x14)
            finally:self.release()
    def request(self,method,args):
        if method=='connect':return self.connect(args[0] if args else {})
        if method=='disconnect':self.close();return True
        if not self.link:raise RuntimeError('Not connected')
        if method=='keepalive':self.transact(0x11);return True
        index=args[0]
        if not isinstance(index,int) or not 0<=index<5:raise ValueError('Invalid profile')
        if method=='read':return list(struct.unpack_from('<4f',self.transact(0x12,bytes([index])),8))
        if method=='write':
            values=args[1]
            if len(values)!=4 or any(not math.isfinite(v) for v in values) or not (0<values[0]<=65535 and 0<values[1]<=65535 and 0<values[2]<=1000 and .01<=values[3]<=1):raise ValueError('Invalid parameters')
            return list(struct.unpack_from('<4f',self.transact(0x13,bytes([index])+struct.pack('<4f',*values)),8))
        raise ValueError('Unknown method')
def main():
    commands=queue.Queue();bridge=Bridge()
    def read_commands():
        for line in sys.stdin:
            try:commands.put(json.loads(line))
            except ValueError:pass
        commands.put(None)
    threading.Thread(target=read_commands,daemon=True).start();last=time.monotonic()
    try:
        while True:
            try:cmd=commands.get(timeout=.1)
            except queue.Empty:
                if bridge.link:
                    try:bridge.logs()
                    except Exception:pass
                if bridge.link and time.monotonic()-last>=.5:
                    try:bridge.request('keepalive',[]);last=time.monotonic()
                    except Exception as e:
                        bridge.release()
                        print(json.dumps({'event':'disconnected','error':str(e)}),flush=True)
                continue
            if cmd is None:break
            try:
                value=bridge.request(cmd['method'],cmd.get('args',[]));last=time.monotonic();reply={'id':cmd['id'],'result':value}
            except Exception as e:reply={'id':cmd['id'],'error':str(e)}
            print(json.dumps(reply,ensure_ascii=True),flush=True)
    finally:
        try:bridge.close()
        except Exception:pass
if __name__=='__main__':main()
