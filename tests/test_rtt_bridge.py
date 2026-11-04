import sys, struct, unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools/pressure_tuner"))
from rtt_bridge import RttMemory
class Memory:
    def __init__(self):self.data=bytearray(0x9000)
    def memory_read8(self,a,n):return list(self.data[a-0x20000000:a-0x20000000+n])
    def memory_write8(self,a,v):self.data[a-0x20000000:a-0x20000000+len(v)]=bytes(v)
    def memory_read32(self,a,n):return list(struct.unpack("<"+"I"*n,bytes(self.memory_read8(a,n*4))))
    def memory_write32(self,a,v):self.memory_write8(a,struct.pack("<"+"I"*len(v),*v))
class TestRings(unittest.TestCase):
    def setUp(self):
        self.m=Memory();self.cb=0x20000100
        self.m.memory_write8(self.cb,b"SEGGER RTT"+bytes(6))
        self.m.memory_write32(self.cb+16,[2,1])
        for i in range(3):self.m.memory_write32(self.cb+24+i*24,[0,0x20001000+i*32,8,0,0,0])
        self.r=RttMemory(self.m)
    def test_wrap_read(self):
        self.m.memory_write8(0x20001000,b"abcdefgh")
        self.m.memory_write32(self.cb+36,[3,6])
        self.assertEqual(bytes(self.r.read(0,99)),b"ghabc")
        self.assertEqual(self.m.memory_read32(self.cb+36,2),[3,3])
    def test_write_leaves_one_slot_and_wraps(self):
        desc=self.cb+24+48
        self.m.memory_write32(desc+12,[6,3])
        self.assertEqual(self.r.write(0,list(b"ABCDE")),4)
        self.assertEqual(bytes(self.m.memory_read8(0x20001040,8)),b"CD"+bytes(4)+b"AB")
        self.assertEqual(self.m.memory_read32(desc+12,2),[2,3])
        self.assertEqual(self.r.write(0,[1]),0)
    def test_corruption_rejected_before_access(self):
        self.m.memory_write32(self.cb+40,[0xe00177a6])
        with self.assertRaisesRegex(RuntimeError,"pointer invalid"):self.r.read(0,256)
if __name__=="__main__":unittest.main()
