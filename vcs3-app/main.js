const { app, BrowserWindow, Menu, shell } = require('electron');
const path = require('path');
const fs = require('fs');

// Two editions in one app: the Original (as the 1970 manual) and Advanced (DK VCO, polyphony, MPE, pin amounts).
const EDITIONS = { original: 'original.html', advanced: 'advanced.html' };
const prefsFile = () => path.join(app.getPath('userData'), 'prefs.json');
function loadPrefs() { try { return JSON.parse(fs.readFileSync(prefsFile(), 'utf8')); } catch (e) { return {}; } }
function savePrefs(p) { try { fs.writeFileSync(prefsFile(), JSON.stringify(p)); } catch (e) { /* not fatal */ } }

let win = null;
let edition = 'advanced';

function show(ed) {
  edition = EDITIONS[ed] ? ed : 'advanced';
  savePrefs({ ...loadPrefs(), edition });
  if (win) {
    win.loadFile(path.join(__dirname, 'app', EDITIONS[edition]));
    win.setTitle(edition === 'advanced' ? 'VCS3 Pin Matrix Advanced' : 'VCS3 Pin Matrix');
  }
  buildMenu();
}

function buildMenu() {
  const editionMenu = {
    label: '版',
    submenu: [
      { label: 'オリジナル（1970 年のマニュアルどおり）', type: 'radio', checked: edition === 'original', accelerator: 'CmdOrCtrl+1', click: () => show('original') },
      { label: 'Advanced（DK VCO・ポリフォニー・MPE）', type: 'radio', checked: edition === 'advanced', accelerator: 'CmdOrCtrl+2', click: () => show('advanced') }
    ]
  };
  const template = process.platform === 'darwin'
    ? [{ role: 'appMenu' }, { role: 'editMenu' }, editionMenu, { role: 'windowMenu' }]
    : [editionMenu, { label: '編集', submenu: [{ role: 'copy' }, { role: 'paste' }, { role: 'selectAll' }] }];
  Menu.setApplicationMenu(Menu.buildFromTemplate(template));
}

function createWindow() {
  win = new BrowserWindow({
    width: 1400, height: 960, minWidth: 420, minHeight: 500,
    backgroundColor: '#3f2615',
    webPreferences: { contextIsolation: true, nodeIntegration: false, sandbox: true }
  });
  win.webContents.setWindowOpenHandler(({ url }) => { shell.openExternal(url); return { action: 'deny' }; });
  win.webContents.on('will-navigate', (e, url) => { if (!url.startsWith('file:')) { e.preventDefault(); shell.openExternal(url); } });
  show(loadPrefs().edition || 'advanced');
}

app.commandLine.appendSwitch('autoplay-policy', 'no-user-gesture-required');
app.whenReady().then(() => {
  createWindow();
  app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); });
});
app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit(); });
