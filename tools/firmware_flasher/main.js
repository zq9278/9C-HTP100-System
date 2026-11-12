const {app, BrowserWindow, ipcMain, dialog, shell, screen} = require('electron');
if (!app.requestSingleInstanceLock()) app.exit(0);
const path = require('node:path');
const fs = require('node:fs');
const preferences = require('./preferences.cjs');
const preferencesFile = () => path.join(app.getPath('userData'), 'programmer-settings.json');
const {Programmer, findJLink, ROOT, validate} = require('./backend.cjs');
const bundled = app.isPackaged;
const bundleRoot = bundled ? path.join(process.resourcesPath, 'payload') : null;
const programmer = new Programmer({bundleRoot});
const bundledJLink = bundled ? path.join(bundleRoot, 'jlink/JLink.exe') : '';
let win;
const {Rtt}=require('./rtt.cjs');
const rtt=new Rtt((event,data)=>{if(win&&!win.isDestroyed())win.webContents.send('rtt-event',event,data);});
function ensureNoRtt(){if(rtt.child||rtt.busy)throw Error('Disconnect RTT before programming');}
ipcMain.handle('rtt',(_,method,args=[])=>rtt.exclusive(async()=>{
  if(programmer.busy)throw Error('Programmer busy');
  const opts=preferences.read(preferencesFile());
  const executable=bundledJLink||opts.jlink||findJLink();
  if(method==='open'||method==='back') {
    if(rtt.child)throw Error('Disconnect RTT first');
    await win.loadFile(path.join(__dirname,method==='open'?'rtt.html':'index.html'));return true;
  }
  if(method==='flash') {
    if(rtt.child)throw Error('Disconnect RTT first');
    const info=await programmer.flash({role:'main',preset:'Tuning',rebuild:!bundled,jlink:executable,serial:opts.serial||'',speed:opts.speed||'1000'},t=>rtt.notify('log',t));return info;
  }
  if(method==='connect') {
    if(rtt.child)throw Error('Already connected');
    if(!executable)throw Error('J-Link not found');
    programmer.verifyRuntime();
    const env={...process.env,JLINK_DLL:path.join(path.dirname(executable),'JLink_x64.dll')};
    if(bundled)rtt.start(path.join(bundleRoot,'htp-rtt-bridge.exe'),[],env);
    else rtt.start(process.env.PYTHON||'python',['-u',path.join(ROOT,'tools/pressure_tuner/rtt_bridge.py')],env);
    try{const info=await rtt.call('connect',{serial:opts.serial||'',speed:opts.speed||1000});rtt.connected=true;return info;}
    catch(e){await rtt.close();throw e;}
  }
  if(method==='disconnect'){await rtt.close();return true;}
  if(!rtt.connected)throw Error('Connect RTT first');
  if(!['read','write'].includes(method))throw Error('Invalid RTT action');
  if(!Array.isArray(args)||!Number.isInteger(args[0])||args[0]<0||args[0]>4)throw Error('Invalid profile');
  if(method==='write'){
    const v=args[1];if(!Array.isArray(v)||v.length!==4||v.some(n=>!Number.isFinite(n))||v[0]<=0||v[0]>65535||v[1]<=0||v[1]>65535||v[2]<=0||v[2]>1000||v[3]<.01||v[3]>1)throw Error('Parameter out of range');
  }
  return rtt.call(method,...args);
}));
app.on('second-instance', () => {if (win && !win.isDestroyed()) {if (win.isMinimized()) win.restore();win.show();win.focus();}});
function log(text) {if (win && !win.isDestroyed()) win.webContents.send('log', text);}
ipcMain.handle('info', () => ({root: bundled ? '软件版本 · ' + programmer.bundle.version : ROOT, jlink: bundledJLink || findJLink(), bundled, preferences: preferences.read(preferencesFile())}));
ipcMain.handle('save-preferences', (_, value) => preferences.save(preferencesFile(), value));
ipcMain.handle('inspect', async (_, options) => {try {return await programmer.inspect(options);} catch {return null;}});
ipcMain.handle('build', (_, options) => {ensureNoRtt();return programmer.build(options, log);});
ipcMain.handle('flash', (_, options) => {ensureNoRtt();return programmer.flash(options, log, step => {if (win && !win.isDestroyed()) win.webContents.send('progress', step);});});
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
  win.on('close', event => {if(rtt.busy){event.preventDefault();return;}if(rtt.child){event.preventDefault();rtt.exclusive(()=>rtt.close()).catch(()=>{}).finally(()=>{rtt.child=null;win.close();});return;}if (programmer.busy) {event.preventDefault(); log('任务仍在执行，完成后可关闭窗口。\n');}});
  win.webContents.setWindowOpenHandler(() => ({action: 'deny'}));
  win.webContents.on('will-navigate', event => event.preventDefault());
  win.loadFile(path.join(__dirname, 'index.html'));
  if (process.argv.includes('--smoke-test')) {
    win.webContents.once('did-fail-load', () => app.exit(1));
    win.webContents.once('did-finish-load', () => setTimeout(async () => {
      try {
        const ready = await win.webContents.executeJavaScript('document.querySelectorAll("[name=role]").length === 2 && document.querySelectorAll("[name=panel]").length === 3 && document.getElementById("root").textContent.length > 0');
        if (!ready) throw Error('Renderer not ready');
        const nested = await win.webContents.executeJavaScript(`(async () => {
          const main = document.querySelector('[name=role][value=main]');
          const screen = document.querySelector('[name=role][value=screen]');
          const old = document.querySelector('[name=panel][value=screen-old]');
          const fresh = document.querySelector('[name=panel][value=screen-new]');
          const nov11 = document.querySelector('[name=panel][value=screen-nov11]');
          if (!document.getElementById('screen-variants').hidden) return false;
          screen.checked = true; await screen.onchange();
          if (document.getElementById('screen-variants').hidden || options().role !== 'screen-new') return false;
          old.checked = true; await old.onchange();
          if (options().role !== 'screen-old') return false;
          nov11.checked = true; await nov11.onchange();
          if (options().role !== 'screen-nov11') return false;
          main.checked = true; await main.onchange();
          if (!document.getElementById('screen-variants').hidden || options().role !== 'main') return false;
          screen.checked = true; fresh.checked = true; await screen.onchange();
          return options().role === 'screen-new';
        })()`);
        if (!nested) throw Error('Nested target selection failed');
        if (process.argv.includes('--preview-progress')) {
          await win.webContents.executeJavaScript('busy = true; flashActive = true; ++revision; render(); flashProgress(3)');
        }
        if (win.isResizable() || win.isMaximizable()) throw Error('Window must be fixed size');
        if (bundled) for (const role of ['main','screen-new','screen-old','screen-nov11']) await programmer.inspect({role,preset:'Release'});
        if (bundled) programmer.verifyRuntime();
        if (process.argv.includes('--preview-long-log')) {
          const fits = await win.webContents.executeJavaScript(`(async () => {
            const section = document.getElementById('logs').closest('section');
            const before = section.getBoundingClientRect().width;
            document.getElementById('log').textContent = 'J-Link diagnostic: ' + '0123456789ABCDEF'.repeat(500) + '\\n' + ('C:/very-long-firmware-path/'.repeat(120));
            document.getElementById('logs').open = true;
            await new Promise(resolve => requestAnimationFrame(resolve));
            const log = document.getElementById('log');
            return section.getBoundingClientRect().width <= before + 1 &&
              document.documentElement.scrollWidth <= document.documentElement.clientWidth + 1 &&
              log.scrollWidth <= log.clientWidth + 1;
          })()`);
          if (!fits) throw Error('Long log expanded the page or failed to wrap');
          console.log('PASS: expanded long log wraps without widening the page');
        }
        if (process.argv.includes('--preview-success')) await win.webContents.executeJavaScript('showSuccess({role:"screen-new", sha256:"preview", file:"screen-new.hex"})');
        await new Promise(resolve => setTimeout(resolve, 250));
        if (process.argv.includes('--preview-rtt')) {
          await win.loadFile(path.join(__dirname,'rtt.html'));
          const readyRtt=await win.webContents.executeJavaScript("document.getElementById('profile').options.length===5 && document.getElementById('write').disabled && document.documentElement.scrollWidth<=document.documentElement.clientWidth");
          if(!readyRtt)throw Error('RTT page failed');
          console.log('PASS: RTT page, five profiles, safe disconnected controls');
        }
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
