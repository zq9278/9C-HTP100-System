const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), os = require('node:os'), crypto = require('node:crypto');
const {Programmer, validate, commandScript, commandArgs, LOADER} = require('./backend.cjs');
const base = {role: 'screen-old', preset: 'Release', speed: 1000, serial: '123456'};
test('targets are closed enums; probe arguments cannot inject commands', () => {
  assert.throws(() => validate({...base, role: '../main'}));
  assert.throws(() => validate({...base, serial: '123\nloadfile evil.hex'}));
  assert.throws(() => validate({...base, speed: '1000;erase'}));
  const args = commandArgs(base, 'C:/with spaces/program.jlink');
  assert.equal(args.at(-1), 'C:/with spaces/program.jlink');
  assert.ok(args.includes('STM32H750XB'));assert.ok(args.includes('123456'));
});
test('both LCD targets use the same QSPI loader; main never selects external flash', () => {
  for (const role of ['screen-old','screen-new']) {
    const script = commandScript(role);
    assert.ok(script.includes(LOADER));assert.ok(script.includes('loadfile "firmware.hex"'));
    assert.doesNotMatch(script,/\berase\b|unlock|secure|optionbytes/i);
  }
  assert.ok(!commandScript('main').includes('DEVICE_SelectLoader'));
});
test('failed compile stops before probe launch and releases busy state', async () => {
  const calls=[];const p=new Programmer({runner:async(cmd,args)=>{calls.push(args);throw Error('compile failed');}});
  await assert.rejects(p.flash({...base,jlink:process.execPath,rebuild:true}),/compile failed/);assert.equal(p.busy,false);assert.equal(calls.length,1);
  assert.equal(calls[0][0],'scripts/build.py');
  assert.ok(calls[0].includes('screen-old'));
});
test('concurrent operations are rejected, not queued to flash a later board', async () => {
  const p=new Programmer();let finish;
  const running=p.exclusive(()=>new Promise(resolve=>finish=resolve));
  await assert.rejects(p.build(base),/尚未完成/);finish();await running;assert.equal(p.busy,false);
});
test('flash uses frozen verified bytes and requires a download completion reply', async () => {
  const folder=fs.mkdtempSync(path.join(os.tmpdir(),'htp-test-'));
  const source=path.join(folder,'sample.hex'),tool=path.join(folder,'JLink.exe');
  fs.writeFileSync(source,'fixture');fs.writeFileSync(tool,'mock');
  const info={role:base.role,preset:base.preset,path:source,sha256:crypto.createHash('sha256').update('fixture').digest('hex')};
  let flashes=0,good=true;
  const p=new Programmer({runner:async(cmd,args,opts)=>{
    if(args[0]==='scripts/package_info.py')return JSON.stringify(info);
    flashes++;assert.equal(cmd,tool);assert.equal(fs.readFileSync(path.join(opts.cwd,'firmware.hex'),'utf8'),'fixture');
    assert.ok(fs.readFileSync(path.join(opts.cwd,'program.jlink'),'utf8').includes(LOADER));
    return good?'Downloading file [firmware.hex]...\nO.K.\n':'Connected only';
  }});
  try {
    await p.flash({...base,jlink:tool,rebuild:false});assert.equal(flashes,1);
    good=false;await assert.rejects(p.flash({...base,jlink:tool}),/完整下载成功/);
    info.sha256='bad';await assert.rejects(p.flash({...base,jlink:tool}),/发生变化/);assert.equal(flashes,2);
  } finally {fs.unlinkSync(source);fs.unlinkSync(tool);fs.rmdirSync(folder);}
});
