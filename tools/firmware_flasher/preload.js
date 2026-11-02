const {contextBridge, ipcRenderer} = require('electron');
contextBridge.exposeInMainWorld('flasher', {
  info: () => ipcRenderer.invoke('info'), inspect: o => ipcRenderer.invoke('inspect', o),
  build: o => ipcRenderer.invoke('build', o), flash: o => ipcRenderer.invoke('flash', o),
  chooseTool: () => ipcRenderer.invoke('choose-tool'), openOutput: o => ipcRenderer.invoke('open-output', o),
  installDriver: () => ipcRenderer.invoke('install-driver'),
  onLog: fn => ipcRenderer.on('log', (_, text) => fn(text))
});
