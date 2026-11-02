const {app,BrowserWindow,ipcMain,dialog}=require('electron');
const {spawn}=require('child_process');const path=require('path');const net=require('net');const fs=require('fs');const os=require('os');
const root=path.resolve(__dirname,'../..');
const elf=path.join(root,'build/Debug/9C-HTP100-main-v2.elf');
const bundleRoots=[process.env.STM32CUBE_BUNDLE_PATH,process.env.CUBE_BUNDLE_PATH,process.env.LOCALAPPDATA&&path.join(process.env.LOCALAPPDATA,'stm32cube/bundles'),path.join(os.homedir(),'Library/Application Support/stm32cube/bundles'),path.join(os.homedir(),'.stm32cube/bundles')].filter(Boolean);
function executable(name,override,group,extra=[]){
 const candidates=[process.env[override],...extra];
 for(const base of bundleRoots){const dir=path.join(base,group);if(fs.existsSync(dir))for(const version of fs.readdirSync(dir).sort().reverse())candidates.push(path.join(dir,version,'bin',name));}
 for(const dir of (process.env.PATH||'').split(path.delimiter))candidates.push(path.join(dir,name));
 for(const file of candidates.filter(Boolean)){try{fs.accessSync(file,fs.constants.X_OK);return file;}catch{}}
 throw Error(`找不到 ${name}。请安装对应工具，或设置 ${override} 环境变量。`);
}
function serverPath(){return executable(process.platform==='win32'?'JLinkGDBServerCL.exe':'JLinkGDBServerCLExe','JLINK_GDB_SERVER','jlink-gdbserver',['/Applications/SEGGER/JLink/JLinkGDBServerCLExe','/usr/local/bin/JLinkGDBServerCLExe']);}
function gdbPath(){return executable(process.platform==='win32'?'arm-none-eabi-gdb.exe':'arm-none-eabi-gdb','ARM_GDB','gnu-gdb-for-stm32');}
function run(command,args){return new Promise((resolve,reject)=>{log('> '+[command,...args].join(' '));const child=spawn(command,args,{cwd:root,windowsHide:true});child.stdout.on('data',b=>log(b.toString()));child.stderr.on('data',b=>log(b.toString()));child.once('error',reject);child.once('close',code=>code===0?resolve():reject(Error(`${command} 失败（退出码 ${code}）`)));});}

