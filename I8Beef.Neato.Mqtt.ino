#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <FS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>

#define SSID_FILE "etc/ssid"
#define PASSWORD_FILE "etc/pass"
#define SERIAL_FILE "etc/serial"
#define MQTT_HOST_FILE "etc/mqttHost"
#define MQTT_USER_FILE "etc/mqttUser"
#define MQTT_PASSWORD_FILE "etc/mqttPass"
#define MQTT_TOPIC_BASE "neato/botvac/"

#define CONNECT_TIMEOUT_SECS 30
#define SERIAL_NUMBER_ATTEMPTS 5

#define AP_SSID "neato"

#define FIRMWARE "1.7"

#define MAX_BUFFER 8192

String serialNumber = "Empty";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

ESP8266WebServer server(80);
ESP8266WebServer updateServer(81);
ESP8266HTTPUpdateServer httpUpdater;

String getSerial() {
  String serial;
  File serial_file = SPIFFS.open(SERIAL_FILE, "r");
  if(!serial_file || serial_file.readString() == "" ) {
    serial_file.close();
    Serial.setTimeout(250);
    String incomingSerial;
    int serialString;
    
    for (int i = 0; i <= SERIAL_NUMBER_ATTEMPTS; i++) {
      Serial.println("GetVersion");
      incomingSerial = Serial.readString();
      serialString = incomingSerial.indexOf("Serial Number");
      if (serialString > -1)
        break;
      delay(50);
    }

    if (serialString == -1) {
      serial = "Empty";
    } else {
      int capUntil = serialString + 50;
      String serialCap = incomingSerial.substring(serialString, capUntil);
      int commaIndex = serialCap.indexOf(',');
      int secondCommaIndex = serialCap.indexOf(',', commaIndex + 1);
      int thirdCommaIndex = serialCap.indexOf(',', secondCommaIndex + 1);
      String incomingSerialCheck = serialCap.substring(secondCommaIndex + 1, thirdCommaIndex); // To the end of the string
      
      if (incomingSerialCheck.indexOf("Welcome") > -1) {
        ESP.reset();
      } else {
        writeConfigFile(SERIAL_FILE, incomingSerialCheck);
        serial = incomingSerialCheck;
      }
    }
  } else {
    serial_file.seek(0, SeekSet);
    serial = serial_file.readString();
    serial_file.close();
  }

  return serial;
}

String readConfigFile(char* path) {
  String result = "XXXX";
  File file = SPIFFS.open(path, "r");
  if (file) {
    result = file.readString();
    file.close();
  }

  return result;
}

bool writeConfigFile(char* path, String content) {
  File file = SPIFFS.open(path, "w");
  if (!file) {
    return false;
  }

  file.print(content);
  file.close();
  return true;
}

void setupRequestHandler() {
  String user_ssid = readConfigFile(SSID_FILE);
  String user_password = readConfigFile(PASSWORD_FILE);

  String mqtt_host = readConfigFile(MQTT_HOST_FILE);
  String mqtt_user = readConfigFile(MQTT_USER_FILE);
  String mqtt_password = readConfigFile(MQTT_PASSWORD_FILE);

  server.send(200, "text/html", String() + 
  "<!DOCTYPE html><html> <body>" +
  "<p>Neato serial number: <b>" + serialNumber + "</b></p>" +
  "<form action=\"\" method=\"post\" style=\"display: inline;\">" +
  "Access Point SSID:<br />" +
  "<input type=\"text\" name=\"ssid\" value=\"" + user_ssid + "\"> <br />" +
  "WPA2 Password:<br />" +
  "<input type=\"text\" name=\"password\" value=\"" + user_password + "\"> <br />" +
  "<br />" +
  "MQTT Host:<br />" +
  "<input type=\"text\" name=\"mqttHost\" value=\"" + mqtt_host + "\"> <br />" +
  "MQTT User:<br />" +
  "<input type=\"text\" name=\"mqttUser\" value=\"" + mqtt_user + "\"> <br />" +
  "MQTT Password:<br />" +
  "<input type=\"text\" name=\"mqttPassword\" value=\"" + mqtt_password + "\"> <br />" +
  "<br />" +
  "<input type=\"submit\" value=\"Submit\"> </form>" +
  "<form action=\"http://neato.local/reboot\" style=\"display: inline;\">" +
  "<input type=\"submit\" value=\"Reboot\" />" +
  "</form>" +
  "<p>Enter the details for your access point. After you submit, the controller will reboot to apply the settings.</p>" +
  "<p><a href=\"http://neato.local:81/update\">Update Firmware</a></p>" +
  "<p><a href=\"https://www.neatorobotics.com/resources/programmersmanual_20140305.pdf\">Command Documentation</a></p>" +
  "</body></html>\n");
}

