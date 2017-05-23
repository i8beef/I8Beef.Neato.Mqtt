#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Update with suitable values
const char* SSID = "........";
const char* SSID_PASSWORD = "........";
const char* MQTT_SERVER = "192.168.1.152";
const char* MQTT_USER = "";
const char* MQTT_PASSWORD = "";
const char* MQTT_TOPIC = "neato/botvac/default";

WiFiClient espClient;
PubSubClient client(espClient);

int maxBuffer = 8192;
int bufferSize = 0;
uint8_t serialBuffer[8193];

void botDissconect() {
  // always disable testmode on disconnect
  Serial.println("TestMode off");
}

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

    serialBuffer[bufferSize] = in;
    bufferSize++;

    // fill up the serial buffer until its max size (8192 bytes, see maxBuffer) 
    // or unitl the end of file marker (ctrl-z; \x1A) is reached
    // a worst caste lidar result should be just under 8k, so that maxBuffer 
    // limit should not be reached under normal conditions
    if (bufferSize > maxBuffer - 1 || in == '\x1A') {
      // Publish message on serial output
      client.publish(strcat(MQTT_TOPIC, "/serial/lastResponse"), serialBuffer);

      serialBuffer[0] = '\0';
      bufferSize = 0;
    }
  }
}

void setup() {
  // start serial
  // botvac serial console is 115200 baud, 8 data bits, no parity, one stop bit (8N1)
  Serial.begin(115200);
  
  setup_wifi();
  
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);
}

void setup_wifi() {
  // We start by connecting to a WiFi network
  WiFi.begin(SSID, SSID_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  // TODO: Add per topic commands
  switch (topic) {
    default:
      // Dangerous for obvious reasons, assumes complete exposure of all commands
      Serial.printf("%s\n", payload);
      break;
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    // Attempt to connect, remove user / pass if not needed
    if (client.connect("ESP8266Client", MQTT_USER, MQTT_PASSWORD)) {
      client.subscribe(strcat(MQTT_TOPIC, "/#/set"));
    } else {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  client.loop();
  serialEvent();
}