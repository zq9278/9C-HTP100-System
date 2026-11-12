const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), os = require('node:os'), crypto = require('node:crypto');
const {Programmer, validate, commandScript, commandArgs, LOADER} = require('./backend.cjs');
const base = {role: 'screen-old', preset: 'Release', speed: 1000, serial: '123456'};
test('8000 kHz reaches J-Link; speeds above the supported maximum are rejected', () => {
  const args = commandArgs({...base, speed:8000}, 'program.jlink');
  assert.equal(args[args.indexOf('-Speed')+1], '8000');
  assert.throws(() => validate({...base,speed:12000}));
});
test('preferences survive restart and corrupted settings fall back to defaults', () => {
  const pref = require('./preferences.cjs');
  const folder = fs.mkdtempSync(path.join(os.tmpdir(),'htp-preferences-'));
  const file = path.join(folder,'settings.json');
  try {
    assert.deepEqual(pref.read(file),{});
    const settings={target:'screen',panel:'screen-old',speed:'8000',serial:'123456',preset:'Release',rebuild:false};
    pref.save(file,settings); assert.deepEqual(pref.read(file),settings);
    pref.save(file,{...settings,target:'main'}); assert.equal(pref.read(file).target,'main');
    assert.equal(pref.read(file).panel,'screen-old');
    assert.deepEqual(pref.clean({speed:'9000',serial:'123\nloadfile x',target:'bad'}),{});
    fs.writeFileSync(file,'broken');assert.deepEqual(pref.read(file),{});
  } finally {fs.rmSync(file,{force:true});fs.rmdirSync(folder);}
});
test('targets are closed enums; probe arguments cannot inject commands', () => {
  assert.throws(() => validate({...base, role: '../main'}));
  assert.throws(() => validate({...base, serial: '123\nloadfile evil.hex'}));
  assert.throws(() => validate({...base, speed: '1000;erase'}));
  const args = commandArgs(base, 'C:/with spaces/program.jlink');
  assert.equal(args.at(-1), 'C:/with spaces/program.jlink');
  assert.ok(args.includes('STM32H750XB'));assert.ok(args.includes('123456'));
});
test('all three LCD targets use the same QSPI loader; main never selects external flash', () => {
  for (const role of ['screen-old','screen-new','screen-nov11']) {
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
  let flashes=0,good=true;const steps=[];
  const p=new Programmer({runner:async(cmd,args,opts)=>{
    if(args[0]==='scripts/package_info.py')return JSON.stringify(info);
    flashes++;assert.equal(cmd,tool);assert.equal(fs.readFileSync(path.join(opts.cwd,'firmware.hex'),'utf8'),'fixture');
    assert.ok(fs.readFileSync(path.join(opts.cwd,'program.jlink'),'utf8').includes(LOADER));
    if (good) {opts.log('Downloa');opts.log('ding file [firmware.hex]...\n');opts.log('O.K.\n');}
    return good?'Downloading file [firmware.hex]...\nO.K.\n':'Connected only';
  }});
  try {
    await p.flash({...base,jlink:tool,rebuild:false},()=>{},step=>steps.push(step));assert.equal(flashes,1);
    assert.deepEqual(steps,[1,2,3]);
    good=false;await assert.rejects(p.flash({...base,jlink:tool}),/完整下载成功/);
    info.sha256='bad';await assert.rejects(p.flash({...base,jlink:tool}),/发生变化/);assert.equal(flashes,2);
  } finally {fs.unlinkSync(source);fs.unlinkSync(tool);fs.rmdirSync(folder);}
});

test('distribution runs only the bundled tool, needs no Python, and rejects modified firmware/runtime', async () => {
  const folder=fs.mkdtempSync(path.join(os.tmpdir(),'htp-bundle-'));
  const digest=value=>crypto.createHash('sha256').update(value).digest('hex');
  fs.mkdirSync(path.join(folder,'firmware'));fs.mkdirSync(path.join(folder,'jlink'));
  const firmware=path.join(folder,'firmware/screen-old.hex'), tool=path.join(folder,'jlink/JLink.exe');
  fs.writeFileSync(firmware,'fixture');fs.writeFileSync(tool,'tool');
  const info={role:'screen-old',preset:'Release',lcd_panel:'OLD',file:'screen-old.hex',sha256:digest('fixture')};
  const manifest={images:{'screen-old':info},runtime:{'jlink/JLink.exe':digest('tool')}};
  fs.writeFileSync(path.join(folder,'bundle.json'),JSON.stringify(manifest));
  let calls=0;
  const p=new Programmer({bundleRoot:folder,runner:async(command)=>{
    calls++;assert.equal(command,tool);return 'Downloading file [firmware.hex]...\nO.K.\n';
  }});
  try {
    assert.equal((await p.inspect(base)).sha256,info.sha256);assert.equal(calls,0);
    await p.flash({...base,jlink:'ignored.exe'});assert.equal(calls,1);
    await assert.rejects(p.flash({...base,rebuild:true}),/不能重新编译/);
    await assert.rejects(p.inspect({...base,preset:'Debug'}),/Release/);
    await assert.rejects(p.build(base),/不支持编译/);
    fs.writeFileSync(firmware,'damaged');await assert.rejects(p.flash(base),/固件损坏/);
    fs.writeFileSync(firmware,'fixture');fs.writeFileSync(tool,'damaged');
    await assert.rejects(p.flash(base),/J-Link 文件损坏/);assert.equal(calls,1);
  } finally {
    for(const file of [firmware,tool,path.join(folder,'bundle.json')])fs.unlinkSync(file);
    fs.rmdirSync(path.join(folder,'firmware'));fs.rmdirSync(path.join(folder,'jlink'));fs.rmdirSync(folder);
  }
});