let win,session,connecting,closing=false,queue=Promise.resolve(),operations=0;
const fields=['fast_speed','retract_speed','kp','approach_ratio'];
function enqueue(fn){operations++;const p=queue.then(fn);queue=p.catch(()=>{});return p.finally(()=>operations--);}
function log(s){if(win&&!win.isDestroyed())win.webContents.send('log',s);}
class Debugger {
 constructor(){this.pending=new Map();this.seq=0;this.buffer='';this.stopped=false;this.connected=false;this.mismatch=false;this.serverOutput='';}
 command(c,timeout=5000){return new Promise((resolve,reject)=>{if(!this.gdb?.stdin.writable)return reject(Error('Debugger disconnected'));const id=++this.seq;const timer=setTimeout(()=>{this.pending.delete(id);reject(Error('GDB timeout: '+c));},timeout);this.pending.set(id,{resolve,reject,timer});this.gdb.stdin.write(id+c+'\n');});}
 parse(data){this.buffer+=data;let pos;while((pos=this.buffer.indexOf('\n'))>=0){const line=this.buffer.slice(0,pos);this.buffer=this.buffer.slice(pos+1);if(line.includes('MIS-MATCHED'))this.mismatch=true;if(line.startsWith('*stopped')){this.stopped=true;this.onStop?.();}if(line.startsWith('*running'))this.stopped=false;const m=line.match(/^(\d+)\^(\w+)(.*)/);if(m&&this.pending.has(+m[1])){const p=this.pending.get(+m[1]);this.pending.delete(+m[1]);clearTimeout(p.timer);m[2]==='error'?p.reject(Error(m[3])):p.resolve(m[3]);}}}
 async numeric(expr){const r=await this.command('-data-evaluate-expression '+JSON.stringify(expr));const m=r.match(/value="([^" ]+)"/);const n=m?Number(m[1]):NaN;if(!Number.isFinite(n))throw Error('Invalid GDB value: '+r);return n;}
 async halt(){if(this.stopped)return;let timer;const event=new Promise((r,j)=>{this.onStop=r;timer=setTimeout(()=>j(Error('Cannot halt target')),5000);});try{await Promise.all([event,this.command('-exec-interrupt')]);}finally{clearTimeout(timer);this.onStop=null;}}
 async transact(fn){await this.halt();try{return await fn();}finally{this.stopped=false;await this.command('-exec-continue');}}
 async connect(flash=false){
  if(!fs.existsSync(elf))throw Error('未找到 Debug 固件，请先编译并烧录。');
  const serverExecutable=serverPath(),gdbExecutable=gdbPath();
  await new Promise((r,j)=>{const p=net.createServer();p.once('error',()=>j(Error('Port 2337 is occupied. Close previous debugger.')));p.listen(2337,'127.0.0.1',()=>p.close(r));});
  this.server=spawn(serverExecutable,['-device','STM32G070CB','-if','SWD','-speed','1000','-port','2337','-swoport','2338','-telnetport','2339','-singlerun','-nogui','-noreset'],{windowsHide:true});
  await new Promise((resolve,reject)=>{const t=setTimeout(()=>reject(Error('J-Link server startup timeout')),15000);const capture=b=>{this.serverOutput=(this.serverOutput+b.toString()).slice(-4000);if(/Waiting for GDB connection/.test(this.serverOutput)){clearTimeout(t);resolve();}};this.server.stdout.on('data',capture);this.server.stderr.on('data',capture);this.server.once('error',e=>{clearTimeout(t);reject(e);});this.server.once('exit',()=>{clearTimeout(t);reject(Error('J-Link server exited: '+this.serverOutput.slice(-1500)));});});
  this.gdb=spawn(gdbExecutable,['--interpreter=mi2','--quiet',elf],{windowsHide:true});this.gdb.stdout.on('data',b=>this.parse(b.toString()));this.gdb.stderr.on('data',b=>log(b.toString()));this.gdb.on('error',e=>log(e.message));this.gdb.on('exit',()=>{this.connected=false;for(const p of this.pending.values()){clearTimeout(p.timer);p.reject(Error('Debugger disconnected'));}this.pending.clear();if(session===this)win?.webContents.send('connection',false);});
  await this.command('-gdb-set mi-async on');await this.command('-gdb-set remotetimeout 180');await this.command('-target-select remote 127.0.0.1:2337',60000);this.attached=true;if(flash){log('正在写入固件，请勿断开连接…');await this.command('-file-exec-and-symbols '+JSON.stringify(elf));await this.command('-target-download',120000);this.mismatch=false;await this.command('-interpreter-exec console "compare-sections"',120000);if(this.mismatch)throw Error('烧录校验失败');await this.command('-interpreter-exec console "monitor reset"',180000);this.mismatch=false;await this.command('-interpreter-exec console "compare-sections"',180000);if(this.mismatch)throw Error('复位后烧录校验失败');await this.command('-target-detach',180000);this.attached=false;await this.dispose();return;}await this.command('-interpreter-exec console "compare-sections .text"');if(this.mismatch)throw Error('Firmware and ELF differ. Flash current Debug firmware first.');if(await this.numeric('g_pressure_jlink_tuning_enabled')!==1)throw Error('Firmware live tuning is disabled');this.stopped=false;await this.command('-exec-continue');await new Promise(resolve=>setTimeout(resolve,1500));this.connected=true;
 }
 async disconnect(){
  let failure;
  try{if(this.connected){await this.transact(async()=>{for(let i=0;i<5;i++)await this.command('-data-evaluate-expression '+JSON.stringify(`g_profiles[${i}]=g_defaults[${i}]`));});}if(this.attached){await this.halt();await this.command('-target-detach');this.attached=false;}}
  catch(e){failure=e;this.attached=false;}
  finally{this.connected=false;await this.dispose();}
  if(failure)throw Error('无法确认默认参数已恢复，请重启设备后再使用。检查 J-Link 和目标板连接。详情：'+failure.message);
 }
 async dispose(){if(this.attached){try{await this.halt();await this.command('-target-detach');this.attached=false;}catch(e){log('Detach failed: '+e.message);}}if(this.gdb){try{await this.command('-gdb-exit');}catch{}this.gdb.kill();}if(this.server&&this.server.exitCode===null){const child=this.server;await new Promise(resolve=>{const timer=setTimeout(resolve,2000);child.once('exit',()=>{clearTimeout(timer);resolve();});child.kill();});}}
}
ipcMain.handle('connect',()=>{if(session?.connected)return true;if(!connecting)connecting=enqueue(async()=>{const s=new Debugger();session=s;try{await s.connect();return true;}catch(e){await s.dispose();session=null;throw e;}}).finally(()=>connecting=null);return connecting;});
ipcMain.handle('flash',()=>enqueue(async()=>{if(session||connecting)throw Error('请先停止调试，再烧录固件');log('正在配置并编译 Debug 固件…');await run('cmake',['--preset','Debug']);await run('cmake',['--build','build/Debug','--parallel']);const s=new Debugger();try{await s.connect(true);log('固件烧录并校验成功，设备已复位。');}catch(e){throw Error('烧录失败，设备固件可能不完整。请检查 J-Link、目标板供电及 SWD 接线后重新烧录。\n'+e.message+(s.serverOutput?'\nJ-Link 日志：\n'+s.serverOutput.slice(-1500):''));}finally{await s.dispose();}return true;}));
ipcMain.handle('telemetry',()=>enqueue(async()=>{if(!session?.connected)throw Error('请先连接 J-Link');return session.transact(async()=>{
 const expressions=['(int)g_pressure_active','(int)g_pressure_recovering','(int)g_ads_sample_valid','(int)g_pressure_stage','g_home_pressure_target','g_pressure_filtered','g_ads_last_unfiltered_pressure_x10','g_pressure_error','g_pressure_command','(int)g_pressure_hold_timer_started','g_pressure_p_speed_limit'];
 const values=[];for(const expression of expressions)values.push(await session.numeric(expression));
 const target=values[4],index=target<=200?0:target<=300?1:target<=400?2:target<=500?3:4;
 const profile=[];for(const field of fields)profile.push(await session.numeric(`g_profiles[${index}].${field}`));
 return {active:values[0],recovering:values[1],valid:values[2],stage:values[3],target,pressure:values[5],raw:values[6]/10,error:values[7],speed:values[8],holdStarted:values[9],limit:values[10],index,profile};
 });}));
