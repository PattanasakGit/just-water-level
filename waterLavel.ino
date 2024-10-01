#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME "WaterLevel"
#define BLYNK_AUTH_TOKEN ""

char ssid[] = "";
char pass[] = "";

int emptyTankDistance = 170;  // ระยะเมื่อถังน้ำเปล่า
int fullTankDistance = 20;    // ระยะเมื่อถังน้ำเต็ม

#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include "LedController.hpp"
#include <BlynkSimpleEsp32.h>

#define TRIGPIN 26
#define ECHOPIN 25
#define wifiLed 2

#define DIN 12
#define CS 13
#define CLK 14

#define VPIN_BUTTON_1 V1
#define VPIN_BUTTON_2 V2

float duration;
float distance;
int waterLevelPer;
bool toggleBuzzer = HIGH;
char auth[] = BLYNK_AUTH_TOKEN;

const char* lineToken = "";
const char* targetId = "";
const char* lineUrl = "https://api.line.me/v2/bot/message/push";

BlynkTimer timer;
LedController<1, 1> lc;

void checkBlynkStatus() {
  bool isconnected = Blynk.connected();
  if (!isconnected) {
    digitalWrite(wifiLed, LOW);
  } else {
    digitalWrite(wifiLed, HIGH);
  }
}

BLYNK_CONNECTED() {
  Blynk.syncVirtual(VPIN_BUTTON_1);
  Blynk.syncVirtual(VPIN_BUTTON_2);
}

void measureDistance() {
  digitalWrite(TRIGPIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGPIN, HIGH);
  delayMicroseconds(20);
  digitalWrite(TRIGPIN, LOW);
  duration = pulseIn(ECHOPIN, HIGH, 30000); // Timeout หลังจาก 30ms

  if (duration == 0) {
    distance = -1; // เซนเซอร์ผิดพลาด
  } else {
    distance = ((duration / 2) * 0.343) / 10;
  }

  if (distance > (fullTankDistance - 10) && distance < emptyTankDistance) {
    if (distance > 100){
      distance = distance + 4;
    };
    waterLevelPer = map((int)distance, emptyTankDistance, fullTankDistance, 0, 100);
    Blynk.virtualWrite(VPIN_BUTTON_1, waterLevelPer);
    Blynk.virtualWrite(VPIN_BUTTON_2, (String(distance) + " cm"));

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.print(" cm");
    Serial.print(" and ");
    Serial.print(waterLevelPer);
    Serial.println(" %");

    checkWaterPerForSendLine();
  } else if (distance == -1) {
    Blynk.virtualWrite(VPIN_BUTTON_2, "Sensor Error");
    Serial.println("Sensor Error");
  }
  delay(100);
}

void sendLineMessage(String dataBody) {
  Serial.println("Sending message to Line...");
  String postData = dataBody;

  HTTPClient http;
  http.begin(lineUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(lineToken));
  int httpCode = http.POST(String(postData));

  if (httpCode == 200) {
    String content = http.getString();
    Serial.println("Content ---------");
    Serial.println(content);
    Serial.println("-----------------");
  } else {
    Serial.println("Fail. error code " + String(httpCode));
  }
  Serial.println("END");
}

unsigned long lastNotificationTime = 0;
unsigned long notificationInterval = 1 * 60 * 1000;

int lastNotifiedWaterLevel = -1;
void checkWaterPerForSendLine() {
  String data70 = "{\"to\":\"" + String(targetId) + "\",\"messages\":[{\"type\":\"text\",\"text\":\"🔔💧\\n ระดับน้ำเหลือ \\n ❗" + String(waterLevelPer) + " % ❗\\n อาจลืมเปิดน้ำขึ้นแทงค์ \"}]}";
  String data50 = "{\"to\":\"" + String(targetId) + "\",\"messages\":[{\"type\":\"text\",\"text\":\"🔔💧⚠️\\n ระดับน้ำเหลือ \\n ❗" + String(waterLevelPer) + " % ❗\\n โปรดตรวจสอบน้ำในแทงค์ \\n อาจลืมเปิดน้ำขึ้นแทงค์ \"}]}";
  String data40 = "{\"to\":\"" + String(targetId) + "\",\"messages\":[{\"type\":\"text\",\"text\":\"❌💧❌\\n ระดับน้ำเหลือ \\n❗" + String(waterLevelPer) + " % ❗\\n โปรดตรวจสอบน้ำกำลังจะหมดแทงค์แล้ว\"}]}";

  unsigned long currentTime = millis();

  if ((waterLevelPer == 50) && 
      (currentTime - lastNotificationTime >= notificationInterval) && 
      (lastNotifiedWaterLevel != waterLevelPer)) {
    sendLineMessage(data50);
    lastNotificationTime = currentTime;
    lastNotifiedWaterLevel = waterLevelPer;
  }
  if ((waterLevelPer == 40) && 
      (currentTime - lastNotificationTime >= notificationInterval) && 
      (lastNotifiedWaterLevel != waterLevelPer)) {
    sendLineMessage(data40);
    lastNotificationTime = currentTime;
    lastNotifiedWaterLevel = waterLevelPer;
  }
  if ((waterLevelPer == 70 ) && 
      (currentTime - lastNotificationTime >= notificationInterval) && 
      (lastNotifiedWaterLevel != waterLevelPer)) {
    sendLineMessage(data70);
    lastNotificationTime = currentTime;
    lastNotifiedWaterLevel = waterLevelPer;
  }
}

void displayNumberOn7Segment(int number) {
  lc.setIntensity(8);
  lc.clearMatrix();

  if ( (number >= 0) && (distance != -1) ) {
    lc.setChar(0, 0, '-', false);
    lc.setDigit(0, 1, number % 10, false);
    lc.setDigit(0, 2, (number / 10) % 10, false);

    if (number >= 100) {
      lc.setChar(0, 4, '-', false);
      lc.setDigit(0, 3, (number / 100) % 10, false);
    } else {
      lc.setChar(0, 3, '-', false);
    }
  } else {
    lc.setChar(0, 0, 'r', false);
    lc.setChar(0, 1, 'o', false);
    lc.setChar(0, 2, 'r', false);
    lc.setChar(0, 3, 'r', false);
    lc.setChar(0, 4, 'E', false);
  }

  if (digitalRead(wifiLed) == HIGH) {
    lc.setDigit(0, 7, 1, false);
  } else {
    lc.setDigit(0, 7, 0, false);
  }

  delay(250);
}

void setup() {
  Serial.begin(115200);

  pinMode(ECHOPIN, INPUT);
  pinMode(TRIGPIN, OUTPUT);
  pinMode(wifiLed, OUTPUT);

  digitalWrite(wifiLed, LOW);

  WiFi.begin(ssid, pass);
  timer.setInterval(2000L, checkBlynkStatus);
  timer.setInterval(1000L, []() { displayNumberOn7Segment(waterLevelPer); }); 
  Blynk.config(auth);
  delay(1000);

  lc = LedController<1, 1>(DIN, CLK, CS);
  lc.setIntensity(8);
  lc.clearMatrix();
}

void loop() {
  measureDistance();
  Blynk.run();
  timer.run();
}
