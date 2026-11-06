# =============================================================================
# AJT JOY16 - Windows release: build VST3 + AAX + app, PACE-sign the AAX,
#             bake the installer, Certum-sign it, and put it into
#             Dropbox / ファミリー ルーム (Retail + Complimentary)
# =============================================================================
# Run on the Windows PC that bakes Groove Activator (iLok inserted,
# SimplySign Desktop logged in), from a clone of ajitosounds/groove-activator:
#     installer\windows\RELEASE_WINDOWS.bat
# or
#     powershell -ExecutionPolicy Bypass -File installer\windows\release_windows.ps1
#
# Same signing setup as Groove Activator (groove-activator-dev/scripts/sign_aax.ps1,
# bake_windows.ps1, build_installer.ps1):
#   AAX : wraptool sign --account gota1@me.com --wcguid (same as GA)
#         --signid <self-signed cert> --extrasigningoptions digest_sha256, --in = --out
#   .exe: signtool /sha1 <Certum thumbprint> /fd sha256 /td sha256 /tr http://time.certum.pl
#
# Needs: Visual Studio 2022 (Desktop development with C++), Inno Setup 6,
#        Node.js, Git, PACE AAX Code Signing Tools (wraptool), Windows SDK (signtool)
#
# Options:
#   -SignThumbprint <thumbprint>  the Certum certificate (found automatically if there is one)
#   -NoSign                       do not Certum-sign the installer (test run)
#   -SkipDropbox                  leave the installer in the build folder only
#   -FamilyDir <folder>           the local "ファミリー ルーム" folder, if not found automatically
#   -Wcguid <guid>                another PACE wrap configuration
#   -NoPull                       do not git pull
# =============================================================================
param(
    [string]$SignThumbprint = "",
    [switch]$NoSign,
    [switch]$SkipDropbox,
    [string]$FamilyDir = "",
    [string]$Account   = "gota1@me.com",
    [string]$Wcguid    = "354CA730-431B-11F1-B00E-005056928F3B",
    [string]$Work      = "C:\JOY16_build",
    [switch]$NoPull
)
$ErrorActionPreference = "Stop"
function P([string[]]$parts) { [IO.Path]::Combine($parts) }
function Step($s) { ""; "=== $s ===" }
function Die($s)  { ""; "X $s"; exit 1 }
function Run($exe, [string[]]$a) {
    # prints the tool's output, returns its exit code.
    # Native tools write progress to stderr; under Stop in PowerShell 5.1 that would abort.
    $ErrorActionPreference = "Continue"
    & $exe @a 2>&1 | ForEach-Object { "$_" } | Out-Host
    $rc = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    return $rc
}
function Cap($exe, [string[]]$a) {
    # returns the tool's output lines; the exit code is left in $script:LastRc
    $ErrorActionPreference = "Continue"
    $o = @(& $exe @a 2>&1 | ForEach-Object { "$_" })
    $script:LastRc = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    return ,$o
}
$t0   = Get-Date
$repo = (Resolve-Path (P @((Split-Path -Parent $MyInvocation.MyCommand.Path), "..", ".."))).Path
if (-not (Test-Path (P @($repo, "vcs3-plugin", "CMakeLists.txt")))) { Die "run this from the groove-activator repository" }
$jst  = [TimeZoneInfo]::ConvertTimeBySystemTimeZoneId((Get-Date), "Tokyo Standard Time")
$DAY  = $jst.ToString("yyyyMMdd")
$utf8 = New-Object System.Text.UTF8Encoding($false)

# -----------------------------------------------------------------------------
Step "0/8 確認"
foreach ($c in @("git", "node", "npm")) {
    if (-not (Get-Command $c -ErrorAction SilentlyContinue)) { Die "$c が見つかりません" }
}
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { Die "Visual Studio 2022 が見つかりません" }
$vsPath = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { Die "Visual Studio に「C++ によるデスクトップ開発」が入っていません" }
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $cmake = Get-ChildItem (P @($vsPath, "Common7", "IDE", "CommonExtensions", "Microsoft", "CMake")) -Recurse -Filter cmake.exe -ErrorAction SilentlyContinue |
             Select-Object -First 1 -ExpandProperty FullName
}
if (-not $cmake) { Die "cmake が見つかりません（VS Installer で「C++ CMake tools for Windows」を入れる）" }
$iscc = Get-ChildItem "${env:ProgramFiles(x86)}\Inno Setup *\ISCC.exe", "$env:ProgramFiles\Inno Setup *\ISCC.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
if (-not $iscc) { Die "Inno Setup 6 が見つかりません" }
$wrap = Get-ChildItem "$env:ProgramFiles\PACEAntiPiracy", "${env:ProgramFiles(x86)}\PACEAntiPiracy" -Recurse -Filter wraptool.exe -ErrorAction SilentlyContinue |
        Sort-Object FullName | Select-Object -Last 1 -ExpandProperty FullName
