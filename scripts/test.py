"""Run native tests against production C sources; no target hardware required."""
from pathlib import Path
import subprocess, shutil, os
ROOT=Path(__file__).resolve().parents[1]
os.chdir(ROOT)
native=ROOT/'.cache/native/ziglang/zig.exe'
cc=[str(native),'cc'] if native.exists() else [os.environ.get('CC',shutil.which('cc') or 'gcc')]
out=ROOT/'.cache/tests';out.mkdir(parents=True,exist_ok=True)
inc=['-Itests/stubs','-Ifirmware/main/App/Inc','-Ifirmware/main/Bsp/Inc','-Ifirmware/screen/HardWare/Inc','-Ishared/protocol']
cases={
 'home_reference':['tests/test_home_reference.c'],
 'pressure_persistence':['tests/test_pressure_persistence.c','firmware/main/Bsp/Src/pressure_tuning.c','shared/protocol/system_wire.c'],
 'motor_driver':['tests/test_motor_driver.c','firmware/main/Bsp/Src/tmc5130_driver.c'],
 'ads_zero':['tests/test_ads_zero.c','firmware/main/Bsp/Src/ads1220_driver.c'],
 'rtt_tuning':['-DPRESSURE_JLINK_TUNING_ENABLE=1','tests/test_rtt_tuning.c','firmware/main/App/Src/rtt_tuning.c','firmware/main/Bsp/Src/pressure_tuning.c','shared/protocol/system_wire.c'],
 'journal':['tests/test_journal.c','shared/protocol/config_journal.c','shared/protocol/system_wire.c'],
 'system':['tests/test_system.c','firmware/main/App/Src/app_controller.c','firmware/main/App/Src/system_link.c','firmware/main/Bsp/Src/pressure_tuning.c','firmware/screen/HardWare/Src/screen_link.c','shared/protocol/system_wire.c'],
 'button':['firmware/main/tests/test_button_debounce.c','firmware/main/App/Src/button_debounce.c'],
 'pressure_limit':['firmware/main/tests/test_pressure_limit.c'],
 'code_table':['firmware/main/tests/test_pressure_code_table.c','firmware/main/Bsp/Src/pressure_tuning.c','shared/protocol/system_wire.c'],
}
for name,src in cases.items():
 exe=out/(name+('.exe' if os.name=='nt' else ''))
 extra_inc=['-Itests/ads_stubs'] if name in ('ads_zero','motor_driver') else []
 subprocess.run(cc+['-std=c11','-O0','-UNDEBUG','-g','-Wall','-Wextra']+extra_inc+inc+src+['-o',str(exe),'-lm'],check=True)
 subprocess.run([str(exe)],check=True)
 print('PASS:',name,flush=True)
