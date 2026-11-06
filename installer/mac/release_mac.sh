#!/bin/bash
# =============================================================================
# AJT JOY16 — Mac release: build, sign, PACE-wrap AAX, package, notarize,
#             and put the .pkg into Dropbox / ファミリー ルーム (Retail + Complimentary)
# =============================================================================
# Run on Gota's Mac, from a clone of ajitosounds/groove-activator:
#     bash installer/mac/release_mac.sh
#
# Uses the same signing setup as Groove Activator (groove-activator-dev/sign_and_package.sh):
#   Developer ID Application / Installer (team M4BPSVA575), notarytool profile "GotaDreamApp",
#   PACE Eden wraptool + iLok for the AAX plug-ins.
# Needs: Xcode command line tools, cmake, node/npm  (brew install cmake node)
#
# Optional environment:
#   SKIP_NOTARIZE=1   build and sign, but do not send to Apple (test run)
#   SKIP_DROPBOX=1    leave the .pkg in the build folder only
#   FAMILY_DIR=...    the local "ファミリー ルーム" folder, if it is not found automatically
#   PACE_WCGUID=...   another PACE wrap configuration (default: the one Groove Activator uses)
# =============================================================================
set -euo pipefail

TEAM_ID="${TEAM_ID:-M4BPSVA575}"
SIGN_APP="${SIGN_APP:-Developer ID Application: gota yashiki ($TEAM_ID)}"
SIGN_PKG="${SIGN_PKG:-Developer ID Installer: gota yashiki ($TEAM_ID)}"
NOTARY_PROFILE="${NOTARY_PROFILE:-GotaDreamApp}"
WRAPTOOL="${WRAPTOOL:-/Applications/PACEAntiPiracy/Eden/Fusion/Versions/5/bin/wraptool}"
PACE_ACCOUNT="${PACE_ACCOUNT:-gota1@me.com}"
PACE_WCGUID="${PACE_WCGUID:-354CA730-431B-11F1-B00E-005056928F3B}"

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)"
[ -f "$REPO/vcs3-plugin/CMakeLists.txt" ] || { echo "ERROR: run this from the groove-activator repository" >&2; exit 1; }
WORK="${WORK:-$HOME/Build/AJT_JOY16_release}"   # outside iCloud (Desktop/Documents xattrs break codesign)
V=$(node -p "require('$REPO/vcs3-app/package.json').version")
DAY=$(TZ=Asia/Tokyo date +%Y%m%d)

step() { echo; echo "=== $* ==="; }
fail() { echo; echo "❌ $*" >&2; exit 1; }

# ---------------------------------------------------------------------------
step "0/8 確認（AJT JOY16 v$V）"
command -v cmake >/dev/null || fail "cmake がありません。ターミナルで: brew install cmake"
command -v npm   >/dev/null || fail "node / npm がありません。ターミナルで: brew install node"
xcode-select -p >/dev/null 2>&1 || fail "Xcode のコマンドラインツールがありません。ターミナルで: xcode-select --install"
IDS=$(security find-identity -v -p basic)
echo "$IDS" | grep -qF "$SIGN_APP" || fail "証明書が見つかりません: $SIGN_APP"
echo "$IDS" | grep -qF "$SIGN_PKG" || fail "証明書が見つかりません: $SIGN_PKG"
echo "  ✓ Developer ID（アプリ用・インストーラー用）"
[ -x "$WRAPTOOL" ] || fail "PACE wraptool が見つかりません: $WRAPTOOL"
WL=$("$WRAPTOOL" list 2>&1 || true)
echo "$WL" | grep -q "License Verified" || { echo "$WL" | sed 's/^/    /'; fail "PACE（Eden Tools）のライセンスが確認できません。iLok を挿して、もう一度。"; }
echo "  ✓ PACE wraptool / iLok"
if [ "${SKIP_NOTARIZE:-0}" != 1 ]; then
  xcrun notarytool history --keychain-profile "$NOTARY_PROFILE" >/dev/null 2>&1 \
    || fail "公証の認証情報（$NOTARY_PROFILE）が使えません。Groove Activator と同じ notarytool store-credentials が要ります。"
  echo "  ✓ 公証（$NOTARY_PROFILE）"
fi
if [ "${SKIP_DROPBOX:-0}" != 1 ]; then
  if [ -z "${FAMILY_DIR:-}" ]; then
    for c in "$HOME"/Library/CloudStorage/Dropbox*/"ファミリー ルーム" "$HOME"/Dropbox*/"ファミリー ルーム"; do
      [ -d "$c" ] && { FAMILY_DIR="$c"; break; }
    done
  fi
  [ -n "${FAMILY_DIR:-}" ] && [ -d "$FAMILY_DIR" ] \
    || fail "Dropbox の「ファミリー ルーム」フォルダが見つかりません。FAMILY_DIR=\"…/ファミリー ルーム\" を付けて実行してください。"
  echo "  ✓ Dropbox: $FAMILY_DIR"