if (-not $wrap) { Die "wraptool.exe が見つかりません（PACE AAX Code Signing Tools）" }
$st = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
      Where-Object { $_.FullName -match "\\x64\\" } | Sort-Object FullName | Select-Object -Last 1 -ExpandProperty FullName
if (-not $st) { Die "signtool.exe (x64) が見つかりません（Windows SDK）" }

# Certum certificate for the installer
$certum = $null
if (-not $NoSign) {
    if ($SignThumbprint -ne "") {
        $certum = Get-ChildItem Cert:\CurrentUser\My, Cert:\LocalMachine\My -ErrorAction SilentlyContinue |
                  Where-Object { $_.Thumbprint -eq $SignThumbprint } | Select-Object -First 1
        if (-not $certum) { Die "拇印 $SignThumbprint の証明書がありません（SimplySign Desktop にログインしていますか）" }
    } else {
        $found = @(Get-ChildItem Cert:\CurrentUser\My, Cert:\LocalMachine\My -CodeSigningCert -ErrorAction SilentlyContinue |
                   Where-Object { $_.Issuer -match "Certum" -and $_.NotAfter -gt (Get-Date) })
        if ($found.Count -eq 0) { Die "Certum の証明書が見つかりません（SimplySign Desktop にログインしてから、もう一度。拇印は -SignThumbprint でも渡せます）" }
        if ($found.Count -gt 1) {
            $found | ForEach-Object { "    $($_.Thumbprint)  $($_.Subject)  〜$($_.NotAfter.ToString('yyyy-MM-dd'))" }
            Die "Certum の証明書が複数あります。-SignThumbprint <拇印> で選んでください"
        }
        $certum = $found[0]
    }
    $SignThumbprint = $certum.Thumbprint
}

# PACE: iLok + licence
$lst = (Cap $wrap @("list")) -join "`n"
if ($lst -notmatch "License Verified") { $lst; Die "wraptool のライセンスが確認できません（iLok は挿さっていますか）" }

# Dropbox family room
if (-not $SkipDropbox) {
    if ($FamilyDir -eq "") {
        $cands = @()
        foreach ($root in @(Get-ChildItem $env:USERPROFILE -Directory -Filter "Dropbox*" -ErrorAction SilentlyContinue)) {
            $cands += @(Get-ChildItem $root.FullName -Directory -Filter "ファミリー ルーム" -ErrorAction SilentlyContinue)
            foreach ($sub in @(Get-ChildItem $root.FullName -Directory -ErrorAction SilentlyContinue)) {
                $cands += @(Get-ChildItem $sub.FullName -Directory -Filter "ファミリー ルーム" -ErrorAction SilentlyContinue)
            }
        }
        if ($cands.Count -gt 0) { $FamilyDir = $cands[0].FullName }
    }
    if ($FamilyDir -eq "" -or -not (Test-Path $FamilyDir)) {
        Die "Dropbox の「ファミリー ルーム」が見つかりません。-FamilyDir <フォルダ> で渡すか、-SkipDropbox で試してください"
    }
}

"  o cmake    : $cmake"
"  o ISCC     : $iscc"
"  o wraptool : $wrap   (License Verified)"
"  o signtool : $st"
if ($certum) { "  o Certum   : $($certum.Subject)  $SignThumbprint" } else { "  ! 署名しない（-NoSign）" }
if (-not $SkipDropbox) { "  o Dropbox  : $FamilyDir" }

