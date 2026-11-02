const {spawn}=require('child_process');const path=require('path');const net=require('net');const fs=require('fs');const os=require('os');
const root=path.resolve(__dirname,'../../firmware/main');
const elf=path.join(root,'build/Tuning/9C-HTP100-main-v2.elf');
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

let log = () => {};
class Flasher {
 constructor(){this.pending=new Map();this.seq=0;this.buffer='';this.stopped=false;this.connected=false;this.mismatch=false;this.serverOutput='';}
 command(c,timeout=5000){return new Promise((resolve,reject)=>{if(!this.gdb?.stdin.writable)return reject(Error('Flasher disconnected'));const id=++this.seq;const timer=setTimeout(()=>{this.pending.delete(id);reject(Error('GDB timeout: '+c));},timeout);this.pending.set(id,{resolve,reject,timer});this.gdb.stdin.write(id+c+'\n');});}
 parse(data){this.buffer+=data;let pos;while((pos=this.buffer.indexOf('\n'))>=0){const line=this.buffer.slice(0,pos);this.buffer=this.buffer.slice(pos+1);if(line.includes('MIS-MATCHED'))this.mismatch=true;if(line.startsWith('*stopped')){this.stopped=true;this.onStop?.();}if(line.startsWith('*running'))this.stopped=false;const m=line.match(/^(\d+)\^(\w+)(.*)/);if(m&&this.pending.has(+m[1])){const p=this.pending.get(+m[1]);this.pending.delete(+m[1]);clearTimeout(p.timer);m[2]==='error'?p.reject(Error(m[3])):p.resolve(m[3]);}}}
 async halt(){if(this.stopped)return;let timer;const event=new Promise((r,j)=>{this.onStop=r;timer=setTimeout(()=>j(Error('Cannot halt target')),5000);});try{await Promise.all([event,this.command('-exec-interrupt')]);}finally{clearTimeout(timer);this.onStop=null;}}
 async connect(){
  if(!fs.existsSync(elf))throw Error('未找到 Tuning 固件，请先编译并烧录。');
  const serverExecutable=serverPath(),gdbExecutable=gdbPath();
  await new Promise((r,j)=>{const p=net.createServer();p.once('error',()=>j(Error('Port 2337 is occupied. Close previous debugger.')));p.listen(2337,'127.0.0.1',()=>p.close(r));});
  this.server=spawn(serverExecutable,['-device','STM32G070CB','-if','SWD','-speed','1000','-port','2337','-swoport','2338','-telnetport','2339','-singlerun','-nogui','-noreset'],{windowsHide:true});
  await new Promise((resolve,reject)=>{const t=setTimeout(()=>reject(Error('J-Link server startup timeout')),15000);const capture=b=>{this.serverOutput=(this.serverOutput+b.toString()).slice(-4000);if(/Waiting for GDB connection/.test(this.serverOutput)){clearTimeout(t);resolve();}};this.server.stdout.on('data',capture);this.server.stderr.on('data',capture);this.server.once('error',e=>{clearTimeout(t);reject(e);});this.server.once('exit',()=>{clearTimeout(t);reject(Error('J-Link server exited: '+this.serverOutput.slice(-1500)));});});
  this.gdb=spawn(gdbExecutable,['--interpreter=mi2','--quiet',elf],{windowsHide:true});this.gdb.stdout.on('data',b=>this.parse(b.toString()));this.gdb.stderr.on('data',b=>log(b.toString()));this.gdb.on('error',e=>log(e.message));this.gdb.on('exit',()=>{this.connected=false;for(const p of this.pending.values()){clearTimeout(p.timer);p.reject(Error('Flasher disconnected'));}this.pending.clear();});
  await this.command('-gdb-set mi-async on');await this.command('-gdb-set remotetimeout 180');await this.command('-target-select remote 127.0.0.1:2337',60000);this.attached=true;log('正在写入固件，请勿断开连接…');await this.command('-file-exec-and-symbols '+JSON.stringify(elf));await this.command('-target-download',120000);this.mismatch=false;await this.command('-interpreter-exec console "compare-sections"',120000);if(this.mismatch)throw Error('烧录校验失败');await this.command('-interpreter-exec console "monitor reset"',180000);this.mismatch=false;await this.command('-interpreter-exec console "compare-sections"',180000);if(this.mismatch)throw Error('复位后烧录校验失败');await this.command('-target-detach',180000);this.attached=false;
 }
 async dispose(){if(this.attached){try{await this.halt();await this.command('-target-detach');this.attached=false;}catch(e){log('Detach failed: '+e.message);}}if(this.gdb){try{await this.command('-gdb-exit');}catch{}this.gdb.kill();}if(this.server&&this.server.exitCode===null){const child=this.server;await new Promise(resolve=>{const timer=setTimeout(resolve,2000);child.once('exit',()=>{clearTimeout(timer);resolve();});child.kill();});}}
}

module.exports = async function flashTuning(onLog = () => {}) {
 log = onLog;
 await run('cmake',['--preset','Tuning']);
 await run('cmake',['--build','build/Tuning','--parallel']);
 const d=new Flasher();try { await d.connect(); } finally { await d.dispose(); }
};
