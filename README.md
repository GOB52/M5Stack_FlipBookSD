# M5Stack_FlipBookSD

[English](README.en.md)

## デモ

https://github.com/GOB52/M5Stack_FlipBookSD/assets/26270227/3c894cd5-74e4-4016-9707-a489553fe9e6

SINTEL (Trailer)
[Creative Commons Attribution 3.0](http://creativecommons.org/licenses/by/3.0/)  
© copyright Blender Foundation | [www.sintel.org](https://www.sintel.org)  
再生の為にデータ形式を改変しています。

## 概要
動画ファイルを専用の形式 (gmv) へ変換したファイルを SD からストリーミング再生するアプリケーションです。  
マルチコアを使用して DMA による描画と、音声再生を行っています。  
***旧形式である gcf + wav は 0.1.1 より再生不可となりました。gmv 形式で再生成するか、gcf + wav => gmv 変換スクリプトを用いて変換してください。***


## 対象デバイス
依存するライブラリが動作して、SDカードを搭載している物。
* M5Stack Basic 2.6 以降
* M5Stack Gray
* M5Stack Core2
* M5Stack CoreS3

## 依存ライブラリ
* [M5Unified](https://github.com/m5stack/M5Unified)
* [M5GFX](https://github.com/m5stack/M5GFX) 
* [SdFat](https://github.com/greiman/SdFat)

## 対応ライブラリ(必須ではありません)
* [M5Stack-SD-Updater](https://github.com/tobozo/M5Stack-SD-Updater)

## 含まれているライブラリ
* [TJpgDec](http://elm-chan.org/fsw/tjpgd/00index.html)  Lovyan03 さんの改造板

## ビルド種別(PlatformIO)
### Basic,Gray,Core2 用
|Env|説明|
|---|---|
|release|基本設定|
|release\_DisplayModule| [ディスプレイモジュール](https://shop.m5stack.com/products/display-module-13-2)対応 |
|release\_SdUpdater| SD-Updater 対応 |
|release\_SdUpdater\_DisplayModule| ディスプレイモジュールと SD-Updater 対応|

### CoreS3 用
|Env|説明|
|---|---|
|S3\_release|基本設定|
|S3\_release_DisplayModule| ディスプレイモジュール 対応|

### 再生用サンプルデータ
[sample_0_1_1.zip](https://github.com/GOB52/M5Stack_FlipBookSD/files/11871296/sample_0_1_1.zip) をダウンロードして解凍し、 SD カードの **/gmv** へコピーしてください。

## データの作成方法
### 必要なもの
* [Python3](https://www.python.org/)
* [FFmpeg](https://ffmpeg.org/)
* [Convert(ImageMagick)](https://imagemagick.org/)
* [ffmpeg-normalize](https://github.com/slhck/ffmpeg-normalize)

インストール方法などは各ウェブページを参照してください。

### 手順
データはターミナル上で作成します。  
動画データは FFmpeg で処理できる物であればフォーマットは問いません。
1. 任意に作ったデータ作成用ディレクトリに動画データをコピーする
1. 同ディレクトリに [conv.sh](script/conv.sh) と [gmv.py](script/gmv.py) をコピーする
1. シェルスクリプトを次のように指定して実行する。  
**bash conv.sh movie_file_path frame_rate [ jpeg_maxumum_size (無指定時は 7168) ]**

|引数|必須?|説明|
|---|---|---|
|movie\_file_path|YES|元となる動画|
|frame\_rate|YES|出力されるデータの FPS (1.0 - 30.0)<br>整数または小数を指定可能|
|jpeg\_maximum\_size|NO|JPEG 1枚あたりの最大ファイルサイズ( 1024 - 10240)<br>大きいと品質が維持されるが処理遅延が発生する可能性が高くなる(既知の問題参照)|

4. 動画ファイル名.gmv が出力される。
5. gmv ファイルを SD カードの **/gmv** にコピーする。

例)
```sh
mkdir foo
cp bar.mp4 foo
cp script/conv.sh foo
cp script/gmv.py foo
cd foo
bash conv.sh bar.mp4 29.97
cp bar.gmv your_sd_card_path/gmv
```

### シェルスクリプトで行っている事
* 動画から指定されたフレームレートでJPEG画像を出力する。  
出力ディレクトリとして ./jpg+PID を作成します。これにより複数のターミナルで並行して変換できます。
* 出力された JPEG ファイルのサイズが、指定サイズを超えていたら収まるように再コンバートする。
* 動画から音声データを出力し、平滑化する。
* gmv.py で画像と音声を合成した専用ファイルを作成する。

#### FFmpeg のパラメータ
```sh
ffmpeg -i $1 -r $2 -vf scale=320:-1,dejudder -qmin 1 -q 1 jpg$$/%06d.jpg
```
出力品質や、フィルター等、変更することでお好みのものにできます。元となる動画によって最適なパラメータは異なるので、FFmpeg の情報を参考にして行ってください。

### データの制限
* 変換可能な動画フォーマット  
FFMpeg が扱う事ができないフォーマットはサポートされません。

* wav データの品質 (8KHz 符号なし 8bit mono)  
処理負荷軽減の為、音声データの品質は下げています。  
スクリプトを編集して品質を上げることは可能ですが、処理負荷によって処理遅延が生じるかもしれません。(既知の問題参照)

* 画像サイズとフレームレート  
動画から JPEG 化する際には幅 320p 高さはアスペクト比を維持した値で出力します。  
<ins>現在の所 320 x 240 で 24 FPS 程度、 320 x 180 で 30 FPS 程度の再生が可能です。</ins>  
画像サイズを変更したい場合は [conv.sh](conv.sh) の FFmpeg へ与えているパラメータを変更してください。 **(scale=)**

* 画像サイズと出力先サイズ  
画像データが出力先サイズに満たない、または逸脱する場合は、センタリングして表示されます。

### データの検索
**/gmv** 内のファイルを探索します。もし存在しない場合は旧版の **/gcf** 内を検索します。  
両方存在する場合は /gmv のみ対象となります。


## 既知の問題
### 音声が途切れる、再生速度が遅い
1 フレーム内での処理が間に合っていないことが原因です。  
またごく稀に SD からの読み込みに時間がかかる時があり、それによって処理が間に合わないフレームが生じているかもしれません。  
SD カードの相性かフォーマット状態に問題がある可能性があります。

https://github.com/greiman/SdFat/issues/96#issuecomment-377332392

>OS utilities should not be used for formatting SD cards. FAT16/FAT32 has lots of options for file system layout. In addition to the cluster size there are options for aligning file structures.  
>The SD Association has a standard layout for each size SD card. Cards are designed to optimize performance for the standard layout. For example, flash chip boundaries are aligned with file system structures.  
>My SdFormatter example produces the standard layout. On a PC use the SD Association Formatter.  
>You should not be getting errors due to the format. The correct format will only enhance performance, not reduce errors.  
>I rarely see the type errors you are having. Most users either have solid errors or no errors.  
>I have seen this type error when another SPI device interferes with the SD or when there are noisy or poor SPI signals.  

>[意訳]  
>SD カードのフォーマットに、各 OS のユーティリティを使わないでください。 FAT16/32 のファイルシステムレイアウトは多岐にわたっており、様々なオプションが存在します。  
>SD アソシエーションによって各サイズの SD カードに対して標準的なレイアウトが制定されています。カードは標準的なレイアウトに対して最適化されています。例えばフラッシュチップの境界はファイルシステムの構造に合わせて整列されます。  
>SD Formater サンプル、または [SD Association Formatter](https://www.sdcard.org/downloads/) を使用することで標準レイアウトが生成されます。  
>(以下略)


#### データでの回避策
* 再生フレームレートを減らす
```sh
bash conv.sh movie.mp4 30 # 30 FPS
bash conv.sh movie.mp4 24 # Reduce to 24
```
* JPEG ファイルサイズを小さくする 
```sh
bash conv.sh movie.mp4 30      # 7168 as default
bash conv.sh movie.mp4 30 5120 # Reduce to 5120
```
* 画像サイズを小さくする
```sh
conv.sh
# ...
#ffmpeg -i $1 -r $2 -vf scale=320:-1,dejudder -qmin 1 -q 1 jpg$$/%06d.jpg  # 320 x n pixel
 ffmpeg -i $1 -r $2 -vf scale=240:-1,dejudder -qmin 1 -q 1 jpg$$/%06d.jpg  # 240 x n pixel
# ...
```

## 操作方法
### メニュー
|ボタン|説明|
|---|---|
|Aボタンクリック|前のファイルへ|
|Bボタンクリック|再生開始|
|Cボタンクリック|次のファイルへ|
|Aボタン長押し|再生種別変更|
|Cボタン長押し|再生種別変更|

### 再生中
|ボタン(Basic,Gray,Core2)|画面タッチ(CoreS3)|説明|
|---|---|---|
|Aボタン押下|画面左 1/3|音量下げる|
|Bボタンクリック|画面真ん中 1/3|再生停止してメニューへ|
|Cボタン押下|画面右 1/3|音量上げる|

## 旧形式 (gcf + wav) からの変換
変換用 Pyhton スクリプト [gcf_to_gmv.py](script/gcf_to_gmv.py) と、カレントディレクトリのファイル群の変換の為のシェルスクリプト [convert_gcf_to_gmv.sh](script/convert_gcf_to_gmv.sh) を用意しました。

```sh
# gcf_dir has gcf + wav files
cp script/gcf_to_gmv.py gcf_dir
cp script/convert_gcf_to_gmv.sh gcf_dir
cd gcf_dir
bash convert_gcf_to_gmv.sh
cp *.gmv your_sd_card_path/gmv
```

## 余談
### 何故 JPEG ファイルをまとめているの?
SD カードのファイルのオープンとシークにはそれなりの時間がかかります。  
1 枚ずつの JPEG ファイルを逐次開いて表示する場合、その速度がボトルネックとなるのです。  
そこで物理メディア(フロッピーディスク等)、光学メディア(CD 等) で有効だった、一つにまとめた大きなファイルをシーケンシャルに読み込み続ける手法を採用し、カードアクセスの時間を短縮しています。  

当初は [unzipLIB](https://github.com/bitbank2/unzipLIB) で、無圧縮 ZIP ファイルから読み込んでいたのですが、内部でシークが 1 回行われる為、シーク無しにシーケンシャルに読み込むことを前提としたオリジナルのファイル形式を策定し、それを利用する事でシークを無くすことができました。  

1 画像あたりの読み込み処理時間は逐次オープンだと数十 ms だったものが 数 ms となっています。

### 余談の余談
元々は ZIP ファイルを扱いたくて unzipLIB を色々試していました。使い方の習得を兼ねて画像ファイルをまとめた物をパラパラ漫画のように再生しようと思ったのが始まりです。  
(結局 unzipLIB を使わない形になってしまったわけですが (´･ω･`) )

### これって [MotionJPEG](https://ja.wikipedia.org/wiki/Motion_JPEG) とは違うの?
定義によればこれも MJPEG と呼んで差し支えないようです。しかし MJPEG はファイルフォーマットとしての統一形式はないので、このアプリケーションはあくまで gmv 形式再生機です。

## 付録
* src/gob_jpg_sprite.hpp
* src/gob_jpg_sprite.cpp  
画面ではなく LGFX_Sprite にマルチコアを使用して JPEG を出力するためのクラスです。  
色々試行錯誤していた時に作ったのですが、結局使用しない事になりました。  
もったいないので同梱します。

## 謝辞
先人としてパラパラ再生をされている方々。お二人に刺激を受けて当アプリケーションは誕生しました。
* [6jiro](https://twitter.com/6jiro1) さん  
https://smile-dental-clinic.info/wordpress/?p=10904  
* [PocketGriffon](https://twitter.com/GriffonPocket) さん  
https://pocketgriffon.hatenablog.com/entry/2023/02/27/103329  

TJepgDec の作者
* [ChaN](http://elm-chan.org/) さん  
[TJpgDec](http://elm-chan.org/fsw/tjpgd/00index.html)


当アプリケーションの母体となった物です。TJpegDec を使用して画面に DMA 描画するロジックを拝借しました。  
技術的なアドバイスもたくさんいただきました。  
M5Unified, M5GFX の作者でもあります。
* [Lovyan03](https://twitter.com/lovyan03) さん   
[M5Stack_JpgLoopAnime](https://github.com/lovyan03/M5Stack_JpgLoopAnime)  
[ESP32_ScreenShotReceiver](https://github.com/lovyan03/ESP32_ScreenShotReceiver)


