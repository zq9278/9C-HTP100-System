const test = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

async function page(fail = false, preferences = {}) {
  const elements = new Map();
  const get = id => {
    if (!elements.has(id)) elements.set(id, {value:'', textContent:'', checked:false, hidden:false, open:false,
      showModal(){this.open=true;}, close(){this.open=false;}});
    return elements.get(id);
  };
  for (const id of ['preset','jlink','serial','speed','rebuild','build','flash','browse','output','driver','success-close','success-dialog']) get(id);
  get('rebuild').checked=true;get('preset').value='Release';get('speed').value='1000';
  const role={value:'screen'}, panel={value:'screen-old'};
  const info={role:panel.value,preset:'Release',lcd_panel:'OLD',file:'screen-old.hex',git_commit:'abc1234567890',sha256:'abc',built_at:new Date().toISOString(),path:'bundled'};
  let sent, saved;
  const context={document:{getElementById:get,querySelector:q=>q==='[name=panel]:checked'?panel:role,querySelectorAll:q=>q==='[name=role]'?[role]:q==='[name=panel]'?[panel]:[...elements.values()]},
    flasher:{info:async()=>({bundled:true,jlink:'bundled.exe',root:'bundle',preferences}),inspect:async()=>info,onLog:()=>{},onProgress:()=>{},savePreferences:async value=>{saved=value;},
      flash:async options=>{sent=options;if(fail)throw Error('Download failed');return info;}}};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname,'renderer.js'),'utf8'),context);
  await new Promise(resolve=>setImmediate(resolve));
  return {get,role,panel,sent:()=>sent,saved:()=>saved};
}
test('saved speed and serial restore; bundled runtime and Release cannot be overridden',async()=>{
  const p=await page(false,{speed:'8000',serial:'654321',preset:'Debug',rebuild:true,jlink:'wrong.exe'});
  assert.equal(p.get('speed').value,'8000');assert.equal(p.get('serial').value,'654321');
  assert.equal(p.get('preset').value,'Release');assert.equal(p.get('rebuild').checked,false);
  assert.equal(p.get('jlink').value,'bundled.exe');
  p.get('speed').value='4000';p.get('speed').onchange();
  assert.equal(p.saved().speed,'4000');assert.equal(p.saved().panel,'screen-old');
  p.get('serial').value='';p.get('serial').oninput();assert.equal(p.saved().serial,'');
});
test('distribution hides build controls and shows success only after resolved flash',async()=>{
  const p=await page();
  assert.equal(p.get('build').hidden,true);assert.equal(p.get('rebuild').checked,false);
  assert.equal(p.get('success-dialog').open,false);
  await p.get('flash').onclick();
  assert.equal(p.sent().rebuild,false);assert.equal(p.sent().preset,'Release');
  assert.equal(p.get('flash-bar').value,4);assert.equal(p.get('flash-progress').hidden,false);assert.equal(p.get('success-dialog').open,true);assert.match(p.get('success-detail').textContent,/旧屏幕/);
  assert.match(p.get('status').textContent,/烧录成功/);
  p.get('success-close').onclick();assert.equal(p.get('success-dialog').open,false);
  p.role.value='main';await p.role.onchange();assert.equal(p.get('status').textContent,'就绪');
});
test('failed flash shows error and log, never a success dialog',async()=>{
  const p=await page(true);await p.get('flash').onclick();
  assert.equal(p.get('success-dialog').open,false);assert.equal(p.get('status').className,'error');
  assert.notEqual(p.get('flash-bar').value,4);assert.equal(p.get('flash-progress').className,'failed');assert.equal(p.get('logs').open,true);assert.match(p.get('log').textContent,/Download failed/);
  assert.equal(p.get('flash').disabled,false);
});

test('nested screen selection maps to firmware and preserves panel choice',async()=>{
  const p=await page();
  assert.equal(p.get('screen-variants').hidden,false);
  await p.get('flash').onclick(); assert.equal(p.sent().role,'screen-old');
  p.panel.value='screen-new'; await p.panel.onchange();
  await p.get('flash').onclick(); assert.equal(p.sent().role,'screen-new');
  p.role.value='main'; await p.role.onchange();
  assert.equal(p.get('screen-variants').hidden,true);
  await p.get('flash').onclick(); assert.equal(p.sent().role,'main');
  p.role.value='screen'; await p.role.onchange();
  assert.equal(p.get('screen-variants').hidden,false);
  await p.get('flash').onclick(); assert.equal(p.sent().role,'screen-new');
});

test('November 11 panel selects its independent firmware',async()=>{
  const p=await page(); p.panel.value='screen-nov11'; await p.panel.onchange();
  await p.get('flash').onclick(); assert.equal(p.sent().role,'screen-nov11');
  assert.equal(p.saved().panel,'screen-nov11');
});