# -----------------------------------------------------------------------------
Step "1/8 ソース"
Set-Location $repo
if (-not $NoPull) {
    if ((Run "git" @("-C", $repo, "pull", "--ff-only")) -ne 0) { Die "git pull に失敗（git -C $repo status で確認）" }
}
$dirty = (Cap "git" @("-C", $repo, "status", "--porcelain", "--untracked-files=no")) -join "`n"
if ($dirty.Trim() -ne "") { $dirty; Die "変更されたファイルがあります。焼く前に元に戻してください" }
$HEAD = ((Cap "git" @("-C", $repo, "rev-parse", "--short", "HEAD")) -join "").Trim()
$BR   = ((Cap "git" @("-C", $repo, "rev-parse", "--abbrev-ref", "HEAD")) -join "").Trim()
$V    = (node -p "require('./vcs3-app/package.json').version").Trim()
"  AJT JOY16 v$V  ($BR @ $HEAD)"

# -----------------------------------------------------------------------------
Step "2/8 プラグインのビルド（VST3 + AAX、音源と FX）"
$wv = Get-ChildItem "$env:LOCALAPPDATA\PackageManagement\NuGet\Packages" -Directory -Filter "Microsoft.Web.WebView2*" -ErrorAction SilentlyContinue
if (-not $wv) {
    "  … WebView2 SDK を入れる（初回だけ）"
    Register-PackageSource -ProviderName NuGet -Name nugetRepository -Location https://www.nuget.org/api/v2 -Force -ErrorAction SilentlyContinue | Out-Null
    Install-Package Microsoft.Web.WebView2 -Scope CurrentUser -RequiredVersion 1.0.1901.177 -Source nugetRepository -Force | Out-Null
}
$B = P @($Work, "plugin")
if ((Run $cmake @("-S", (P @($repo, "vcs3-plugin")), "-B", $B, "-A", "x64", "-DJOY16_WITH_AAX=ON")) -ne 0) { Die "cmake の設定に失敗" }
if ((Run $cmake @("--build", $B, "--config", "Release", "--parallel", "--target",
                  "VCS3PinMatrix_VST3", "VCS3PinMatrix_AAX", "JOY16FX_VST3", "JOY16FX_AAX")) -ne 0) { Die "ビルドに失敗" }
$vst3   = P @($B, "VCS3PinMatrix_artefacts", "Release", "VST3", "AJT JOY16.vst3")
$vst3fx = P @($B, "JOY16FX_artefacts", "Release", "VST3", "AJT JOY16 FX.vst3")
$aax    = P @($B, "VCS3PinMatrix_artefacts", "Release", "AAX", "AJT JOY16.aaxplugin")
$aaxfx  = P @($B, "JOY16FX_artefacts", "Release", "AAX", "AJT JOY16 FX.aaxplugin")
foreach ($f in @($vst3, $vst3fx, $aax, $aaxfx)) { if (-not (Test-Path $f)) { Die "できていません: $f" } }
"  o AJT JOY16.vst3 / AJT JOY16 FX.vst3 / AJT JOY16.aaxplugin / AJT JOY16 FX.aaxplugin"

# -----------------------------------------------------------------------------
Step "3/8 アプリのビルド（AJT JOY16.exe）"
Set-Location (P @($repo, "vcs3-app"))
if ((Run "npm.cmd" @("ci")) -ne 0)            { Die "npm ci に失敗" }
if ((Run "npm.cmd" @("run", "sync")) -ne 0)   { Die "npm run sync に失敗" }
$env:CSC_IDENTITY_AUTO_DISCOVERY = "false"
if ((Run "npx.cmd" @("electron-builder", "--win", "dir", "--x64")) -ne 0) { Die "electron-builder に失敗" }
Set-Location $repo
$appDir = P @($repo, "vcs3-app", "dist", "win-unpacked")
if (-not (Test-Path (P @($appDir, "AJT JOY16.exe")))) { Die "AJT JOY16.exe ができていません" }
"  o $appDir"

# -----------------------------------------------------------------------------
Step "4/8 AAX に PACE 署名（wraptool）"
# The AAX is signed with a self-signed certificate, as for Groove Activator (Pro Tools
# checks the PACE signature). Use GA's certificate if it is on this PC.
$cert = Get-ChildItem Cert:\CurrentUser\My |
        Where-Object { ($_.FriendlyName -eq "Groove Activator AAX signing (self-signed)" -or $_.FriendlyName -eq "AJITO SOUNDS AAX signing (self-signed)") -and $_.NotAfter -gt (Get-Date).AddDays(30) } |
        Select-Object -First 1
