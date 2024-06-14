#include <Arduino.h>
#include <Wire.h>
// https://github.com/olikraus/u8g2/wiki
#include <U8g2lib.h>
#include "WiFi.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "WiFiPassword.h"

U8G2_SSD1306_128X64_NONAME_F_SW_I2C
u8g2(U8G2_R0, 22, 21, U8X8_PIN_NONE);

const String url = "https://api.bart.gov/api/etd.aspx?cmd=etd&orig=24th&key=MW9S-E7SL-26DU-VV8V&json=y";

HTTPClient http;

char buffer[256];

void setup(void) {
  Serial.begin(115200); 

  u8g2.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.setDrawColor(1);
  u8g2.setFontMode(0);

  Serial.print("Connecting to WiFi...");

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");

    sprintf(buffer, "Connecting: %d", ++counter);
    u8g2.drawStr(0, 5, buffer);
    u8g2.sendBuffer();
  }
  Serial.println();

  sprintf(buffer, "Connected to: %s", WIFI_SSID);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 20, buffer);
  u8g2.sendBuffer();

  delay(1500);
}

void loop(void) {
  Serial.print("Connecting to ");
  Serial.println(url);

  http.begin(url);
  int httpCode = http.GET();
  StaticJsonDocument<2000> doc;
  DeserializationError error = deserializeJson(doc, http.getString());

  http.end();

  if (error)
  {
    Serial.print(F("deserializeJson Failed"));
    Serial.println(error.f_str());
    delay(2500);
    return;
  }

  Serial.print("HTTP Status Code: ");
  Serial.println(httpCode);

  u8g2.clearBuffer();

  JsonArray estimationsArray = doc["root"]["station"][0]["etd"].as<JsonArray>();
  int destCount = 0;
  char destNames[10][4] = {"", "", "", "", "", "", "", "", "", ""};
  int estimationCounts[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  int estimations[10][10];

  for (JsonVariant estimation : estimationsArray) {
    String abbr = estimation["abbreviation"].as<String>().substring(0, 3);

    Serial.println("Encountered abbr: " + abbr);

    int destNum = -1;
    for (int i = 0; i < destCount; i++) {
      if (0 == abbr.compareTo(destNames[i])) {
        destNum = i;
        Serial.print("Found existing destNames item: ");
        Serial.println(i);
        break;
      }
    }

    if (-1 == destNum) {
      Serial.println("Didn't find an existing destNames item. Creating it...");

      destNum = destCount++;
      sprintf(destNames[destNum], "%s", abbr);
      estimationCounts[destNum] = 0;
    }

    for (JsonVariant estimate : estimation["estimate"].as<JsonArray>()) {
      int estimationMinutes = estimate["minutes"].as<int>();

      // Maintain sorted array in ascending order. Find insertion point
      int curSize = estimationCounts[destNum];

      int insertionPoint = 0;
      while (insertionPoint < curSize && estimations[destNum][insertionPoint] <= estimationMinutes) {
        insertionPoint++;
      }

      int val = estimationMinutes;
      for (int i = insertionPoint; i < curSize + 1; i++) {
        int oldVal = estimations[destNum][i];
        estimations[destNum][i] = val;
        val = oldVal;
      }

      estimationCounts[destNum]++;
    }
  }

  char estimationChars[4] = "";
  for (int destNum = 0; destNum < destCount; destNum++) {
    int y = (destNum % 4 + 1) * 15;
    int x = (destNum / 4) * 64;

    u8g2.drawStr(x, y, destNames[destNum]);
    Serial.print(destNames[destNum]);
    Serial.print("");

    x += 24;

    int estimationsCount = estimationCounts[destNum];

    for (int i = 0; i < estimationsCount && i < 2; i++) {
      sprintf(estimationChars, "%d", estimations[destNum][i]);
      Serial.print(estimationChars);
      Serial.print(" ");

      u8g2.drawStr(x, y, estimationChars);

      x += 19;
    }

    Serial.println();
  }

  u8g2.drawVLine(22, 0, 100);
  u8g2.drawVLine(62, 0, 100);
  u8g2.drawVLine(86, 0, 100);

  u8g2.sendBuffer();

  delay(30000);
}