void saveRequestHandler() {
  String user_ssid = server.arg("ssid");
  String user_password = server.arg("password");

  String mqtt_host = server.arg("mqttHost");
  String mqtt_user = server.arg("mqttUser");
  String mqtt_password = server.arg("mqttPassword");

  SPIFFS.format();

  if (user_ssid != "" && user_password != "") {
    if (!writeConfigFile(SSID_FILE, user_ssid)) {
      server.send(200, "text/html", "<!DOCTYPE html><html> <body> Setting Access Point SSID failed!</body> </html>");
      return;
    }

    if (!writeConfigFile(PASSWORD_FILE, user_password)) {
      server.send(200, "text/html", "<!DOCTYPE html><html> <body> Setting Access Point password failed!</body> </html>");
      return;
    }
  }

  if (mqtt_host != "") {
    if (!writeConfigFile(MQTT_HOST_FILE, mqtt_host)) {
      server.send(200, "text/html", "<!DOCTYPE html><html> <body> Setting MQTT host failed!</body> </html>");
      return;
    }
  }

  if (mqtt_user != "" && mqtt_password != "") {
    if (!writeConfigFile(MQTT_USER_FILE, mqtt_user)) {
      server.send(200, "text/html", "<!DOCTYPE html><html> <body> Setting MQTT user failed!</body> </html>");
      return;
    }

    if (!writeConfigFile(MQTT_PASSWORD_FILE, mqtt_password)) {
      server.send(200, "text/html", "<!DOCTYPE html><html> <body> Setting MQTT password failed!</body> </html>");
      return;
    }
  }

  server.send(200, "text/html", String() + 
  "<!DOCTYPE html><html> <body>" +
  "Setting Access Point SSID / password was successful! <br />" +
  "<br />SSID was set to \"" + user_ssid + "\" with the password \"" + user_password + "\". <br />" +
  "<br />MQTT host was set to \"" + mqtt_host + "\" with the username \"" + mqtt_user + "\" and  password \"" + mqtt_password + "\". <br />" +
  "<br /> The controller will now reboot. Please re-connect to your Wi-Fi network.<br />" +
  "If the SSID or password was incorrect, the controller will return to Access Point mode." +
  "</body> </html>");

  ESP.reset();
}

void rebootRequestHandler() {
  server.send(200, "text/html", String() + 
  "<!DOCTYPE html><html> <body>" +
  "The controller will now reboot.<br />" +
  "If the SSID or password is set but is incorrect, the controller will return to Access Point mode." +
  "</body> </html>");

  ESP.reset();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.setTimeout(500);
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  
  Serial.println();

  // Convert serial String to char array
  String result = Serial.readString();
  char resultChar[result.length() + 1];
  result.toCharArray(resultChar, sizeof(resultChar));
  resultChar[sizeof(resultChar) - 1] = '\0';

  // Pull response topic from the incoming topic
  char publishTopicChar[strlen(topic) - 4 + 1];
  strncpy(publishTopicChar, topic, strlen(topic) - 4);
  publishTopicChar[sizeof(publishTopicChar) - 1] = '\0';

  mqttClient.publish(publishTopicChar, resultChar);
}