if (-not $cert) {
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject "CN=AJITO SOUNDS, O=GOTA RECORDS" `
              -FriendlyName "AJITO SOUNDS AAX signing (self-signed)" -CertStoreLocation Cert:\CurrentUser\My `
              -KeyAlgorithm RSA -KeyLength 3072 -HashAlgorithm SHA256 -NotAfter (Get-Date).AddYears(5)
    "  o 自己署名の証明書を作った"
}
"  o AAX 用の証明書: $($cert.FriendlyName)  $($cert.Thumbprint)"
$pay = P @($Work, "payload")
if (Test-Path $pay) { Remove-Item -Recurse -Force $pay }
New-Item -ItemType Directory -Force -Path $pay | Out-Null
$signedAax = @()
foreach ($src in @($aax, $aaxfx)) {
    $name = Split-Path $src -Leaf
    Copy-Item -Recurse -Force $src $pay          # sign a copy, in place (--in = --out)
    $t   = P @($pay, $name)
    $bin = P @($t, "Contents", "x64", $name)
    if (-not (Test-Path $bin)) { Die "AAX の中身がありません: $bin" }
    $before = (Get-Item $bin).Length
    "  … $name"
    $out = Cap $wrap @("sign", "--verbose", "--account", $Account, "--wcguid", $Wcguid, "--signid", $cert.Thumbprint,
                       "--signtool", $st, "--extrasigningoptions", "digest_sha256", "--in", $t, "--out", $t)
    if ($script:LastRc -ne 0) { $out | Select-Object -Last 25; Die "wraptool sign に失敗: $name" }
    $after = (Get-Item $bin).Length
    if ($after -le $before) { Die "署名後も大きさが変わっていません（$before → $after）: $name" }
    $v = (Cap $wrap @("verify", "--verbose", "--in", $bin)) -join "`n"
    if ($v -notmatch "The digital signature was verified") { $v; Die "wraptool verify が通りません: $name" }
    "  o $name  {0:N0} → {1:N0} bytes, verified" -f $before, $after
    $signedAax += $t
}

# -----------------------------------------------------------------------------
Step "5/8 インストーラーを焼く（Inno Setup）"
$outDir = P @($Work, "out")
if (Test-Path $outDir) { Remove-Item -Recurse -Force $outDir }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$rc = Run $iscc @("/DAppVersion=$V", "/DAppDir=$appDir", "/DVst3Dir=$vst3", "/DVst3FxDir=$vst3fx",
                  "/DAaxDir=$($signedAax[0])", "/DAaxFxDir=$($signedAax[1])", "/O$outDir",
                  (P @($repo, "installer", "windows", "joy16.iss")))
if ($rc -ne 0) { Die "ISCC に失敗" }
$exe = P @($outDir, "AJT-JOY16-Installer-$V.exe")
if (-not (Test-Path $exe)) { Die "インストーラーができていません: $exe" }
$final = P @($outDir, "AJITO_SOUNDS_AJT_JOY16_Win_v${V}_${DAY}.exe")
Move-Item -Force $exe $final
"  o $final  ({0:N1} MB)" -f ((Get-Item $final).Length / 1MB)

# -----------------------------------------------------------------------------
Step "6/8 インストーラーに Certum 署名"
if ($certum) {
    $o = Cap $st @("sign", "/sha1", $SignThumbprint, "/fd", "sha256", "/td", "sha256", "/tr", "http://time.certum.pl", "/v", $final)
    if ($script:LastRc -ne 0) { $o | Select-Object -Last 20; Die "signtool に失敗（SimplySign Desktop にログインしていますか）" }
    $sig = Get-AuthenticodeSignature $final
    if ("$($sig.Status)" -ne "Valid" -or $sig.SignerCertificate.Thumbprint -ne $SignThumbprint) { Die "署名が Valid になっていません: $($sig.Status)" }
    if (-not $sig.TimeStamperCertificate) { Die "タイムスタンプが付いていません" }
    "  o Valid  $($sig.SignerCertificate.Subject)  (timestamp: $($sig.TimeStamperCertificate.Subject))"
} else { "  ! 署名しない（-NoSign）" }

