import importlib.util, pathlib, struct, unittest
path=pathlib.Path(__file__).parent/'rtt_bridge.py'
spec=importlib.util.spec_from_file_location('rtt_bridge',path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class FakeLink:
 def __init__(self):self.parser=m.Frames();self.reply=bytearray()
 def rtt_write(self,index,data):
  n=min(3,len(data))
  for kind,seq,p in self.parser.feed(bytes(data[:n])):
   reply=bytearray(28);reply[0]=kind;reply[4:8]=p[:4];struct.pack_into('<4f',reply,8,20000,60000,200,.9)
   self.reply.extend(m.encode(0x15,seq,reply))
  return n
 def rtt_read(self,index,size):
  b=self.reply[:2];del self.reply[:2];return list(b)
class ProtocolTests(unittest.TestCase):
 def test_split_and_corrupt(self):
  packet=m.encode(0x12,5,b'abc')
  for split in range(len(packet)+1):
   frames=m.Frames();self.assertEqual(frames.feed(packet[:split])+frames.feed(packet[split:]),[(0x12,5,b'abc')])
  bad=bytearray(packet);bad[-1]^=1;frames=m.Frames();self.assertEqual(frames.feed(bad+packet),[(0x12,5,b'abc')])
 def test_partial_writes_and_reads(self):
  bridge=m.Bridge();bridge.link=FakeLink();values=bridge.request('read',[1]);self.assertAlmostEqual(values[2],200)
 def test_bad_index_rejected_before_io(self):
  bridge=m.Bridge();bridge.link=FakeLink()
  with self.assertRaises(ValueError):bridge.request('read',[5])
if __name__=='__main__':unittest.main()
