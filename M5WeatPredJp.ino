#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <math.h>

static M5GFX display;

// ----------------------------------------------------------
// 各種定義（ユーザ）
// ----------------------------------------------------------
static const char *ssid = "";
static const char *password = "";
// 東京地方の例
static const char *endpoint_weatherData = "https://www.jma.go.jp/bosai/forecast/data/forecast/130000.json";
static const char *region_3days = "伊豆諸島北部";  // 3日天気予報のエリア
static const char *region_3days_city = "大島";     // 3日天気予報のエリア(都市名)
static const char *region_7days = "小笠原諸島";    // 7日天気予報のエリア
static const char *region_7days_city = "父島";     // 7日天気予報のエリア（都市名）

// ----------------------------------------------------------
// 各種定義
// ----------------------------------------------------------
static const uint8_t days3 = 0;  // 3日予報のインデックス
static const uint8_t days7 = 1;  // 7日予報のインデックス
typedef enum {
  kDisplayDate_Today = 0,
  kDisplayDate_Tomorrow,
  kDisplayDate_DayAfterTomorrow
} kDisplayDate;

// ----------------------------------------------------------
// 起動処理
// ----------------------------------------------------------
void setup() {
  auto cfg = M5.config();
  cfg.clear_display = true;  // 起動時に画面クリア
  M5.begin(cfg);

  // ディスプレイの初期化
  display.init();
  display.setBrightness(128);       // 輝度を中程度に設定（0-255）
  display.setRotation(1);           // 画面を縦向きに設定（270度回転）
  display.setTextColor(TFT_WHITE);  // テキスト色設定
  display.setTextFont(4);           // フォントサイズ設定

  WiFi.begin(ssid, password);
  drawBlueScreen("Waiting Wi-fi Connection");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

// ----------------------------------------------------------
// ループ処理
// ----------------------------------------------------------

void loop() {
  static kDisplayDate nowDisplay = kDisplayDate_Today;
  static uint32_t ms100counter = 0;
  static uint32_t ppm_old = 0;
  static uint32_t ppm_now = 0;
  static bool stop_co2_update = false;
  static DynamicJsonDocument weatherInfo(20000);

  // ボタンに合わせて天気の表示日を変える
  bool need_redraw = false;
  M5.update();

  // タッチ処理
  if (M5.Touch.getCount()) {
    auto t = M5.Touch.getDetail();
    if (t.wasPressed()) {
      int16_t x = t.x;
      int16_t y = t.y;

      if (x < display.width() / 3) {  // 左エリア
        nowDisplay = kDisplayDate_Today;
        need_redraw = true;
        stop_co2_update = false;
      } else if (x < (display.width() * 2 / 3)) {  // 中央エリア
        nowDisplay = kDisplayDate_Tomorrow;
        need_redraw = true;
        stop_co2_update = false;
      } else {  // 右エリア
        nowDisplay = kDisplayDate_DayAfterTomorrow;
        need_redraw = true;
        stop_co2_update = false;
      }
    }
  }

  // 開始直後または1時間に1度、天気情報を更新する
  if (ms100counter == 0 || ms100counter > (60 * 60 * 10 * 1)) {
    weatherInfo = getJson(endpoint_weatherData);
    need_redraw = true;
    ms100counter = 1;
  }

  // 画面の再描画フラグが立っている場合、再描画する
  if (need_redraw) {
    drawWeather(weatherInfo, (uint32_t)nowDisplay);
  }

  delay(100);
  ms100counter++;
}

// ----------------------------------------------------------
// 情報更新系関数
// ----------------------------------------------------------
// JSONデータを取得する
DynamicJsonDocument getJson(const char* url) {
  DynamicJsonDocument jsonDoc(20000);

  if ((WiFi.status() == WL_CONNECTED)) {
    HTTPClient http;
    http.begin(url);
    if (http.GET() > 0) {
      deserializeJson(jsonDoc, http.getString());
    } else {
      drawBlueScreen("HTTP Error!");
    }
    http.end();
  } else {
    drawBlueScreen("Wi-fi Error!");
  }
  return jsonDoc;
}

// ----------------------------------------------------------
// 画面描画系関数
// ----------------------------------------------------------
// 天気情報を描画する
void drawWeather(DynamicJsonDocument jsonDoc, uint32_t day_index) {
  if (jsonDoc == NULL) return;

  display.clear();
  drawBackLine(day_index);                                 // 区切り線の描画
  drawDataDays(jsonDoc);                                   // データ取得日の描画
  String todayStr = drawSelectedDays(jsonDoc, day_index);  // 選択されたデータの日付の描画（3日予報の日付を使用）
  drawWeatherForcast(jsonDoc, day_index);                  // 天気予報の描画（3日予報）
  drawRainPred(jsonDoc, todayStr);                         // 降水確率の描画（3日予報+7日予報）
  drawTemp(jsonDoc, todayStr);                             // 最低・最高気温の描画（3日予報（地区詳細）+7日予報）

  display.setTextSize(2);
  display.setTextColor(TFT_WHITE);
  display.drawString("`C", 250, 135);
}
// Areaの文字列が一致するIndexを求める
uint8_t getAreaIndex(DynamicJsonDocument jsonDoc, uint32_t days, uint32_t time_index, String region) {
  for (int i = 0; i < jsonDoc[days]["timeSeries"][time_index]["areas"].size(); i++) {
    if (jsonDoc[days]["timeSeries"][time_index]["areas"][i]["area"]["name"].as<String>() == region) {
      return i;
    }
  }
  return 0;
}
// 背景の区切り線の描画
void drawBackLine(uint32_t day_index) {
  M5.Lcd.drawLine(0, 50, 320, 50, TFT_LIGHTGREY);
  M5.Lcd.drawLine(0, 190, 320, 190, TFT_LIGHTGREY);
  M5.Lcd.drawLine(120, 130, 120, 175, TFT_LIGHTGREY);
  M5.Lcd.fillRect(317, 80 * day_index, 3, 80, TFT_LIGHTGREY);  // 表示中の日を表すバー
}
// データ取得日の描画
void drawDataDays(DynamicJsonDocument jsonDoc) {
  String date_origin = jsonDoc[days3]["reportDatetime"];
  String dataTime = date_origin.substring(0, 19);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.setTextFont(2);
  M5.Lcd.drawString(dataTime, 0, 0);
}
// 選択されたデータの日付の描画（3日予報の日付を使用）
String drawSelectedDays(DynamicJsonDocument jsonDoc, uint32_t day_index) {
  JsonObject dayData = jsonDoc[days3]["timeSeries"][0];
  String date_today = dayData["timeDefines"][day_index];
  String todayStr = date_today.substring(0, 10);
  M5.Lcd.setTextFont(4);
  M5.Lcd.drawString(todayStr, 5, 20);
  return todayStr;
}
// 天気予報の描画（3日予報）
void drawWeatherForcast(DynamicJsonDocument jsonDoc, uint32_t day_index) {
  JsonObject weatherData = jsonDoc[days3]["timeSeries"][0];
  String weather = weatherData["areas"][getAreaIndex(jsonDoc, days3, 0, region_3days)]["weathers"][day_index].as<String>();
  String weatherStr = "";
  if (weather.indexOf("晴") != -1) {
    weatherStr = "sunny ";
    M5.Lcd.setTextColor(TFT_RED);
  }
  if (weather.indexOf("雨") != -1) {
    weatherStr = "rainy ";
    M5.Lcd.setTextColor(TFT_BLUE);
  }
  if (weather.indexOf("雪") != -1) {
    weatherStr = "snow ";
    M5.Lcd.setTextColor(TFT_WHITE);
  }
  if (weather.indexOf("くもり") != -1) {
    weatherStr += "cloudy";
    M5.Lcd.setTextColor(TFT_LIGHTGREY);
  }
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawString(weatherStr, 10, 55);
}
// 降水確率の描画（3日予報+7日予報）
void drawRainPred(DynamicJsonDocument jsonDoc, String todayStr) {
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.drawString("%", 280, 200);

  JsonObject rainfallData = jsonDoc[days3]["timeSeries"][1];  // 3日予報の降水確率
  uint32_t rainPred_draw_cnt = 0;
  bool data_exist = false;
  for (int i = 0; i < rainfallData["timeDefines"].size(); i++) {
    if (rainfallData["timeDefines"][i].as<String>().indexOf(todayStr) != -1) {
      uint32_t rainPred = rainfallData["areas"][getAreaIndex(jsonDoc, days3, 0, region_3days)]["pops"][i].as<int>();
      M5.Lcd.drawString(String(rainPred), 20 + rainPred_draw_cnt * 65, 200);
      M5.Lcd.fillRect(rainPred_draw_cnt * 65, 233, 65, 7, getDrawColorFromRainPred(rainPred));
      rainPred_draw_cnt++;
      data_exist = true;
    }
  }
  if (data_exist) return;
  rainfallData = jsonDoc[days7]["timeSeries"][0];  // 7日予報の降水確率
  for (int i = 0; i < rainfallData["timeDefines"].size(); i++) {
    if (rainfallData["timeDefines"][i].as<String>().indexOf(todayStr) != -1) {
      uint32_t rainPred = rainfallData["areas"][getAreaIndex(jsonDoc, days7, 0, region_7days)]["pops"][i].as<int>();
      M5.Lcd.drawString(String(rainPred), 20, 200);
      M5.Lcd.fillRect(0, 233, 65, 7, getDrawColorFromRainPred(rainPred));
    }
  }
}
// 最低・最高気温の描画（3日予報（地区詳細）+7日予報（地区詳細））
void drawTemp(DynamicJsonDocument jsonDoc, String todayStr) {
  M5.Lcd.setTextSize(3);
  uint32_t days3_minTemperature = 999;
  uint32_t days3_maxTemperature = 0;
  bool data_exist = false;
  JsonObject temperatureData = jsonDoc[days3]["timeSeries"][2];  // 3日予報（地区詳細）の気温
  for (int i = 0; i < temperatureData["timeDefines"].size(); i++) {
    if (temperatureData["timeDefines"][i].as<String>().indexOf(todayStr) != -1) {
      uint32_t val = temperatureData["areas"][getAreaIndex(jsonDoc, days3, 2, region_3days_city)]["temps"][i];
      if (val < days3_minTemperature) days3_minTemperature = val;
      if (val > days3_maxTemperature) days3_maxTemperature = val;
      data_exist = true;
    }
  }
  if (data_exist) {
    M5.Lcd.setTextColor(TFT_BLUE);
    M5.Lcd.drawString(String(days3_minTemperature), 10, 120);
    M5.Lcd.setTextColor(TFT_RED);
    M5.Lcd.drawString(String(days3_maxTemperature), 137, 121);
    return;
  }

  temperatureData = jsonDoc[days7]["timeSeries"][1];  // 7日予報（地区詳細）の気温
  for (int i = 0; i < temperatureData["timeDefines"].size(); i++) {
    if (temperatureData["timeDefines"][i].as<String>().indexOf(todayStr) != -1) {
      String minTemperature = temperatureData["areas"][getAreaIndex(jsonDoc, days7, 1, region_7days_city)]["tempsMin"][i];
      String maxTemperature = temperatureData["areas"][getAreaIndex(jsonDoc, days7, 1, region_7days_city)]["tempsMax"][i];
      M5.Lcd.setTextColor(TFT_BLUE);
      M5.Lcd.drawString(minTemperature, 10, 120);
      M5.Lcd.setTextColor(TFT_RED);
      M5.Lcd.drawString(maxTemperature, 137, 121);
      return;
    }
  }
}
// 降水確率を描画する色を決める
uint16_t getDrawColorFromRainPred(uint32_t val) {
  if (val <= 20) return TFT_RED;
  if (val <= 60) return TFT_LIGHTGREY;
  return TFT_BLUE;
}

// 青画面の描画（各種情報表示用）
void drawBlueScreen(String s) {
  display.fillScreen(TFT_BLUE);
  display.setTextColor(TFT_WHITE);
  display.setTextSize(1);
  display.drawString(s, 0, 10);
}
