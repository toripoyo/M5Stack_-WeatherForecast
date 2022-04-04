#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <M5Stack.h>
#include <WiFi.h>
#include <math.h>

#include "pngle/pngle.h"

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
static const char *endpoint_weatherImg = "https://www.jma.go.jp/bosai/weather_map/data/list.json";
static const uint8_t days3 = 0;  // 3日予報のインデックス
static const uint8_t days7 = 1;  // 7日予報のインデックス
typedef enum {
  kDisplayDate_Today = 0,
  kDisplayDate_Tomorrow,
  kDisplayDate_DayAfterTomorrow,
  kDisplayDate_Map
} kDisplayDate;

// ----------------------------------------------------------
// 起動処理
// ----------------------------------------------------------
void setup() {
  M5.begin();
  Wire.begin();
  dacWrite(25, 0);  // ノイズ対策

  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.setTextFont(4);
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
  if (M5.BtnA.wasPressed()) {
    if (nowDisplay == kDisplayDate_Today) {
      // 気象庁の天気予報画像を表示
      nowDisplay = kDisplayDate_Map;
      DynamicJsonDocument pictureInfo = getJson(endpoint_weatherImg);
      String url = "https://www.jma.go.jp/bosai/weather_map/data/png/" + pictureInfo["near"]["ft24"][0].as<String>();
      load_png(url, 0.6);
      stop_co2_update = true;
    } else {
      nowDisplay = kDisplayDate_Today;
      need_redraw = true;
      stop_co2_update = false;
    }
  }
  if (M5.BtnB.wasPressed()) {
    nowDisplay = kDisplayDate_Tomorrow;
    need_redraw = true;
    stop_co2_update = false;
  }
  if (M5.BtnC.wasPressed()) {
    nowDisplay = kDisplayDate_DayAfterTomorrow;
    need_redraw = true;
    stop_co2_update = false;
  }

  // 開始直後または1時間に1度、天気情報を更新する
  if (ms100counter == 0 || ms100counter > (60 * 60 * 10 * 1)) {
    weatherInfo = getJson(endpoint_weatherData);
    need_redraw = true;
    ms100counter = 1;
  }

  // 画面の再描画フラグが立っている場合、再描画する
  if (need_redraw && nowDisplay < kDisplayDate_Map) {
    drawWeather(weatherInfo, (uint32_t)nowDisplay);
  }

  delay(100);
  ms100counter++;
}

