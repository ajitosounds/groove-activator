// Regenerates Resources/default_snapshot.json from the Advanced page (run after changing knobs or the first preset):
//   node tools/gen_default_snapshot.js
// Needs Playwright with Chromium. The plugin uses this file for its knob names and its state before any session is saved.
const path = require('path');
const fs = require('fs');
const { chromium } = require('playwright');
(async () => {
  const page = path.join(__dirname, '..', '..', 'vcs3-advanced', 'index.html');
  const b = await chromium.launch();
  const p = await b.newPage();
  await p.goto('file://' + page);
  const snap = await p.evaluate(() => window.vcs3.snapshot());
  await b.close();
  const out = path.join(__dirname, '..', 'Resources', 'default_snapshot.json');
  fs.writeFileSync(out, JSON.stringify(snap, null, 1) + '\n');
  console.log('wrote', out, Object.keys(snap.p).length, 'params,', snap.pins.s.length, 'pins');
})();
