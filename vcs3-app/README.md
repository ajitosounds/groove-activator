# VCS3 Pin Matrix — desktop app

Wraps the simulator page `../vcs3/index.html` in Electron and builds installers.
`npm run sync` copies that page into `app/`, so edit the page there, not here.

| Command | Output |
| --- | --- |
| `npm start` | Run the app locally |
| `npm run dist:win` | `dist/VCS3-Pin-Matrix-Setup-<version>.exe` (on Linux this needs wine) |
| `npm run dist:mac` | `dist/VCS3-Pin-Matrix-<version>.dmg` (macOS only; universal, ad-hoc signed) |
| `npm run dist:linux` | `dist/VCS3-Pin-Matrix-<version>.AppImage` |

The GitHub Actions workflow `.github/workflows/vcs3-app.yml` builds the macOS and
Windows installers on every push that touches `vcs3/` or `vcs3-app/`, and attaches
them to the run as artifacts.

The installers are not signed with an Apple Developer ID or a Windows code-signing
certificate, so macOS Gatekeeper and Windows SmartScreen warn on first launch.
Bump `version` in `package.json` for each release.