// ----------------------------------------------------------
// 情報更新系関数
// ----------------------------------------------------------
// JSONデータを取得する
DynamicJsonDocument getJson(const char *url) {
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
// 天気情報を描画する（関数が長いがいずれ整理で…。）
void drawWeather(DynamicJsonDocument jsonDoc, uint32_t day_index) {
  if (jsonDoc == NULL) return;

  // 3日天気用のarea index
  uint8_t areaIndex_3days = 0;
  for (int i = 0; i < jsonDoc[days3]["timeSeries"][0]["areas"].size(); i++) {
    if (jsonDoc[days3]["timeSeries"][0]["areas"][i]["area"]["name"].as<String>() == region_3days) {
      areaIndex_3days = i;
      break;
    }
  }
  // 3日天気（詳細地域）用のarea index
  uint8_t areaIndex_3days_city = 0;
  for (int i = 0; i < jsonDoc[days3]["timeSeries"][2]["areas"].size(); i++) {
    if (jsonDoc[days3]["timeSeries"][2]["areas"][i]["area"]["name"].as<String>() == region_3days_city) {
      areaIndex_3days_city = i;
      break;
    }
  }
  // 7日間天気用のarea index
  uint8_t areaIndex_7days = 0;
  for (int i = 0; i < jsonDoc[days7]["timeSeries"][0]["areas"].size(); i++) {
    if (jsonDoc[days7]["timeSeries"][0]["areas"][i]["area"]["name"].as<String>() == region_7days) {
      areaIndex_7days = i;
      break;
    }
  }
  uint8_t areaIndex_7days_city = 0;
  for (int i = 0; i < jsonDoc[days7]["timeSeries"][0]["areas"].size(); i++) {
    if (jsonDoc[days7]["timeSeries"][1]["areas"][i]["area"]["name"].as<String>() == region_7days_city) {
      areaIndex_7days_city = i;
      break;
    }
  }

  M5.Lcd.clear();

  // 区切り線の描画
  M5.Lcd.drawLine(0, 50, 320, 50, TFT_LIGHTGREY);
  M5.Lcd.drawLine(0, 190, 320, 190, TFT_LIGHTGREY);
  M5.Lcd.drawLine(120, 130, 120, 175, TFT_LIGHTGREY);
  M5.Lcd.fillRect(317, 80 * day_index, 3, 80, TFT_LIGHTGREY);  // 表示中の日を表すバー

  // データ取得日の描画
  String date_origin = jsonDoc[days3]["reportDatetime"];
  String dataTime = date_origin.substring(0, 19);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.setTextFont(2);
  M5.Lcd.drawString(dataTime, 0, 0);

  // 選択されたデータの日付の描画（3日予報の日付を使用）
  JsonObject dayData = jsonDoc[days3]["timeSeries"][0];
  String date_today = dayData["timeDefines"][day_index];
  String todayStr = date_today.substring(0, 10);
  M5.Lcd.setTextFont(4);
  M5.Lcd.drawString(todayStr, 5, 20);

  // 天気予報の描画（3日予報）
  JsonObject weatherData = jsonDoc[days3]["timeSeries"][0];
  String weather = weatherData["areas"][areaIndex_3days]["weathers"][day_index].as<String>();
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

  // 降水確率の描画（3日予報+7日予報）
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.drawString("%", 280, 200);
  bool days3_data_available = false;
  JsonObject rainfallData = jsonDoc[days3]["timeSeries"][1];  // 3日予報の降水確率
  uint32_t rainPred_draw_cnt = 0;
  for (int i = 0; i < rainfallData["timeDefines"].size(); i++) {
    if (rainfallData["timeDefines"][i].as<String>().indexOf(todayStr) != -1) {
      uint32_t rainPred = rainfallData["areas"][areaIndex_3days]["pops"][i].as<int>();
      M5.Lcd.drawString(String(rainPred), 20 + rainPred_draw_cnt * 65, 200);
      M5.Lcd.fillRect(rainPred_draw_cnt * 65, 233, 65, 7, getDrawColorFromRainPred(rainPred));
      rainPred_draw_cnt++;
      days3_data_available = true;
    }
  }
  rainfallData = jsonDoc[days7]["timeSeries"][0];  // 7日予報の降水確率
  rainPred_draw_cnt = 0;
  for (int i = 0; i < rainfallData["timeDefines"].size(); i++) {
    if (rainfallData["timeDefines"][i].as<String>().indexOf(todayStr) != -1 && !days3_data_available) {
      if (rainfallData["areas"][areaIndex_7days]["pops"][i] != "")  // ""なことがある
      {
        uint32_t rainPred = rainfallData["areas"][areaIndex_7days]["pops"][i].as<int>();
        M5.Lcd.drawString(String(rainPred), 20 + rainPred_draw_cnt * 65, 200);
        M5.Lcd.fillRect(rainPred_draw_cnt * 65, 233, 65, 7, getDrawColorFromRainPred(rainPred));
        rainPred_draw_cnt++;
      }
    }
  }

  // 最低・最高気温の描画（3日予報（地区詳細）+7日予報）
  M5.Lcd.setTextSize(3);
  days3_data_available = false;
  uint32_t days3_minTemperature = 999;
  uint32_t days3_maxTemperature = 0;
  JsonObject temperatureData = jsonDoc[days3]["timeSeries"][2];  // 3日予報（地区詳細）の気温
  for (int i = 0; i < temperatureData["timeDefines"].size(); i++) {
    if (temperatureData["timeDefines"][i].as<String>().indexOf(todayStr) != -1) {
      uint32_t val = temperatureData["areas"][areaIndex_3days_city]["temps"][i];
      if (val < days3_minTemperature) days3_minTemperature = val;
      if (val > days3_maxTemperature) days3_maxTemperature = val;
      days3_data_available = true;
    }
  }
  if (days3_data_available) {
    M5.Lcd.setTextColor(TFT_BLUE);
    M5.Lcd.drawString(String(days3_minTemperature), 10, 120);
    M5.Lcd.setTextColor(TFT_RED);
    M5.Lcd.drawString(String(days3_maxTemperature), 137, 121);
  }
  temperatureData = jsonDoc[days7]["timeSeries"][1];  // 7日予報の気温
  for (int i = 0; i < temperatureData["timeDefines"].size(); i++) {
    if (temperatureData["timeDefines"][i].as<String>().indexOf(todayStr) != -1 && !days3_data_available) {
      String minTemperature = temperatureData["areas"][areaIndex_7days_city]["tempsMin"][i];
      String maxTemperature = temperatureData["areas"][areaIndex_7days_city]["tempsMax"][i];
      M5.Lcd.setTextColor(TFT_BLUE);
      M5.Lcd.drawString(minTemperature, 10, 120);
      M5.Lcd.setTextColor(TFT_RED);
      M5.Lcd.drawString(maxTemperature, 137, 121);
    }
  }
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.drawString("`C", 250, 135);
}
// 降水確率を描画する色を決める
uint16_t getDrawColorFromRainPred(uint32_t val) {
  if (val <= 20) return TFT_RED;
  if (val <= 60) return TFT_LIGHTGREY;
  return TFT_BLUE;
}

// 青画面の描画（各種情報表示用）
void drawBlueScreen(String s) {
  M5.Lcd.fillScreen(TFT_BLUE);
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.setTextSize(1);
  M5.Lcd.drawString(s, 0, 10);
}

// ----------------------------------------------------------
// png描画系関数
// https://github.com/kikuchan/pngle
// 標準のArduinoサンプルではなく、TFTサンプルの方を使うこと（無限待ちになる）
// ----------------------------------------------------------
double g_scale = 1.0;
void pngle_on_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]) {
  uint16_t color = (rgba[0] << 8 & 0xf800) | (rgba[1] << 3 & 0x07e0) | (rgba[2] >> 3 & 0x001f);

  if (rgba[3]) {
    x = ceil(x * g_scale);
    y = ceil(y * g_scale);
    w = ceil(w * g_scale);
    h = ceil(h * g_scale);
    M5.Lcd.fillRect(x, y, w, h, color);
  }
}
void load_png(String url, double scale) {
  HTTPClient http;
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    drawBlueScreen("HTTP Error");
    http.end();
    return;
  }

  int total = http.getSize();

  WiFiClient *stream = http.getStreamPtr();

  pngle_t *pngle = pngle_new();
  pngle_set_draw_callback(pngle, pngle_on_draw);
  g_scale = scale;

  uint8_t buf[2048];
  int remain = 0;
  uint32_t timeout = millis();
  while (http.connected() && (total > 0 || remain > 0)) {
    // Break out of loop after 10s
    if ((millis() - timeout) > 10000UL) {
      drawBlueScreen("HTTP Timeout");
      break;
    }

    size_t size = stream->available();
    if (!size) {
      delay(1);
      continue;
    }
    if (size > sizeof(buf) - remain) {
      size = sizeof(buf) - remain;
    }

    int len = stream->readBytes(buf + remain, size);
    if (len > 0) {
      int fed = pngle_feed(pngle, buf, remain + len);
      if (fed < 0) {
        drawBlueScreen("PNGLE Error");
        break;
      }
      total -= len;
      remain = remain + len - fed;
      if (remain > 0) memmove(buf, buf + fed, remain);
    } else {
      delay(1);
    }
  }
  pngle_destroy(pngle);
  http.end();
}