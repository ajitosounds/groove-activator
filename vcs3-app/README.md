# AJT JOY16 — desktop app

Wraps both simulator pages in Electron and builds installers: the Original
(`../vcs3/index.html`) and Advanced (`../vcs3-advanced/index.html`). The 版 menu
(Cmd/Ctrl+1, Cmd/Ctrl+2) switches between them and the app remembers the last one.
`npm run sync` copies the pages into `app/`, so edit them there, not here.

| Command | Output |
| --- | --- |
| `npm start` | Run the app locally |
| `npm run dist:win` | `dist/VCS3-Pin-Matrix-Setup-<version>.exe` (on Linux this needs wine) |
| `npm run dist:mac` | `dist/VCS3-Pin-Matrix-<version>.dmg` (macOS only; universal, ad-hoc signed) |
| `npm run dist:linux` | `dist/VCS3-Pin-Matrix-<version>.AppImage` |

The GitHub Actions workflow `.github/workflows/vcs3-build.yml` builds the macOS and Windows installers
and plugins, puts them in one folder (artifact `AJT-JOY16`) and uploads it to Dropbox when the
Dropbox secrets are set.

The installers are not signed with an Apple Developer ID or a Windows code-signing
certificate, so macOS Gatekeeper and Windows SmartScreen warn on first launch.
Bump `version` in `package.json` for each release.
