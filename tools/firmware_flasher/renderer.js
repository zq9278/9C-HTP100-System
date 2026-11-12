const $ = id => document.getElementById(id);
let busy = false, revision = 0, bundled = false, ready = false;
let flashActive = false;
function flashProgress(step) {
  $('flash-progress').hidden = false;
  $('flash-bar').value = step;
  $('flash-progress-text').textContent = ['','1/4 · 准备固件','2/4 · 连接设备','3/4 · 下载及校验中，请勿断开连接','4/4 · 烧录完成'][step];
}
const roleNames = {'main':'主机控制板','screen-new':'新屏幕','screen-old':'旧屏幕','screen-nov11':'11月11日更新屏幕程序'};
function savePreferences() {
  if (!ready) return;
  const value = {...options(), target: document.querySelector('[name=role]:checked').value,
    panel: document.querySelector('[name=panel]:checked').value};
  return flasher.savePreferences(value).catch(error => log('设置保存失败: ' + error.message + '\n'));
}
function restorePreferences(p = {}) {
  for (const radio of document.querySelectorAll('[name=role]')) if (radio.value === p.target) radio.checked = true;
  for (const radio of document.querySelectorAll('[name=panel]')) if (radio.value === p.panel) radio.checked = true;
  if (['1000','4000','8000'].includes(p.speed)) $('speed').value = p.speed;
  if (typeof p.serial === 'string') $('serial').value = p.serial;
  if (!bundled) {
    if (p.preset) $('preset').value = p.preset;
    if (typeof p.rebuild === 'boolean') $('rebuild').checked = p.rebuild;
    if (p.jlink) $('jlink').value = p.jlink;
  }
}
function options() {const target = document.querySelector('[name=role]:checked').value;return {role: target === 'screen' ? document.querySelector('[name=panel]:checked').value : 'main', preset: $('preset').value, jlink: $('jlink').value.trim(), serial: $('serial').value.trim(), speed: $('speed').value, rebuild: $('rebuild').checked};}
function render() {
  for (const control of document.querySelectorAll('button,input,select')) control.disabled = busy || !ready;
  $('flash').textContent = bundled ? '开始烧录' : $('rebuild').checked ? '编译并烧录' : '烧录已生成固件';
  $('progress').hidden = !busy || flashActive;
  const role = options().role;
  $('screen-variants').hidden = role === 'main';
  $('target-note').textContent = role === 'main' ? (bundled ? '内置主机 Release 固件，连接主机控制板后烧录。' : '主机固件独立下载；Tuning 版本支持临时压力调参。') : `${roleNames[role]} LCD 驱动，内部 BOOT 与外部屏幕程序合并为一个 HEX。三种屏幕都包含 BOOT。`;
}
function log(text) {const p = $('log');p.textContent = (p.textContent + text).slice(-100000);p.scrollTop = p.scrollHeight;}
function show(info) {$('package').textContent = `${info.file}  ·  ${info.preset}  ·  LCD: ${info.lcd_panel || '不适用'}\n提交 ${info.git_commit.slice(0, 12)}${info.dirty ? '（含本地修改）' : ''}  ·  ${new Date(info.built_at).toLocaleString()}\nSHA-256  ${info.sha256}\n${info.path}`;}
async function refresh() {
  if (!busy) $('flash-progress').hidden = true;
  const request = ++revision; render();
  if (!busy) {$('status').className = '';$('status').textContent = '就绪';}
  try {const info = await flasher.inspect(options());if (!info) throw Error('No valid package');if (request === revision) show(info);}
  catch {if (request === revision) $('package').textContent = bundled ? '内置固件不可用，请重新解压完整软件包。' : '当前对象没有有效固件，请先编译生成。';}
}
function showSuccess(info) {
  $('status').className = 'success';$('status').textContent = '✓ 烧录成功 · ' + roleNames[info.role];
  $('success-detail').textContent = roleNames[info.role] + ' · ' + info.file + '\n' + new Date().toLocaleString();
  $('success-dialog').showModal();
}
$('success-close').onclick = () => $('success-dialog').close();
function action(fn, flash = false) {return async () => {
  if (busy) return; savePreferences(); busy = true; flashActive = flash; ++revision; render(); $('status').className = ''; $('status').textContent = '正在执行…'; $('log').textContent = '';
  $('flash-progress').className = ''; $('flash-progress').hidden = !flash;
  if (flash) flashProgress(1);
  try {const info = await fn();if (info) show(info);if (flash) {flashProgress(4);showSuccess(info);}else $('status').textContent = '编译完成';}
  catch (error) {$('status').textContent = '操作失败';$('status').className = 'error';$('logs').open = true;log(error.message + '\n');if (flash) {$('flash-progress').className = 'failed';$('flash-progress-text').textContent += ' · 失败，未完成烧录';}}
  finally {busy = false;flashActive = false;render();}
};}
$('build').onclick = action(() => flasher.build(options()));
$('flash').onclick = action(() => flasher.flash(options()), true);
$('driver').onclick = async () => {try {await flasher.installDriver();log('驱动安装程序已打开，请完成安装后再烧录。\n');$('logs').open=true;} catch(e) {log(e.message+'\n');$('logs').open=true;}};
$('browse').onclick = async () => {try {const file = await flasher.chooseTool();if (file) {$('jlink').value = file;savePreferences();}} catch (e) {log(e.message);}};
$('output').onclick = async () => {try {await flasher.openOutput(options());} catch (e) {log(e.message);}};
for (const radio of document.querySelectorAll('[name=role]')) radio.onchange = () => {savePreferences();return refresh();};
for (const radio of document.querySelectorAll('[name=panel]')) radio.onchange = () => {savePreferences();return refresh();};
$('preset').onchange = () => {savePreferences();return refresh();};
$('rebuild').onchange = () => {savePreferences();render();};
$('speed').onchange = savePreferences;
$('serial').oninput = savePreferences;
$('jlink').oninput = savePreferences;
flasher.onLog(log);
flasher.onProgress(step => {if (busy && flashActive && Number.isInteger(step) && step >= 1 && step <= 3) flashProgress(step);});
flasher.info().then(info => {
  bundled = info.bundled;ready = true;$('jlink').value = info.jlink;$('root').textContent = info.root;
  restorePreferences(info.preferences);
  if (bundled) {
    $('rebuild').checked = false;$('preset').value = 'Release';
    for (const id of ['rebuild-label','preset-label','tool-row','build']) $(id).hidden = true;
    $('intro').textContent = '已内置固件与 J-Link，选择控制板后即可烧录。';
  }
  return refresh();
}).catch(error => {log(error.message);$('logs').open = true;});
render();

$('rtt-open').onclick = async () => {try {await savePreferences(); await flasher.rtt('open');} catch(e) {log(e.message+'\n');}};
