const {app, BrowserWindow, ipcMain, dialog, shell, screen} = require('electron');
if (!app.requestSingleInstanceLock()) app.exit(0);
const path = require('node:path');
const fs = require('node:fs');
const {Programmer, findJLink, ROOT, validate} = require('./backend.cjs');
const bundled = app.isPackaged;
const bundleRoot = bundled ? path.join(process.resourcesPath, 'payload') : null;
const programmer = new Programmer({bundleRoot});
const bundledJLink = bundled ? path.join(bundleRoot, 'jlink/JLink.exe') : '';
let win;
app.on('second-instance', () => {if (win && !win.isDestroyed()) {if (win.isMinimized()) win.restore();win.show();win.focus();}});
function log(text) {if (win && !win.isDestroyed()) win.webContents.send('log', text);}
ipcMain.handle('info', () => ({root: bundled ? '软件版本 · ' + programmer.bundle.version : ROOT, jlink: bundledJLink || findJLink(), bundled}));
ipcMain.handle('inspect', async (_, options) => {try {return await programmer.inspect(options);} catch {return null;}});
ipcMain.handle('build', (_, options) => programmer.build(options, log));
ipcMain.handle('flash', (_, options) => programmer.flash(options, log));
ipcMain.handle('choose-tool', async () => {
  if (bundled) throw Error('分发版使用内置 J-Link');
  const result = await dialog.showOpenDialog(win, {title: '选择 JLink.exe', properties: ['openFile'], filters: [{name: 'J-Link Commander', extensions: ['exe']}]});
  return result.canceled ? '' : result.filePaths[0];
});
ipcMain.handle('open-output', async (_, options) => {
  validate(options); const folder = bundled ? path.join(bundleRoot, 'firmware') : path.join(ROOT, 'out', options.preset);
  if (!bundled) fs.mkdirSync(folder, {recursive: true});
  const error = await shell.openPath(folder); if (error) throw Error(error);
});
ipcMain.handle('install-driver', async () => {
  if (programmer.busy) throw Error('请等待当前任务结束');
  const executable = bundledJLink || findJLink();
  if (!executable) throw Error('未找到 J-Link');
  programmer.verifyRuntime();
  const installer = path.join(path.dirname(executable), 'USBDriver/InstDrivers.exe');
  if (!fs.existsSync(installer)) throw Error('未找到驱动安装程序');
  const error = await shell.openPath(installer);
  if (error) throw Error(error);
});
app.whenReady().then(() => {
  const area = screen.getPrimaryDisplay().workAreaSize;
  const scale = Math.min(1, (area.width - 24) / 1040, (area.height - 64) / 780);
  win = new BrowserWindow({width: Math.floor(1040 * scale), height: Math.floor(780 * scale), useContentSize: true,
    resizable: false, maximizable: false, fullscreenable: false,
    show: !process.argv.includes('--smoke-test'), backgroundColor: '#f4f6f9',
    webPreferences: {zoomFactor: scale, preload: path.join(__dirname, 'preload.js'), contextIsolation: true, nodeIntegration: false, sandbox: true}});
  win.removeMenu();
  win.on('close', event => {if (programmer.busy) {event.preventDefault(); log('任务仍在执行，完成后可关闭窗口。\n');}});
  win.webContents.setWindowOpenHandler(() => ({action: 'deny'}));
  win.webContents.on('will-navigate', event => event.preventDefault());
  win.loadFile(path.join(__dirname, 'index.html'));
  if (process.argv.includes('--smoke-test')) {
    win.webContents.once('did-fail-load', () => app.exit(1));
    win.webContents.once('did-finish-load', () => setTimeout(async () => {
      try {
        const ready = await win.webContents.executeJavaScript('document.querySelectorAll("[name=role]").length === 3 && document.getElementById("root").textContent.length > 0');
        if (!ready) throw Error('Renderer not ready');
        if (win.isResizable() || win.isMaximizable()) throw Error('Window must be fixed size');
        if (bundled) for (const role of ['main','screen-new','screen-old']) await programmer.inspect({role,preset:'Release'});
        if (bundled) programmer.verifyRuntime();
        if (process.argv.includes('--preview-success')) await win.webContents.executeJavaScript('showSuccess({role:"screen-new", sha256:"preview", file:"screen-new.hex"})');
        const image = await win.webContents.capturePage();
        const capture = process.env.HTP_SMOKE_OUTPUT || path.join(app.getPath('temp'), 'htp-flasher-preview.png');
        fs.writeFileSync(capture,image.toPNG());
        console.log('PASS: renderer, fixed window, bundled assets; no probe connection');
        app.quit();
      } catch (e) {console.error(e);app.exit(1);}
    }, 2500));
  }
});
app.on('window-all-closed', () => app.quit());
