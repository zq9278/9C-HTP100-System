const $ = id => document.getElementById(id);
let busy = false, revision = 0;
function options() {return {role: document.querySelector('[name=role]:checked').value, preset: $('preset').value, jlink: $('jlink').value.trim(), serial: $('serial').value.trim(), speed: $('speed').value, rebuild: $('rebuild').checked};}
function render() {
  for (const control of document.querySelectorAll('button,input,select')) control.disabled = busy;
  $('flash').textContent = $('rebuild').checked ? '编译并烧录' : '烧录已生成固件';
  $('progress').hidden = !busy;
  const role = options().role;
  $('target-note').textContent = role === 'main' ? '主机固件独立下载；Tuning 版本支持临时压力调参。' : `${role === 'screen-new' ? '新' : '旧'} LCD 驱动，内部 BOOT 与外部屏幕程序合并为一个 HEX。两种屏幕都包含 BOOT。`;
}
function log(text) {const p = $('log');p.textContent = (p.textContent + text).slice(-100000);p.scrollTop = p.scrollHeight;}
function show(info) {$('package').textContent = `${info.file}  ·  ${info.preset}  ·  LCD: ${info.lcd_panel || '不适用'}\n提交 ${info.git_commit.slice(0, 12)}${info.dirty ? '（含本地修改）' : ''}  ·  ${new Date(info.built_at).toLocaleString()}\nSHA-256  ${info.sha256}\n${info.path}`;}
async function refresh() {
  const request = ++revision; render();
  try {const info = await flasher.inspect(options());if (!info) throw Error('No valid package');if (request === revision) show(info);}
  catch {if (request === revision) $('package').textContent = '当前对象没有有效固件，请先编译生成。';}
}
function action(fn) {return async () => {
  if (busy) return; busy = true; ++revision; render(); $('status').className = ''; $('status').textContent = '正在执行…'; $('log').textContent = '';
  try {const info = await fn();if (info) show(info);$('status').textContent = '已完成';}
  catch (error) {$('status').textContent = '操作失败';$('status').className = 'error';$('logs').open = true;log(error.message + '\n');}
  finally {busy = false;render();}
};}
$('build').onclick = action(() => flasher.build(options()));
$('flash').onclick = action(() => flasher.flash(options()));
$('browse').onclick = async () => {try {const file = await flasher.chooseTool();if (file) $('jlink').value = file;} catch (e) {log(e.message);}};
$('output').onclick = async () => {try {await flasher.openOutput(options());} catch (e) {log(e.message);}};
for (const radio of document.querySelectorAll('[name=role]')) radio.onchange = refresh;
$('preset').onchange = refresh; $('rebuild').onchange = render;
flasher.onLog(log);
flasher.info().then(info => {$('jlink').value = info.jlink;$('root').textContent = info.root;return refresh();}).catch(error => log(error.message));
render();
