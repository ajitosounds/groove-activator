VCS3 Pin Matrix
===============

このフォルダには、Mac 用と Windows 用の 2 種類が入っています。

Mac/
  VCS3-Pin-Matrix-<版>.dmg
    単体のアプリです。開いて「VCS3 Pin Matrix」をアプリケーションフォルダへドラッグしてください。
    オリジナル（1970 年のマニュアルどおり）と Advanced を「版」メニュー（⌘1 / ⌘2）で切り替えられます。
  VCS3-Pin-Matrix-Plugin-macOS.zip
    DAW 用のプラグイン（Advanced 版）。VST3・AU・単体アプリが入っています。入れ方は中の INSTALL.txt を見てください。

Windows/
  VCS3-Pin-Matrix-Setup-<版>.exe
    単体のアプリのインストーラーです。オリジナルと Advanced を「版」メニュー（Ctrl+1 / Ctrl+2）で切り替えられます。
  VCS3-Pin-Matrix-Plugin-Windows.zip
    DAW 用のプラグイン（Advanced 版）。VST3・単体アプリが入っています。入れ方は中の INSTALL.txt を見てください。

はじめて開くとき
  Apple の公証・Windows のコード署名をしていないので、警告が出ます。
  Mac: 右クリック →「開く」。「壊れている」と出たら、ターミナルで
       xattr -cr "/Applications/VCS3 Pin Matrix.app"
  Windows: SmartScreen で「詳細情報」→「実行」。
