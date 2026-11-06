// Regenerates Resources/default_snapshot.json and default_snapshot_fx.json from the Advanced page (run after changing knobs or the first preset):
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
  const fx = await p.evaluate(() => window.vcs3.fxSnapshot()); // AJT JOY16 FX starts from the [FX] preset
  await b.close();
  for (const [name, s] of [['default_snapshot.json', snap], ['default_snapshot_fx.json', fx]]) {
    const out = path.join(__dirname, '..', 'Resources', name);
    fs.writeFileSync(out, JSON.stringify(s, null, 1) + '\n');
    console.log('wrote', out, Object.keys(s.p).length, 'params,', s.pins.s.length, 'pins');
  }
})();
