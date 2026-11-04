const {contextBridge, ipcRenderer} = require('electron');
contextBridge.exposeInMainWorld('flasher', {
  rtt: (method,args) => ipcRenderer.invoke('rtt',method,args),
  onRtt: fn => ipcRenderer.on('rtt-event',(_,event,data)=>fn(event,data)),
  info: () => ipcRenderer.invoke('info'), inspect: o => ipcRenderer.invoke('inspect', o),
  build: o => ipcRenderer.invoke('build', o), flash: o => ipcRenderer.invoke('flash', o),
  chooseTool: () => ipcRenderer.invoke('choose-tool'), openOutput: o => ipcRenderer.invoke('open-output', o),
  installDriver: () => ipcRenderer.invoke('install-driver'),
  savePreferences: value => ipcRenderer.invoke('save-preferences', value),
  onProgress: fn => ipcRenderer.on('progress', (_, step) => fn(step)),
  onLog: fn => ipcRenderer.on('log', (_, text) => fn(text))
});
