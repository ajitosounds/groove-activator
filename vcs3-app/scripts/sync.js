// Copies the simulator page (vcs3/index.html, the single source of truth) into the app bundle.
const fs = require('fs');
const path = require('path');
const src = path.join(__dirname, '..', '..', 'vcs3', 'index.html');
const dst = path.join(__dirname, '..', 'app', 'index.html');
fs.mkdirSync(path.dirname(dst), { recursive: true });
fs.copyFileSync(src, dst);
console.log('synced', path.relative(process.cwd(), src), '->', path.relative(process.cwd(), dst));
