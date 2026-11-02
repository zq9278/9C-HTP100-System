import sys, tempfile, unittest, hashlib, json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from ihex import Image, read_hex, write_hex, merge_screen, record
from package_info import inspect

def vector(base): return (0x20010000).to_bytes(4,'little')+(base+9).to_bytes(4,'little')+b'CODE'

class HexTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.boot=self.root/'boot.hex';self.app=self.root/'app.hex';self.result=self.root/'merged.hex'
        write_hex(Image([(0x08000000,vector(0x08000000))],0x08000009),self.boot)
        write_hex(Image([(0x90000000,vector(0x90000000))],0x90000009),self.app)
    def tearDown(self):self.tmp.cleanup()
    def test_merge_keeps_boot_and_app_at_separate_addresses(self):
        merge_screen(self.boot,self.app,self.result);merged=read_hex(self.result)
        self.assertEqual(merged.segments,read_hex(self.boot).segments+read_hex(self.app).segments)
        self.assertEqual(merged.entry,0x08000009)
        self.assertLess(self.result.stat().st_size,512)
    def test_checksum_and_eof_rejected(self):
        self.app.write_text(':010000000100\n:00000001FF\n')
        with self.assertRaisesRegex(ValueError,'checksum'):read_hex(self.app)
        self.app.write_text(record(0,0,b'a'))
        with self.assertRaisesRegex(ValueError,'EOF'):read_hex(self.app)
    def test_overlap_and_trailing_records_rejected(self):
        self.app.write_text(record(0,0,b'abc')+record(0,2,b'x')+record(1,0,b''))
        with self.assertRaisesRegex(ValueError,'Overlapping'):read_hex(self.app)
        self.app.write_text(record(1,0,b'')+record(0,0,b'x'))
        with self.assertRaisesRegex(ValueError,'framing'):read_hex(self.app)
    def test_wrong_board_address_is_rejected(self):
        write_hex(Image([(0x08000000,vector(0x08000000))]),self.app)
        with self.assertRaisesRegex(ValueError,'outside'):merge_screen(self.boot,self.app,self.result)
    def test_invalid_reset_vector_rejected(self):
        write_hex(Image([(0x90000000,b'\xff'*16)]),self.app)
        with self.assertRaisesRegex(ValueError,'stack'):merge_screen(self.boot,self.app,self.result)
    def test_64k_boundary_roundtrip(self):
        expected=Image([(0x9000fff0,bytes(range(128)))],0x90000001)
        write_hex(expected,self.app);self.assertEqual(read_hex(self.app),expected)
    def test_extended_segment_address(self):
        self.app.write_text(record(2,0,b'\x10\x00')+record(0,4,b'abc')+record(1,0,b''))
        self.assertEqual(read_hex(self.app).segments,[(0x10004,b'abc')])
    def test_package_variant_hash_and_boot_validation(self):
        folder=self.root/'out/Release';folder.mkdir(parents=True)
        boot=self.root/'firmware/screen/boot/BOOT.hex';boot.parent.mkdir(parents=True);boot.write_bytes(self.boot.read_bytes())
        image=folder/'screen-old.hex';merge_screen(boot,self.app,image)
        info={'role':'screen-old','preset':'Release','file':image.name,'lcd_panel':'OLD',
              'sha256':hashlib.sha256(image.read_bytes()).hexdigest(),'boot_sha256':hashlib.sha256(boot.read_bytes()).hexdigest()}
        descriptor=folder/'screen-old.json';descriptor.write_text(json.dumps(info))
        self.assertEqual(inspect('screen-old','Release',self.root)['lcd_panel'],'OLD')
        info['lcd_panel']='NEW';descriptor.write_text(json.dumps(info))
        with self.assertRaisesRegex(ValueError,'variant'):inspect('screen-old','Release',self.root)
        info['lcd_panel']='OLD';descriptor.write_text(json.dumps(info));image.write_bytes(image.read_bytes()+b'\n')
        with self.assertRaisesRegex(ValueError,'SHA'):inspect('screen-old','Release',self.root)

if __name__=='__main__':unittest.main()
