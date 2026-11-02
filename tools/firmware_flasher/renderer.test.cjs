const test = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

async function page(fail = false) {
  const elements = new Map();
  const get = id => {
    if (!elements.has(id)) elements.set(id, {value:'', textContent:'', checked:false, hidden:false, open:false,
      showModal(){this.open=true;}, close(){this.open=false;}});
    return elements.get(id);
  };
  for (const id of ['preset','jlink','serial','speed','rebuild','build','flash','browse','output','driver','success-close','success-dialog']) get(id);
  get('rebuild').checked=true;get('preset').value='Release';get('speed').value='1000';
  const role={value:'screen-old'};
  const info={role:role.value,preset:'Release',lcd_panel:'OLD',file:'screen-old.hex',git_commit:'abc1234567890',sha256:'abc',built_at:new Date().toISOString(),path:'bundled'};
  let sent;
  const context={document:{getElementById:get,querySelector:()=>role,querySelectorAll:q=>q==='[name=role]'?[role]:[...elements.values()]},
    flasher:{info:async()=>({bundled:true,jlink:'bundled.exe',root:'bundle'}),inspect:async()=>info,onLog:()=>{},
      flash:async options=>{sent=options;if(fail)throw Error('Download failed');return info;}}};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname,'renderer.js'),'utf8'),context);
  await new Promise(resolve=>setImmediate(resolve));
  return {get,role,sent:()=>sent};
}
test('distribution hides build controls and shows success only after resolved flash',async()=>{
  const p=await page();
  assert.equal(p.get('build').hidden,true);assert.equal(p.get('rebuild').checked,false);
  assert.equal(p.get('success-dialog').open,false);
  await p.get('flash').onclick();
  assert.equal(p.sent().rebuild,false);assert.equal(p.sent().preset,'Release');
  assert.equal(p.get('success-dialog').open,true);assert.match(p.get('success-detail').textContent,/旧屏幕/);
  assert.match(p.get('status').textContent,/烧录成功/);
  p.get('success-close').onclick();assert.equal(p.get('success-dialog').open,false);
  p.role.value='main';await p.role.onchange();assert.equal(p.get('status').textContent,'就绪');
});
test('failed flash shows error and log, never a success dialog',async()=>{
  const p=await page(true);await p.get('flash').onclick();
  assert.equal(p.get('success-dialog').open,false);assert.equal(p.get('status').className,'error');
  assert.equal(p.get('logs').open,true);assert.match(p.get('log').textContent,/Download failed/);
  assert.equal(p.get('flash').disabled,false);
});
