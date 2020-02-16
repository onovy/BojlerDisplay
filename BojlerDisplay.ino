#include <TFT_eSPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <math.h>

#include "config.h"

#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN   0x10
#endif

#define TFT_MOSI            19
#define TFT_SCLK            18
#define TFT_CS              5
#define TFT_DC              16
#define TFT_RST             23

#define TFT_BL          4  // Display backlight control pin
#define ADC_EN          14
#define ADC_PIN         34
#define BUTTON_1        35
#define BUTTON_2        0

WiFiClient espClient;
PubSubClient client(espClient);

bool relay_state = false;
double temp1 = 0;
double temp2 = 0;
int64_t last_update = 0;

TFT_eSPI tft = TFT_eSPI(135, 240);

void setup_wifi() {
  delay(10);
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, 0);

  tft.print("Wifi: ");

  WiFi.begin(ssid, password);
  int c = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
    if (c++ >= 60) {
      ESP.restart();
    }
  }
  tft.println(" OK");
  tft.print("IP:  ");
  tft.println(WiFi.localIP());
  tft.print("GW:  ");
  tft.println(WiFi.gatewayIP());
  tft.print("DNS: ");
  tft.println(WiFi.dnsIP());
  delay(1000);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Start");
  tft.init();
  tft.setRotation(3);
  tft.setTextDatum(TL_DATUM);

  pinMode(TFT_BL, OUTPUT); // Set backlight pin to output mode
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on. TFT_BACKLIGHT_ON has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, led_backlight);

  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);

  setup_wifi();
  last_update = esp_timer_get_time();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String msg;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    msg += (char)payload[i];
  }
  Serial.println();

  if (strcmp(mqtt_topic_temp, topic) == 0) {
    // Temperature
    StaticJsonDocument<JSON_OBJECT_SIZE(20) + 220> doc;
    DeserializationError error = deserializeJson(doc, msg);

    if (error) {
      temp1 = NAN;
      temp2 = NAN;
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
    } else {
      if (doc.containsKey(mqtt_temp_key1)) {
        temp1 = doc[mqtt_temp_key1].as<double>();
      } else {
        temp1 = NAN;
      }
      if (doc.containsKey(mqtt_temp_key2)) {
        temp2 = doc[mqtt_temp_key2].as<double>();
      } else {
        temp2 = NAN;
      }
      last_update = esp_timer_get_time();
    }
  } else if (strcmp(mqtt_topic_relay, topic) == 0) {
    // Relay
    relay_state = (msg == "on");
  }

  draw();
}

void draw() {
  tft.fillScreen(relay_state ? TFT_RED : TFT_BLACK);
  tft.setTextFont(0);

  tft.setCursor(0, 24);
  tft.setTextColor(TFT_YELLOW);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.println(mqtt_temp_title1);

  tft.setCursor(0, 62);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeMonoBold12pt7b);
  if (isnan(temp1)) {
    tft.print("?");
  } else {
    tft.print(temp1);
  }
  tft.println(" C");

  tft.setCursor(0, 95);
  tft.setTextColor(TFT_YELLOW);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.println(mqtt_temp_title2);

  tft.setCursor(0, 133);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeMonoBold12pt7b);
  if (isnan(temp2)) {
    tft.print("?");
  } else {
    tft.print(temp2);
  }
  tft.println(" C");
}

void reconnect() {
  int c = 0;
  while (!client.connected()) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextFont(1);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(0, 0);

    tft.print("MQTT: ");
    if (client.connect(mqtt_client, mqtt_user, mqtt_pass)) {
      tft.println("OK");
      delay(1000);
      client.subscribe(mqtt_topic_temp);
      client.subscribe(mqtt_topic_relay);
    } else {
      tft.setTextColor(TFT_RED);
      tft.print("ERROR ");
      tft.println(client.state());
      delay(1000);
    }

    if (c++ >= 60) {
      ESP.restart();
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if ((esp_timer_get_time() - last_update) > 120 * 1000000) {
    ESP.restart();
  }
}
