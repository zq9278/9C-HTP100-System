const test=require('node:test'),assert=require('node:assert/strict'),vm=require('node:vm'),fs=require('node:fs');
function setup(){const c=vm.createContext({require:n=>n==='electron'?{app:{whenReady:()=>({then(){}}),on(){}},ipcMain:{handle(){}},dialog:{}}:require(n),process,__dirname,setTimeout,clearTimeout,console});vm.runInContext(fs.readFileSync(__dirname+'/main.js','utf8')+'\nglobalThis.Debugger=Debugger;',c);return c;}
test('MI fragmented response resolves numeric command',async()=>{const c=setup(),d=new c.Debugger();d.gdb={stdin:{writable:true,write(){}}};const p=d.numeric('x');d.parse('1^done,val');d.parse('ue="0.300000012"\n');assert.equal(await p,0.300000012);});
test('resume even if update fails',async()=>{const c=setup(),d=new c.Debugger(),events=[];d.halt=async()=>events.push('halt');d.command=async s=>events.push(s);await assert.rejects(d.transact(async()=>{throw Error('bad')}),/bad/);assert.deepEqual(events,['halt','-exec-continue']);});
test('defaults restore precedes detach',async()=>{const c=setup(),d=new c.Debugger(),events=[];d.connected=true;d.attached=true;d.transact=async fn=>fn();d.halt=async()=>{};d.command=async x=>events.push(x);d.dispose=async()=>{};await d.disconnect();assert.equal(events.length,6);assert.match(events[4],/g_defaults\[4\]/);assert.equal(events[5],'-target-detach');});
test('read retrieves selected profile in one transaction and rejects invalid profiles',async()=>{
 const handlers={};
 const c=vm.createContext({require:n=>n==='electron'?{app:{whenReady:()=>({then(){}}),on(){}},ipcMain:{handle:(name,fn)=>handlers[name]=fn},dialog:{}}:require(n),process,__dirname,setTimeout,clearTimeout,console});
 vm.runInContext(fs.readFileSync(__dirname+'/main.js','utf8'),c);
 const expressions=[];c.fake={connected:true,transact:async fn=>fn(),numeric:async expr=>{expressions.push(expr);return expressions.length;}};
 vm.runInContext('session=fake',c);
 assert.deepEqual(Array.from(await handlers.read(null,2)),[1,2,3,4]);
 assert.deepEqual(expressions,['g_profiles[2].fast_speed','g_profiles[2].retract_speed','g_profiles[2].kp','g_profiles[2].approach_ratio']);
 await assert.rejects(handlers.read(null,5),/档位无效/);
});
test('disconnect cleans up and warns when restoring defaults fails',async()=>{
 const c=setup(),d=new c.Debugger();let disposed=false;
 d.connected=true;d.attached=true;d.transact=async()=>{throw Error('Cannot access memory');};d.dispose=async()=>{disposed=true;};
 await assert.rejects(d.disconnect(),/无法确认默认参数已恢复.*重启设备/);
 assert.equal(disposed,true);assert.equal(d.connected,false);assert.equal(d.attached,false);
});
test('telemetry uses the target pressure to read the active profile',async()=>{
 const handlers={};const c=vm.createContext({require:n=>n==='electron'?{app:{whenReady:()=>({then(){}}),on(){}},ipcMain:{handle:(n,f)=>handlers[n]=f},dialog:{}}:require(n),process,__dirname,setTimeout,clearTimeout,console});
 vm.runInContext(fs.readFileSync(__dirname+'/main.js','utf8'),c);
 const expressions=[];c.fake={connected:true,transact:async fn=>fn(),numeric:async expr=>{expressions.push(expr);if(expr==='g_home_pressure_target')return 350;if(expr==='g_ads_last_unfiltered_pressure_x10')return 1234;return 1;}};vm.runInContext('session=fake',c);
 const t=await handlers.telemetry();assert.equal(t.index,2);assert.equal(t.raw,123.4);assert.deepEqual(expressions.slice(-4),['g_profiles[2].fast_speed','g_profiles[2].retract_speed','g_profiles[2].kp','g_profiles[2].approach_ratio']);
});
