const test=require('node:test'),assert=require('node:assert/strict'),vm=require('node:vm'),fs=require('node:fs');
function setup(){
 const c=vm.createContext({require:n=>n==='electron'?{app:{whenReady:()=>({then(){}}),on(){}},ipcMain:{handle(){}},dialog:{}}:require(n),process,__dirname,setTimeout,clearTimeout,console});
 vm.runInContext(fs.readFileSync(__dirname+'/main.js','utf8')+'\nglobalThis.Bridge=RttBridge;',c);return new c.Bridge();
}
test('fragmented replies complete only the matching request',async()=>{
 const b=setup();b.child={stdin:{write:(s,cb)=>cb()}};
 const p=b.call('read',1);b.parse('{"id":1,"res');b.parse('ult":[1,2,3,4]}\n');assert.deepEqual(Array.from(await p),[1,2,3,4]);assert.equal(b.pending.size,0);
});
test('bridge failure rejects all pending requests',async()=>{
 const b=setup();b.child={stdin:{write:(s,cb)=>cb()}};
 const p=b.call('read',1),q=b.call('write',1,[1,2,3,.4]);b.fail(Error('disconnected'));
 await assert.rejects(p,/disconnected/);await assert.rejects(q,/disconnected/);assert.equal(b.pending.size,0);
});
test('firmware rejection is not displayed as applied',async()=>{
 const b=setup();b.child={stdin:{write:(s,cb)=>cb()}};
 const p=b.call('write',0,[1,2,3,.4]);b.parse('{"id":1,"error":"rejected"}\n');await assert.rejects(p,/rejected/);
});
test('stdin failure releases the request timer',async()=>{
 const b=setup();b.child={stdin:{write:(s,cb)=>cb(Error('pipe closed'))}};
 await assert.rejects(b.call('read',0),/pipe closed/);assert.equal(b.pending.size,0);
});
