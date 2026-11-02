const {app, BrowserWindow, ipcMain, dialog, shell} = require('electron');
const path = require('node:path');
const fs = require('node:fs');
const {Programmer, findJLink, ROOT, validate} = require('./backend.cjs');
const programmer = new Programmer();
let win;
function log(text) {if (win && !win.isDestroyed()) win.webContents.send('log', text);}
ipcMain.handle('info', () => ({root: ROOT, jlink: findJLink()}));
ipcMain.handle('inspect', async (_, options) => {try {return await programmer.inspect(options);} catch {return null;}});
ipcMain.handle('build', (_, options) => programmer.build(options, log));
ipcMain.handle('flash', (_, options) => programmer.flash(options, log));
ipcMain.handle('choose-tool', async () => {
  const result = await dialog.showOpenDialog(win, {title: '选择 JLink.exe', properties: ['openFile'], filters: [{name: 'J-Link Commander', extensions: ['exe']}]});
  return result.canceled ? '' : result.filePaths[0];
});
ipcMain.handle('open-output', async (_, options) => {
  validate(options); const folder = path.join(ROOT, 'out', options.preset); fs.mkdirSync(folder, {recursive: true});
  const error = await shell.openPath(folder); if (error) throw Error(error);
});
app.whenReady().then(() => {
  win = new BrowserWindow({width: 1120, height: 860, minWidth: 960, minHeight: 720,
    show: !process.argv.includes('--smoke-test'), backgroundColor: '#f4f6f9',
    webPreferences: {preload: path.join(__dirname, 'preload.js'), contextIsolation: true, nodeIntegration: false, sandbox: true}});
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
        const image = await win.webContents.capturePage();
        fs.mkdirSync(path.join(ROOT,'.cache'),{recursive:true});
        fs.writeFileSync(path.join(ROOT,'.cache/flasher-preview.png'),image.toPNG());
        app.quit();
      } catch (e) {console.error(e);app.exit(1);}
    }, 2500));
  }
});
app.on('window-all-closed', () => app.quit());
