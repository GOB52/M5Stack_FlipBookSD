# M5Stack_FlipBookSD

[English](README.en.md)

## デモ

https://github.com/GOB52/M5Stack_FlipBookSD/assets/26270227/3c894cd5-74e4-4016-9707-a489553fe9e6

SINTEL (Trailer)
[Creative Commons Attribution 3.0](http://creativecommons.org/licenses/by/3.0/)  
© copyright Blender Foundation | [www.sintel.org](https://www.sintel.org)  
再生の為にデータ形式を改変しています。

## 概要

動画ファイルから作成された分割 [JPEG](https://ja.wikipedia.org/wiki/JPEG) ファイルを [gcf file](#gcfファイルフォーマット)(オリジナル形式) でまとめたファイルと Wave ファイルを SD カードから再生します。  
SD からパラパラ漫画のように画像と音声を再生することで、動画再生に近い事をするアプリケーションです。  
マルチコアを使用して DMA による描画と、音声再生を行っています。

## 対象デバイス
依存するライブラリが動作して、SDカードを搭載している物。
* M5Stack Basic 2.6
* M5Stack Gray
* M5Stack Core2
* M5Stack CoreS3

但し PSRAM を積んでいない Basic と Gray では音声再生可能な時間に大幅な制限あり。

## 依存ライブラリ
* [M5Unified](https://github.com/m5stack/M5Unified)
* [M5GFX](https://github.com/m5stack/M5GFX) 
* [SdFat](https://github.com/greiman/SdFat)

## 対応ライブラリ(必須ではありません)
* [M5Stack-SD-Updater](https://github.com/tobozo/M5Stack-SD-Updater)

## 含まれているライブラリ
* [TJpgDec](http://elm-chan.org/fsw/tjpgd/00index.html)  Lovyan03 さんの改造板

## ビルド種別(PlatfromIO)
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
[sample_data_002.zip](https://github.com/GOB52/M5Stack_FlipBookSD/files/11531987/sample_data_002.zip) をダウンロードして SD カードの **/gcf** へコピーしてください。

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
1. 同ディレクトリに [conv.sh](conv.sh) と [gcf.py](gcf.py) をコピーする
1. シェルスクリプトを次のように指定して実行する。  
**bash conv.sh move_file_path frame_rate [ jpeg_maxumu,_size (無指定時は 7168) ]**

|引数|必須?|説明|
|---|---|---|
|move_file_path|YES|元となる動画|
|frame_rate|YES|出力されるデータの FPS (1 - 30)|
|jpeg_maximum_size|NO|JPEG 1枚あたりの最大ファイルサイズ( 1024 - 10240)<br>大きいと品質が維持されやすいがクラッシュする可能性が高くなる(既知の問題参照)|

4. 動画ファイル名.フレームレート.gcf 動画ファイル名.wav が出力される
5. 上記 2 つのファイルを SD カードの **/gcf** にコピーする。

例)
```
mkdir foo
cp bar.mp4 foo
cp conv.sh foo
cp gcf.py foo
cd foo
bash conv.sh bar.mp4 24
cp bar.24.gcf your_sd_card_path/gcf
cp bar.wav your_sd_card_path/gcf
```

### シェルスクリプトで行っている事
* 動画から指定されたフレームレートでJPEG画像を出力する。  
出力ディレクトリとして ./jpg+PID を作成します。これにより複数のターミナルで並行して変換できます。
* 出力された JPEG ファイルのサイズが、指定サイズを超えていたら収まるように再コンバートする。
* JPEG ファイルを結合して gcf ファイルを作成する。
* 動画から音声データを出力し、平滑化する。

#### FFmpeg のパラメータ
```sh
ffmpeg -i $1 -r $2 -vf scale=320:-1,dejudder -qmin 1 -q 1 jpg$$/%06d.jpg
```
出力品質や、フィルター等、変更することでお好みのものにできます。元となる動画によって最適なパラメータは異なるので、FFmpeg の情報を参考にして行ってください。

### データの制限
* wav データは 8KHz 符号なし 8bit mono で出力します  
再生時はメモリ上に全て読み込んでから再生するので、 足りない場合は音は鳴りません。PSRAM がない機種では短い物でないと鳴らないでしょう。  
Core2 では PSRAM 通常使用可能なサイズが 4MiB なので、約8分30秒が再生可能最大長と思われます。  
CoreS3 では 8MB 利用可能なので、約 16 分が再生可能最大長と思われます。

* 画像サイズとフレームレート  
動画から JPEG 化する際には幅 320p 高さはアスペクト比を維持した値で出力します。  
<ins>現在の所 320 x 240 で 24 FPS 程度、 320 x 180 で 30 FPS 程度の再生が可能です。</ins>  
画像サイズを変更したい場合は [conv.sh](conv.sh) の FFmpeg へ与えているパラメータを変更してください。 **(scale=)**


## 既知の問題
### 再生時にリセットがかかる
実行中にリセットがかかった場合 Serial モニタに assert で引っ掛かった旨が表示されているはずです。  
原因は描画が所定の時間内に終わらず、 SD と Lcd のバスが衝突した事による物です。

またごく稀に SD からの読み込みに時間がかかる時があり、それによって上記と同様にリセットがかかる場合があります。  
これは原因がわかっていません。

#### プログラムでの回避策
* マルチコア再生をシングルコア再生に切り替える  
main.cpp の該当箇所をシングルコア使用の物に切り替えます。  
再生速度は落ちますが、バス競合を確実に避けるようになります。

```cpp
src/main.cpp
static void loopRender()
{
    // ...
	{
        ScopedProfile(drawCycle);
        //mainClass.drawJpg(buffer, JPG_BUFFER_SIZE); // Process on multiple cores
        mainClass.drawJpg(buffer, JPG_BUFFER_SIZE, false); // Process on single core.
	}
    // ...
}
```
#### データでの回避策
* 再生フレームレートを減らす
```
bash conv.sh video.mp4 30 # 30 FPS
bash conv.sh video.mp4 24 # Reduce to 24
```
* JPEG ファイルサイズを小さくする 
```
bash conv.sh video.mp4 30      # 7168 as default
bash conv.sh video.mp4 30 5120 # Reduce to 5120
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

## gcfファイルフォーマット
拡張子は **G**ob **C**ombined **F**iles の略称です。  
以下の模式のように、ヘッダに続いてサイズと実データが並んでいるだけの単純なファイルです。  
先頭からサイズ分だけ読み続けていくだけで一切明示的なシークを必要としません。

```cpp
//GCF HEADER
uint32_t signature; // "GCF0" 0x30464347 (リトルエンディアン)
uint32_t files; // 格納されているファイル数
uint32_t reserved[2]; // 予約
//GCF FILES
uint32_t size0; // ファイルサイズ 0
uint8_t data0[size0]; // 無圧縮データ 0
uint32_t size1; // ファイルサイズ 1
uint8_t data1[size1]; // 無圧縮データ 1
.
.
.
uint32_t sizen{0xFFFFFFFF}; // 終端
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
定義によればこれも MJPEG と呼んで差し支えないようです。しかし MJPEG はファイルフォーマットとしての統一形式はないので、このアプリケーションはあくまで gcf 形式再生機です。


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
M5Unfied, M5GFX の作者でもあります。
* [Lovyan03](https://twitter.com/lovyan03) さん   
[M5Stack_JpgLoopAnime](https://github.com/lovyan03/M5Stack_JpgLoopAnime)  
[ESP32_ScreenShotReceiver](https://github.com/lovyan03/ESP32_ScreenShotReceiver)


