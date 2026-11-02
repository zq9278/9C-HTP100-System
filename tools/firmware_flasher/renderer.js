const $ = id => document.getElementById(id);
let busy = false, revision = 0, bundled = false, ready = false;
const roleNames = {'main':'主机控制板','screen-new':'新屏幕','screen-old':'旧屏幕'};
function options() {return {role: document.querySelector('[name=role]:checked').value, preset: $('preset').value, jlink: $('jlink').value.trim(), serial: $('serial').value.trim(), speed: $('speed').value, rebuild: $('rebuild').checked};}
function render() {
  for (const control of document.querySelectorAll('button,input,select')) control.disabled = busy || !ready;
  $('flash').textContent = bundled ? '开始烧录' : $('rebuild').checked ? '编译并烧录' : '烧录已生成固件';
  $('progress').hidden = !busy;
  const role = options().role;
  $('target-note').textContent = role === 'main' ? (bundled ? '内置主机 Release 固件，连接主机控制板后烧录。' : '主机固件独立下载；Tuning 版本支持临时压力调参。') : `${role === 'screen-new' ? '新' : '旧'} LCD 驱动，内部 BOOT 与外部屏幕程序合并为一个 HEX。两种屏幕都包含 BOOT。`;
}
function log(text) {const p = $('log');p.textContent = (p.textContent + text).slice(-100000);p.scrollTop = p.scrollHeight;}
function show(info) {$('package').textContent = `${info.file}  ·  ${info.preset}  ·  LCD: ${info.lcd_panel || '不适用'}\n提交 ${info.git_commit.slice(0, 12)}${info.dirty ? '（含本地修改）' : ''}  ·  ${new Date(info.built_at).toLocaleString()}\nSHA-256  ${info.sha256}\n${info.path}`;}
async function refresh() {
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
  if (busy) return; busy = true; ++revision; render(); $('status').className = ''; $('status').textContent = '正在执行…'; $('log').textContent = '';
  try {const info = await fn();if (info) show(info);if (flash) showSuccess(info);else $('status').textContent = '编译完成';}
  catch (error) {$('status').textContent = '操作失败';$('status').className = 'error';$('logs').open = true;log(error.message + '\n');}
  finally {busy = false;render();}
};}
$('build').onclick = action(() => flasher.build(options()));
$('flash').onclick = action(() => flasher.flash(options()), true);
$('driver').onclick = async () => {try {await flasher.installDriver();log('驱动安装程序已打开，请完成安装后再烧录。\n');$('logs').open=true;} catch(e) {log(e.message+'\n');$('logs').open=true;}};
$('browse').onclick = async () => {try {const file = await flasher.chooseTool();if (file) $('jlink').value = file;} catch (e) {log(e.message);}};
$('output').onclick = async () => {try {await flasher.openOutput(options());} catch (e) {log(e.message);}};
for (const radio of document.querySelectorAll('[name=role]')) radio.onchange = refresh;
$('preset').onchange = refresh; $('rebuild').onchange = render;
flasher.onLog(log);
flasher.info().then(info => {
  bundled = info.bundled;ready = true;$('jlink').value = info.jlink;$('root').textContent = info.root;
  if (bundled) {
    $('rebuild').checked = false;$('preset').value = 'Release';
    for (const id of ['rebuild-label','preset-label','tool-row','build']) $(id).hidden = true;
    $('intro').textContent = '已内置固件与 J-Link，选择控制板后即可烧录。';
  }
  return refresh();
}).catch(error => {log(error.message);$('logs').open = true;});
render();
