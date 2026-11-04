// Copies the simulator pages (the single sources of truth) into the app bundle:
//   ../vcs3/index.html          -> app/original.html
//   ../vcs3-advanced/index.html -> app/advanced.html
const fs = require('fs');
const path = require('path');
const root = path.join(__dirname, '..', '..');
const out = path.join(__dirname, '..', 'app');
fs.mkdirSync(out, { recursive: true });
for (const [from, to] of [['vcs3/index.html', 'original.html'], ['vcs3-advanced/index.html', 'advanced.html']]) {
  fs.copyFileSync(path.join(root, from), path.join(out, to));
  console.log('synced', from, '->', 'app/' + to);
}