fi
mkdir -p "$WORK"
rm -rf "$WORK/sign" "$WORK/stage" "$WORK/pk" "$WORK/out"

# ---------------------------------------------------------------------------
step "1/8 プラグインのビルド（AU・VST3・AAX、Apple シリコン + Intel）"
cmake -S "$REPO/vcs3-plugin" -B "$WORK/build" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DJOY16_WITH_AAX=ON
cmake --build "$WORK/build" --config Release --parallel "$(sysctl -n hw.ncpu)" \
  --target VCS3PinMatrix_AU VCS3PinMatrix_VST3 VCS3PinMatrix_AAX JOY16FX_AU JOY16FX_VST3 JOY16FX_AAX
A="$WORK/build/VCS3PinMatrix_artefacts/Release"; F="$WORK/build/JOY16FX_artefacts/Release"
BUNDLES=("$A/AU/AJT JOY16.component" "$A/VST3/AJT JOY16.vst3" "$A/AAX/AJT JOY16.aaxplugin"
         "$F/AU/AJT JOY16 FX.component" "$F/VST3/AJT JOY16 FX.vst3" "$F/AAX/AJT JOY16 FX.aaxplugin")
for b in "${BUNDLES[@]}"; do
  [ -d "$b" ] || fail "ビルド結果がありません: $b"
  exe="$b/Contents/MacOS/$(basename "${b%.*}")"
  for arch in arm64 x86_64; do
    m=$(vtool -arch $arch -show-build "$exe" | awk '/minos/{print $2}')
    [ "${m%%.*}" -le 11 ] || fail "$(basename "$b") [$arch] が macOS $m 以上向けになっています"
  done
done
echo "  ✓ 6 個とも macOS 11 以上・両 CPU 対応"

# ---------------------------------------------------------------------------
step "2/8 アプリのビルドと署名（AJT JOY16.app）"
( cd "$REPO/vcs3-app" && npm ci --no-audit --no-fund && npm run sync \
  && CSC_IDENTITY_AUTO_DISCOVERY=true npx electron-builder --mac dir --universal \
       -c.mac.identity="gota yashiki ($TEAM_ID)" -c.mac.hardenedRuntime=true -c.mac.gatekeeperAssess=false )
APP=$(find "$REPO/vcs3-app/dist" -maxdepth 2 -name "AJT JOY16.app" -type d | head -1)
[ -n "$APP" ] || fail "AJT JOY16.app ができていません"
codesign --verify --deep --strict "$APP" || fail "アプリの署名の確認に失敗"
echo "  ✓ $APP"

# ---------------------------------------------------------------------------
step "3/8 プラグインの署名（Developer ID）と AAX の PACE 署名"
ENT="$WORK/entitlements.plist"
cat > "$ENT" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>com.apple.security.cs.disable-library-validation</key><true/>
  <key>com.apple.security.cs.allow-unsigned-executable-memory</key><true/>
  <key>com.apple.security.cs.allow-jit</key><true/>
  <key>com.apple.security.device.audio-input</key><true/>
</dict></plist>
EOF
mkdir -p "$WORK/sign"
for b in "${BUNDLES[@]}"; do
  d="$WORK/sign/$(basename "$b")"
  cp -R "$b" "$d"; xattr -cr "$d"
  codesign --force --deep --sign "$SIGN_APP" --timestamp --options runtime --entitlements "$ENT" "$d"
  if [[ "$d" == *.aaxplugin ]]; then
    "$WRAPTOOL" sign --verbose --account "$PACE_ACCOUNT" --wcguid "$PACE_WCGUID" --signid "$SIGN_APP" \
      --in "$d" --out "$d.pace" || fail "PACE 署名に失敗: $(basename "$d")"
    rm -rf "$d"; mv "$d.pace" "$d"
    "$WRAPTOOL" verify --verbose --in "$d" >/dev/null 2>&1 && echo "  ✓ PACE 確認 $(basename "$d")" || echo "  (wraptool verify は省略)"
  else
    codesign --verify --deep --strict "$d" || fail "署名の確認に失敗: $(basename "$d")"
  fi
  echo "  ✓ $(basename "$d")"
done

# ---------------------------------------------------------------------------
step "4/8 インストーラー（.pkg）を作る"
S="$WORK/stage"; P="$WORK/pk"
mkdir -p "$S/app/Applications" "$S/au/Library/Audio/Plug-Ins/Components" "$S/vst3/Library/Audio/Plug-Ins/VST3" \
         "$S/aax/Library/Application Support/Avid/Audio/Plug-Ins" "$P"