# -----------------------------------------------------------------------------
Step "7/8 Dropbox / ファミリー ルームの Retail と Complimentary に入れる"
if ($SkipDropbox) {
    "  ! -SkipDropbox: Dropbox には入れません"
} else {
    $tpl_ja = [IO.File]::ReadAllText((P @($repo, "installer", "dist", "README_ja.txt")), $utf8)
    $tpl_en = [IO.File]::ReadAllText((P @($repo, "installer", "dist", "README_en.txt")), $utf8)
    $hash = (Get-FileHash $final -Algorithm SHA256).Hash.ToLower()
    $signNote = "NOT signed"
    if ($certum) { $signNote = "installer Certum-signed ($SignThumbprint)" }
    $winLine = "Windows: built on $env:COMPUTERNAME; VST3/AAX (instrument + FX) + app; AAX PACE-wrapped (wcguid $Wcguid), $signNote"
    foreach ($KIND in @("Retail", "Complimentary")) {
        $D = Get-ChildItem $FamilyDir -Directory -Filter "AJITO_SOUNDS_AJT_JOY16_v${V}_${KIND}_*" -ErrorAction SilentlyContinue |
             Sort-Object Name | Select-Object -Last 1 -ExpandProperty FullName
        if (-not $D) { $D = P @($FamilyDir, "AJITO_SOUNDS_AJT_JOY16_v${V}_${KIND}_${DAY}"); New-Item -ItemType Directory -Force -Path $D | Out-Null }
        Get-ChildItem $D -File -Filter "AJITO_SOUNDS_AJT_JOY16_Win_*.exe" | Remove-Item -Force
        $name = "AJITO_SOUNDS_AJT_JOY16_Win_v${V}_${DAY}_${KIND}.exe"
        Copy-Item -Force $final (P @($D, $name))

        if ($KIND -eq "Retail") { $ja = "お求めいただき、ありがとうございます。"; $en = "Thank you for your purchase." }
        else { $ja = "AJITO SOUNDS からの贈呈版です。どうぞお楽しみください。"; $en = "This is a complimentary copy from AJITO SOUNDS. Enjoy." }
        [IO.File]::WriteAllText((P @($D, "README_はじめに.txt")), $tpl_ja.Replace("VERSION", $V).Replace("__GREETING__", $ja), $utf8)
        [IO.File]::WriteAllText((P @($D, "README_EN.txt")),       $tpl_en.Replace("VERSION", $V).Replace("__GREETING__", $en), $utf8)

        # SHA256.txt: keep the Mac line, replace the Windows line
        $shaFile = P @($D, "SHA256.txt")
        $lines = @()
        if (Test-Path $shaFile) { $lines = @([IO.File]::ReadAllLines($shaFile, $utf8) | Where-Object { $_.Trim() -ne "" -and $_ -notmatch "_Win_" }) }
        $lines += "$hash  $name"
        [IO.File]::WriteAllText($shaFile, (($lines -join "`n") + "`n"), $utf8)

        # BUILT_FROM.txt: keep the Mac lines, replace the Windows line
        $bf = P @($D, "BUILT_FROM.txt")
        $old = @()
        if (Test-Path $bf) { $old = @([IO.File]::ReadAllLines($bf, $utf8)) }
        $mac = @($old | Where-Object { $_ -match "^Mac:" -or $_ -match "^\s+Developer ID" })
        if ($mac.Count -eq 0) { $mac = @("Mac: (not in this folder yet - installer/mac/release_mac.sh puts it here)") }
        $new = @("AJT JOY16 v$V  $KIND  " + $jst.ToString("yyyy-MM-dd"),
                 "source: groove-activator@$HEAD ($BR)") + $mac + @($winLine,
                 "serial: none (Retail and Complimentary installers are identical)")
        [IO.File]::WriteAllText($bf, (($new -join "`n") + "`n"), $utf8)

        if (-not (Get-ChildItem $D -File -Filter "*_Mac_*.pkg")) { "  ! $(Split-Path $D -Leaf) に Mac 版がまだありません" }
        "  o $(Split-Path $D -Leaf)"
    }
}

# -----------------------------------------------------------------------------
Step "8/8 完了"
"OK AJT JOY16 v$V（Windows: VST3・AAX・アプリ）  {0:N0} 分" -f ((Get-Date) - $t0).TotalMinutes
"   $final"
if (-not $SkipDropbox) { "   Dropbox のファミリー ルームに入れました。同期が終われば、他の人からも見えます。" }
exit 0
