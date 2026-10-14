#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <MHZ19.h>
#include "secrets.h"

const char* WIFI_SSID  = WIFI_SSID_VALUE;
const char* WIFI_PASS  = WIFI_PASS_VALUE;
const char* SERVER_URL = SERVER_URL_VALUE;
const char* DEVICE_KEY = DEVICE_KEY_VALUE;
const unsigned long SEND_INTERVAL_MS = 60UL * 1000;

DHT dht(4, DHT11);
MHZ19 mhz;
HardwareSerial co2Serial(1);

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("WiFi 연결 중");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);   // 슈퍼미니 연결 안정화용
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("\n연결됨! IP: %s\n", WiFi.localIP().toString().c_str());
  else
    Serial.println("\n연결 실패 → 다음 주기에 재시도");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  dht.begin();
  co2Serial.begin(9600, SERIAL_8N1, 7, 6);
  mhz.begin(co2Serial);
  mhz.autoCalibration(false);
  connectWiFi();
}

void loop() {
  connectWiFi();

  int co2 = mhz.getCO2();
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  Serial.printf("CO2: %d | 온도: %.0f | 습도: %.0f\n", co2, t, h);

  if (millis() < 60000) {
    Serial.println("CO2 예열 중 → 전송 안 함");
  } else if (co2 <= 0) {
    Serial.println("CO2 값 이상 → 전송 안 함");
  } else if (WiFi.status() == WL_CONNECTED) {
    // DHT 실패 시 null로 보내서 CO2는 살림
    char tBuf[8], hBuf[8], body[96];
    if (isnan(t)) strcpy(tBuf, "null"); else snprintf(tBuf, sizeof tBuf, "%.0f", t);
    if (isnan(h)) strcpy(hBuf, "null"); else snprintf(hBuf, sizeof hBuf, "%.0f", h);
    snprintf(body, sizeof body, "{\"co2\":%d,\"temp\":%s,\"humidity\":%s}", co2, tBuf, hBuf);

    HTTPClient http;
    http.begin(SERVER_URL);
    http.setTimeout(5000);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Key", DEVICE_KEY);
    int code = http.POST(body);
    Serial.printf("전송 %s → 응답 %d\n", body, code);
    http.end();
  }

  delay(SEND_INTERVAL_MS);
}