ditto "$APP" "$S/app/Applications/AJT JOY16.app"
for x in component vst3 aaxplugin; do
  case $x in component) to="$S/au/Library/Audio/Plug-Ins/Components";; vst3) to="$S/vst3/Library/Audio/Plug-Ins/VST3";;
              aaxplugin) to="$S/aax/Library/Application Support/Avid/Audio/Plug-Ins";; esac
  for d in "$WORK/sign"/*."$x"; do ditto "$d" "$to/$(basename "$d")"; done
done
for c in app au vst3 aax; do
  pkgbuild --analyze --root "$S/$c" "$P/$c.plist"
  i=0
  while /usr/libexec/PlistBuddy -c "Print :$i" "$P/$c.plist" >/dev/null 2>&1; do
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$P/$c.plist" 2>/dev/null \
      || /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$P/$c.plist"
    i=$((i+1))
  done
  pkgbuild --root "$S/$c" --component-plist "$P/$c.plist" --identifier "com.ajitosounds.joy16.$c" \
    --version "$V" --install-location / "$P/$c.pkg"
done
sed "s/VERSION/$V/g" "$REPO/installer/mac/distribution.xml" > "$P/distribution.xml"
productbuild --distribution "$P/distribution.xml" --resources "$REPO/installer/mac/resources" --package-path "$P" "$P/unsigned.pkg"
mkdir -p "$WORK/out"
PKG="$WORK/out/AJITO_SOUNDS_AJT_JOY16_Mac_v${V}_${DAY}.pkg"
productsign --sign "$SIGN_PKG" --timestamp "$P/unsigned.pkg" "$PKG"
pkgutil --check-signature "$PKG" | head -3
echo "  ✓ $PKG"

# ---------------------------------------------------------------------------
if [ "${SKIP_NOTARIZE:-0}" != 1 ]; then
  step "5/8 Apple の公証（数分かかります）"
  xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait | tee "$WORK/notary.log"
  grep -q "status: Accepted" "$WORK/notary.log" || fail "公証が通りませんでした（上の id で xcrun notarytool log <id> --keychain-profile $NOTARY_PROFILE）"
  step "6/8 公証の印を付ける（staple）"
  xcrun stapler staple "$PKG"
  spctl -a -vv -t install "$PKG" 2>&1 | head -3
else
  echo; echo "（SKIP_NOTARIZE=1: 公証は省略）"
fi

# ---------------------------------------------------------------------------
if [ "${SKIP_DROPBOX:-0}" = 1 ]; then
  echo; echo "✅ できました（Dropbox には入れていません）: $PKG"; exit 0
fi
step "7/8 Dropbox / ファミリー ルームの Retail と Complimentary に入れる"
for KIND in Retail Complimentary; do
  D=$(ls -d "$FAMILY_DIR"/AJITO_SOUNDS_AJT_JOY16_v${V}_${KIND}_* 2>/dev/null | sort | tail -1 || true)
  [ -n "$D" ] || { D="$FAMILY_DIR/AJITO_SOUNDS_AJT_JOY16_v${V}_${KIND}_${DAY}"; mkdir -p "$D"; }
  rm -f "$D"/AJITO_SOUNDS_AJT_JOY16_Mac_*.pkg
  cp "$PKG" "$D/AJITO_SOUNDS_AJT_JOY16_Mac_v${V}_${DAY}_${KIND}.pkg"
  if [ "$KIND" = Retail ]; then JA="お求めいただき、ありがとうございます。"; EN="Thank you for your purchase."
  else JA="AJITO SOUNDS からの贈呈版です。どうぞお楽しみください。"; EN="This is a complimentary copy from AJITO SOUNDS. Enjoy."; fi
  sed -e "s/VERSION/${V}/" -e "s/__GREETING__/${JA}/" "$REPO/installer/dist/README_ja.txt" > "$D/README_はじめに.txt"
  sed -e "s/VERSION/${V}/" -e "s/__GREETING__/${EN}/" "$REPO/installer/dist/README_en.txt" > "$D/README_EN.txt"
  ( cd "$D" && shasum -a 256 *.pkg *.exe 2>/dev/null > SHA256.txt )
  {
    echo "AJT JOY16 v${V}  ${KIND}  $(TZ=Asia/Tokyo date +%F)"
    echo "source: groove-activator@$(git -C "$REPO" rev-parse --short HEAD) ($(git -C "$REPO" rev-parse --abbrev-ref HEAD))"
    echo "Mac: built on $(scutil --get ComputerName 2>/dev/null || hostname); AU/VST3/AAX (instrument Jy16 + FX Jy1F) + app;"
    echo "     Developer ID ($TEAM_ID), AAX PACE-wrapped (wcguid $PACE_WCGUID), $( [ "${SKIP_NOTARIZE:-0}" = 1 ] && echo 'NOT notarized' || echo 'notarized + stapled')"
    echo "Windows: from the GitHub build (VST3 + app; AAX not yet)"
    echo "serial: none (Retail and Complimentary installers are identical)"
  } > "$D/BUILT_FROM.txt"
  ls "$D" | grep -q "_Win_" || echo "  ⚠ $(basename "$D") に Windows 版がまだありません（GitHub のビルドが入れます）"
  echo "  ✓ $(basename "$D")"
done

step "8/8 完了"
echo "✅ AJT JOY16 v$V（Mac: AU・VST3・AAX・アプリ）を Dropbox のファミリー ルームに入れました。"
echo "   Dropbox の同期が終われば、他の人からも見えます。"
