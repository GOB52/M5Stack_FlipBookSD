# M5Stack_FlipBookSD

[in English](README.en.md)

## 概要

動画ファイルから作成された分割 [JPEG](https://ja.wikipedia.org/wiki/JPEG) ファイルを gcf (オリジナル形式) でまとめたファイルと Wave ファイルを SD カードから再生します。  
SD からパラパラ漫画のように画像と音声を再生することで、動画再生に近い事をするアプリケーションです。  
マルチコアを使用して DMA による描画と、音声再生を行っています。

## 対象デバイス
依存するライブラリが動作して、SDカードを搭載している物。
* M5Stack Basic 2.6
* M5Stack Gray
* M5Stack Core2
* その他 基準を満たす物

但し PSRAM を積んでいない Basic と Gray では音声再生可能な時間に大幅な制限あり。

## 依存ライブラリ
* [M5Unified](https://github.com/m5stack/M5Unified)
* [M5GFX](https://github.com/m5stack/M5GFX) 
* [SdFat](https://github.com/greiman/SdFat)

## 対応ライブラリ(必須ではありません)
* [M5Stack-SD-Updater](https://github.com/tobozo/M5Stack-SD-Updater)

## 含まれているライブラリ
* [TJpgDec](http://elm-chan.org/fsw/tjpgd/00index.html)  Lovyan03 さんの改造板

## データの作成方法

### 必要なもの
* [Python3](https://www.python.org/)
* [FFmpeg](https://ffmpeg.org/)
* [Convert(ImageMagick)](https://imagemagick.org/)
* [ffmpeg-normalize](https://github.com/slhck/ffmpeg-normalize)

インストール方法などは各ヘージを参照してください。

### 手順
データはターミナル上で作成します。  
動画データは FFmpeg で処理できる物であればフォーマットは問いません。
1. 任意に作ったデータ作成用ディレクトリに動画データをコピーする
1. 同ディレクトリに [conv.sh](conv.sh) と [gcf.py](gcf.py) をコピーする
1. シェルスクリプトを次のように指定して実行する。(シェルはあなたの環境のものを使用してください)  
**bash conv.sh 動画ファイル名 フレームレート(数)**
1. 動画ファイル名.フレームレート.gcf 動画ファイル名.wav が出力される
1. 上記 2 つのファイルを SD カードの **/gcf** にコピーする。

例)
```
cd ~
mkdir making_dir
cp bar.mp4 making_dir
cp conv.sh making_dir
cp gcf.py making_dir
bash conv.sh bar.mp4 24
cp bar.24.gcf your_sd_card_path/gcf
cp bar.wav your_sd_card_path/gcf
```

### データの制限
* wav データは 8KHz 符号なし 8bit mono で出力します  
再生時はメモリ上に全て読み込んでから再生するので、 足りない場合は音は鳴りません。PSRAM がない機種では短い物でないと鳴らないでしょう。  
Core2 では PSRAM 通常使用可能なサイズが 4MiB なので、約8分30秒が再生可能最大長と思われます。
* 画像サイズとフレームレート  
動画から JPEG 化する際には幅 320p 高さはアスペクト比を維持した値で出力します。  
現在の所 320 x 240 で 24 FPS 程度、 320 x 180 で 30 FPS 程度の再生が可能です。  
画像サイズを変更したい場合は [conv.sh](conv.sh) の以下の部分の **scale=...** の部分を編集してください。
```sh
ffmpeg -i $1 -r $2 -vf scale=320:-1 jpg/%05d.jpg
```

## 既知の問題
### 再生時にリセットがかかる
実行中にリセットがかかった場合 Serial モニタに assert で引っ掛かった旨が表示されているはずです。  
原因は描画が所定の時間内に終わらず、 SD と Lcd のバスが衝突した事による物です。

またごく稀に SD からの読み込みに時間がかかる時があり、それによって上記と同様にリセットがかかる場合があります。  
これは原因がわかっていません。

#### プログラムでの対処
マルチコア再生をシングルコア再生に切り替えてみる。  
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
#### データでの対処
再生フレームレートを減らしてみてください。 conv.sh へ与える引数で調整します。  
または画像サイズを小さくするのも有効です。

```
bash conv.sh video,mp4 30 # If reset during playback
bash conv.sh video.mp4 24 # Reduce frame rate
```
```sh
conv.sh
# ...
#ffmpeg -i $1 -r $2 -vf scale=320:-1 jpg/%05d.jpg # Width 320px standard
ffmpeg -i $1 -r $2 -vf scale=240:-1 jpg/%05d.jpg  # Reduce width to 240px
# ...
```

## 操作方法
### メニュー

|ボタン|機能|
|---|---|
|Aボタンクリック|前のファイルへ|
|Bボタンクリック|再生開始|
|Cボタンクリック|音量上げる|
|Aボタン長押し|再生方法変更|
|Cボタン長押し|再生方法変更|

### 再生中
|ボタン|機能|
|---|---|
|Aボタン押下|音量下げる|
|Bボタンクリック|再生停止してメニューへ|
|Cボタン押下|音量上げる|

## 余談
### 何故 JPEG ファイルをまとめているの?
SD カードのファイルのオープンとシークにはそれなりの時間がかかります。  
1 枚ずつの JPEG ファイルを逐次開いて表示する場合、その速度がボトルネックとなるのです。  
そこで物理メディア(フロッピーディスク等)、光学メディア(CD 等) で有効だった、一つにまとめた大きなファイルをシーケンシャルに読み込み続ける手法を採用し、カードアクセスの時間を短縮しています。  

当初は [unzipLIB](https://github.com/bitbank2/unzipLIB) で、無圧縮 ZIP ファイルから読み込んでいたのですが、内部でシークが 1 回行われる為、シーク無しにシーケンシャルに読み込むことを前提としたオリジナルのファイル形式を策定し、それを利用する事でシークを無くすことができました。  

1 画像あたりの読み込み処理時間は逐次オープンだと数十 ms だったものが 数 ms となっています。

### gcf ファイル
拡張子は **G**ob **C**ombined **F**iles の略称です。  
以下の模式のように、ヘッダに続いてサイズと実データが並んでいるだけの単純なファイルです。  
先頭からサイズ分だけ読み続けていくだけで一切明示的なシークを必要としません。

```cpp
//GCF HEADER
uint32_t signature; // "GCF0" 0x30464347 (little endian)
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

### 余談の余談

元々は ZIP ファイルを扱いたくて unzipLIB を色々試していました。使い方の習得を兼ねて画像ファイルをまとめた物をパラパラ漫画のように再生しようと思ったのが始まりです。  
(結局 unzipLIB を使わない形になってしまったわけですが (´･ω･`) )



## 謝辞
先人としてパラパラ再生をされている方々。お二人に刺激を受けて当アプリケーションは誕生しました。
* [6jiro](https://twitter.com/6jiro1) さん  
https://smile-dental-clinic.info/wordpress/?p=10904  
* [PocketGriffon](https://twitter.com/GriffonPocket) さん  
https://pocketgriffon.hatenablog.com/entry/2023/02/27/103329  

TJepgDec の作者
* [ChaN](http://elm-chan.org/) さん  
[TJpgDec](http://elm-chan.org/fsw/tjpgd/00index.html)


当アプリケーションの母体となった物です。TJpegDec を使用した JPEG デコーダ部分を拝借し、改造して使っています。  
M5Unfied, M5GFX の作者でもあります。
* [Lovyan03](https://twitter.com/lovyan03) さん   
[M5Stack_JpgLoopAnime](https://github.com/lovyan03/M5Stack_JpgLoopAnime)  
[ESP32_ScreenShotReceiver](https://github.com/lovyan03/ESP32_ScreenShotReceiver)








