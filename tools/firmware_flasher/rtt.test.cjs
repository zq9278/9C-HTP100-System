const test=require('node:test'),assert=require('node:assert/strict');
const {Rtt}=require('./rtt.cjs');
test('RTT JSON IPC handles fragmented replies, logs, failure and exclusive operations',async()=>{
 const events=[];const r=new Rtt((...e)=>events.push(e));
 const script=`require('readline').createInterface({input:process.stdin}).on('line',l=>{const q=JSON.parse(l);process.stdout.write(JSON.stringify({event:'log',text:'test log\\n'})+'\\n');const s=JSON.stringify(q.method==='bad'?{id:q.id,error:'rejected'}:{id:q.id,result:[1,2,3,4]})+'\\n';process.stdout.write(s.slice(0,5));setTimeout(()=>process.stdout.write(s.slice(5)),5);});`;
 r.start(process.execPath,['-e',script],process.env);
 try {
  assert.deepEqual(await r.call('read',0),[1,2,3,4]);
  assert(events.some(e=>e[0]==='log'&&e[1].includes('test log')));
  await assert.rejects(r.call('bad'),/rejected/);
  let release;const held=r.exclusive(()=>new Promise(resolve=>release=resolve));
  await assert.rejects(r.exclusive(()=>{}),/busy/);release();await held;
 }finally{await r.close();}
});
