気象庁の週間天気情報をM5Stackに表示します。  

# ハードウェア
M5Stack Core

# 事前準備

1. "// 各種定義（ユーザ）" となっている部分に、Wi-FiのSSID、パスワードを設定
1. 気象庁 - 全国の天気予報（https://www.jma.go.jp/bosai/forecast/ ） にアクセス
1. 都道府県選択メニューを開き、情報を取得したい都道府県を選択し、URLの"area_code=XXXXXX" をメモ
1. "https://www.jma.go.jp/bosai/forecast/data/forecast/XXXXXX.json"; にarea_coreを入力
1. 都道府県選択メニューで開いた天気予報の"明後日までの詳細" "７日先まで" から、地方名、都市名、県名、都市名を入力する

|変数名|説明|
|-|-|
|region_3days|"明後日までの詳細" の地方名|
|region_3days_city|"明後日までの詳細" の"気温（℃）" の右側の都市名|
|region_7days|"７日先まで" の地方名|
|region_7days_city|"７日先まで" の都市名|

# 操作方法

|ボタン|動作|
|-|-|
|左|"明後日までの詳細" データの1日目を表示|
|真ん中|〃 2日目|
|右|〃 3日目|
|左 → 左|気象庁の天気図画像を表示（真ん中、右ボタンで表示を解除）|

# 使用させていただいたライブラリ
pngle：https://github.com/kikuchan/pngle  

PNG画像をWebから取得して表示するのに使わせていただきました。

# データ取得URL
* https://www.jma.go.jp/bosai/forecast/data/forecast/
* https://www.jma.go.jp/bosai/weather_map/data/list.json
* https://www.jma.go.jp/bosai/weather_map/data/png/

# 参考URL
* https://qiita.com/michan06/items/48503631dd30275288f7