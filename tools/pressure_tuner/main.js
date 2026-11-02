const {app,BrowserWindow,ipcMain,dialog}=require('electron');
const {spawn}=require('child_process');const path=require('path');const fs=require('fs');
let win,bridge,connected=false,closing=false,queue=Promise.resolve();
function log(s){if(win&&!win.isDestroyed())win.webContents.send('log',s);}
function state(v){connected=v;if(win&&!win.isDestroyed())win.webContents.send('connection',v);}
function enqueue(fn){const p=queue.then(fn);queue=p.catch(()=>{});return p;}
class RttBridge {
 constructor(){this.pending=new Map();this.id=0;this.buffer='';}
 start(){
  const local=path.join(__dirname,'.venv',process.platform==='win32'?'Scripts/python.exe':'bin/python');
  const python=process.env.PYTHON||(fs.existsSync(local)?local:(process.platform==='win32'?'python':'python3'));
  this.child=spawn(python,['-u',path.join(__dirname,'rtt_bridge.py')],{windowsHide:true});
  this.child.stdout.on('data',s=>this.parse(s.toString()));this.child.stderr.on('data',s=>log(s.toString()));
  this.child.on('error',e=>this.fail(e));this.child.on('exit',()=>this.fail(Error('RTT bridge exited')));
 }
 fail(e){for(const p of this.pending.values()){clearTimeout(p.timer);p.reject(e);}this.pending.clear();state(false);}
 parse(data){
  this.buffer+=data;let end;
  while((end=this.buffer.indexOf('\n'))>=0){
   const line=this.buffer.slice(0,end);this.buffer=this.buffer.slice(end+1);
   let r;try{r=JSON.parse(line);}catch{log(line);continue;}
   if(r.event==='disconnected'){this.fail(Error(r.error));log(r.error);continue;}
   const p=this.pending.get(r.id);if(!p)continue;this.pending.delete(r.id);clearTimeout(p.timer);
   r.error?p.reject(Error(r.error)):p.resolve(r.result);
  }
 }
 call(method,...args){return new Promise((resolve,reject)=>{
  const id=++this.id;const timer=setTimeout(()=>{this.pending.delete(id);reject(Error('调参通信超时'));this.child?.kill();},10000);
  this.pending.set(id,{resolve,reject,timer});
  this.child.stdin.write(JSON.stringify({id,method,args})+'\n',error=>{if(error){this.pending.delete(id);clearTimeout(timer);reject(error);}});
 });}
 async close(){try{if(connected)await this.call('disconnect');}finally{this.child?.kill();this.fail(Error('Disconnected'));}}
}
ipcMain.handle('connect',()=>enqueue(async()=>{
 if(connected)return true;if(bridge)await bridge.close();bridge=new RttBridge();bridge.start();
 try{const info=await bridge.call('connect');state(true);log('已连接 RTT，固件版本 '+info.firmware);return true;}
 catch(e){await bridge.close();bridge=null;throw e;}
}));
ipcMain.handle('read',(_,index)=>enqueue(async()=>{if(!connected)throw Error('请先连接');return bridge.call('read',index);}));
ipcMain.handle('write',(_,index,values)=>enqueue(async()=>{
 if(!connected)throw Error('请先连接');
 if(!Number.isInteger(index)||index<0||index>4||!Array.isArray(values)||values.length!==4||values.some(v=>!Number.isFinite(v))||values[0]<=0||values[0]>65535||values[1]<=0||values[1]>65535||values[2]<=0||values[2]>1000||values[3]<.01||values[3]>1)throw Error('参数超出范围');
 return bridge.call('write',index,values);
}));
async function disconnect(){try{if(bridge)await bridge.close();}finally{bridge=null;state(false);}return true;}
ipcMain.handle('disconnect',()=>enqueue(disconnect));
ipcMain.handle('flash',()=>enqueue(async()=>{if(bridge||connected)throw Error('请先断开调参连接');await require('./flash.cjs')(log);log('Tuning 固件已烧录');return true;}));
app.whenReady().then(()=>{
 win=new BrowserWindow({show:!process.argv.includes('--smoke-test'),width:1000,height:520,webPreferences:{preload:path.join(__dirname,'preload.js'),contextIsolation:true,nodeIntegration:false}});
 win.on('close',e=>{if(closing||!bridge)return;e.preventDefault();enqueue(disconnect).catch(e=>dialog.showErrorBox('断开调试',e.message)).finally(()=>{closing=true;win.close();});});
 win.loadFile(path.join(__dirname,'index.html'));
 if(process.argv.includes('--smoke-test'))win.webContents.once('did-finish-load',()=>setTimeout(()=>app.quit(),1000));
});app.on('window-all-closed',()=>app.quit());
