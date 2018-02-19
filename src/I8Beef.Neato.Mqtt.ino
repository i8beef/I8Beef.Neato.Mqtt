#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

// Update with suitable values
#define MAX_BUFFER 8192
#define SSID ""
#define SSID_PASSWORD ""
#define MQTT_SERVER ""
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "ESP8266"
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_TOPIC_LAST_SERIAL "neato/botvac/default/serial/lastResponse"
#define MQTT_TOPIC_SUBSCRIBE "neato/botvac/default/serial/set"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Web server
ESP8266WebServer webServer(80);

int bufferLocation = 0;
uint8_t serialBuffer[8193];

void serialEvent() {
  while (Serial.available() > 0) {
    char in = Serial.read();

    // there is no propper utf-8 support so replace all non-ascii 
    // characters (<127) with underscores; this should have no 
    // impact on normal operations and is only relevant for non-english 
    // plain-text error messages
    if (in > 127) {
      in = '_';
    }
    
    // fill up the serial buffer until its max size (8192 bytes, see maxBuffer) 
    // or unitl the end of file marker (ctrl-z; \x1A) is reached
    // a worst caste lidar result should be just under 8k, so that maxBuffer 
    // limit should not be reached under normal conditions
    if (bufferLocation > MAX_BUFFER || in == '\x1A') {
      // Strip off CRLF
      if (bufferLocation >= 2) {
        bufferLocation = bufferLocation - 2;
        serialBuffer[bufferLocation] = '\0';
      }

      if (bufferLocation != 0) {
        mqttClient.publish(MQTT_TOPIC_LAST_SERIAL, (char*)serialBuffer);
      }

      bufferLocation = 0;
      serialBuffer[bufferLocation] = '\0';
    } else {
      serialBuffer[bufferLocation] = in;
      bufferLocation++;
      serialBuffer[bufferLocation] = '\0';
    }
  }
}

void statusHandler() {
  webServer.send(200, "text/html", "<!DOCTYPE html><html><body>Active</body></html>");
}

void setup() {
  // start serial
  // botvac serial console is 115200 baud, 8 data bits, no parity, one stop bit (8N1)
  Serial.begin(115200);
  
  setup_wifi();

  // start webserver
  webServer.on("/", HTTP_GET, statusHandler);
  webServer.begin();
  
  // OTA updates
  ArduinoOTA.begin();

  // MQTT client
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

void setup_wifi() {
  // We start by connecting to a WiFi network
  WiFi.begin(SSID, SSID_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  
  Serial.println();
}

void mqttReconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    // Attempt to connect, remove user / pass if not needed
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      mqttClient.subscribe(MQTT_TOPIC_SUBSCRIBE);
    } else {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  // OTA Update
  ArduinoOTA.handle();

  // Web server
  webServer.handleClient();
  
  // MQTT
  if (!mqttClient.connected()) {
    mqttReconnect();
  }

  mqttClient.loop();

  // Serial
  serialEvent();
}