void mqttReconnect() {
  if (!mqttClient.connected()) {
    String subTopic = MQTT_TOPIC_BASE + serialNumber + "/+/set";
    char subTopicChar[subTopic.length() + 1];
    subTopic.toCharArray(subTopicChar, sizeof(subTopicChar));
    subTopicChar[sizeof(subTopicChar) - 1] = '\0';

    if (SPIFFS.exists(MQTT_USER_FILE) && SPIFFS.exists(MQTT_PASSWORD_FILE)) {
      char mqttUser[64];
      readConfigFile(MQTT_USER_FILE).toCharArray(mqttUser, 64);

      char mqttPass[64];
      readConfigFile(MQTT_PASSWORD_FILE).toCharArray(mqttPass, 64);

      if (mqttClient.connect("ESP8266Client", mqttUser, mqttPass)) {
        mqttClient.subscribe(subTopicChar);
      }
    } else {
      if (mqttClient.connect("ESP8266Client")) {
        mqttClient.subscribe(subTopicChar);
      }
    }
  }
}

void setup() {
  // start serial
  // botvac serial console is 115200 baud, 8 data bits, no parity, one stop bit (8N1)
  Serial.begin(115200);
  
  // try to mount the filesystem. if that fails, format the filesystem and try again.
  if (!SPIFFS.begin()) {
    SPIFFS.format();
    SPIFFS.begin();
  }
    
  serialNumber = getSerial();
  
  // read cached Wifi connection info
  if (SPIFFS.exists(SSID_FILE) && SPIFFS.exists(PASSWORD_FILE)) {
    char wifiSsid[64];
    readConfigFile(SSID_FILE).toCharArray(wifiSsid, 64);
    char wifiPasswd[64];
    readConfigFile(PASSWORD_FILE).toCharArray(wifiPasswd, 64);

    // attempt station connection
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSsid, wifiPasswd);

    for (int i = 0; i < CONNECT_TIMEOUT_SECS * 20 && WiFi.status() != WL_CONNECTED; i++) {
      delay(50);
    }
  }

  // start AP mode if either the AP / password do not exist, or cannot be connected to within CONNECT_TIMEOUT_SECS seconds.
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);

    if (!WiFi.softAP(AP_SSID)) {
      // reset because there's no good reason for setting up an AP to fail
      ESP.reset();
    }
  }

  // OTA update hooks
  ArduinoOTA.onStart([]() { SPIFFS.end(); });
  ArduinoOTA.onEnd([]() { SPIFFS.begin(); });

  ArduinoOTA.begin();
  httpUpdater.setup(&updateServer);
  updateServer.begin();

  // start webserver
  server.on("/", HTTP_GET, setupRequestHandler);
  server.on("/", HTTP_POST, saveRequestHandler);
  server.on("/reboot", HTTP_GET, rebootRequestHandler);
  
  server.onNotFound(setupRequestHandler);
  server.begin();

  // start MDNS
  // this means that the botvac can be reached at http://neato.local or ws://neato.local:81
  if (!MDNS.begin("neato")) {
    // reset because there's no good reason for setting up MDNS to fail
    ESP.reset();
  }

  MDNS.addService("http", "tcp", 80);
  MDNS.addService("http", "tcp", 81);

  // MQTT Client
  if (SPIFFS.exists(MQTT_HOST_FILE)) {
    char mqttHost[64];
    readConfigFile(MQTT_HOST_FILE).toCharArray(mqttHost, 64);

    mqttClient.setServer(mqttHost, 1883);
    mqttClient.setCallback(mqttCallback);
  }
}

void loop() {
  // Web server
  server.handleClient();
  
  // Update server
  ArduinoOTA.handle();
  updateServer.handleClient();

  mqttReconnect();
  mqttClient.loop();
}