ipcMain.handle('read',(_,index)=>enqueue(async()=>{if(!session?.connected)throw Error('请先连接 J-Link');if(!Number.isInteger(index)||index<0||index>4)throw Error('档位无效');return session.transact(async()=>{const values=[];for(const f of fields)values.push(await session.numeric(`g_profiles[${index}].${f}`));return values;});}));
ipcMain.handle('write',(_,index,values)=>enqueue(async()=>{if(!session?.connected)throw Error('Not connected');if(!Number.isInteger(index)||index<0||index>4||!Array.isArray(values)||values.length!==4||values.some(v=>!Number.isFinite(v))||values[0]<0||values[0]>65535||values[1]<0||values[1]>65535||values[2]<0||values[2]>1000||values[3]<0.01||values[3]>1)throw Error('Parameter out of range');return session.transact(async()=>{const old=[];for(const f of fields)old.push(await session.numeric(`g_profiles[${index}].${f}`));try{for(let i=0;i<4;i++){await session.numeric(`g_profiles[${index}].${fields[i]}=${values[i]}`);const actual=await session.numeric(`g_profiles[${index}].${fields[i]}`);if(Math.abs(actual-values[i])>Math.max(1e-5,Math.abs(values[i])*1e-5))throw Error('Write verification failed');}}catch(e){for(let i=0;i<4;i++)await session.numeric(`g_profiles[${index}].${fields[i]}=${old[i]}`);throw e;}return true;});}));
function disconnect(){return enqueue(async()=>{if(session){const s=session;try{await s.disconnect();}finally{if(session===s)session=null;if(win&&!win.isDestroyed())win.webContents.send('connection',false);}}return true;});}ipcMain.handle('disconnect',disconnect);
app.whenReady().then(()=>{win=new BrowserWindow({width:1000,height:520,webPreferences:{preload:path.join(__dirname,'preload.js'),contextIsolation:true,nodeIntegration:false}});win.on('close',e=>{if(closing||(!session&&!connecting&&!operations))return;e.preventDefault();disconnect().then(()=>{closing=true;win.close();}).catch(e=>{dialog.showErrorBox('断开调试',e.message);closing=true;win.close();});});win.loadFile(path.join(__dirname,'index.html'));if(process.argv.includes('--smoke-test'))win.webContents.once('did-finish-load',()=>setTimeout(()=>app.quit(),1000));});app.on('window-all-closed',()=>app.quit());
