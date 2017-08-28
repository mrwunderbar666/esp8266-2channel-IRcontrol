/*
   IRremoteESP8266:
   IR MQTT Client
   29 Jul 2017

   Revision:
   AC Control:
   - 2 Channel IR
   - BME280 Sensor
   - MQTT & HTTP Control
   - OTA Update
   25 Aug 2017
*/
#ifndef UNIT_TEST
#include <Arduino.h>
#endif

#include "acircommands.h"
#include "credentials.h" // Create your own credentials.h file and insert your WiFi Info there

// ESP8266 Libraries
#include <ESP8266WiFi.h>
// SPIFFS
#include <string.h>
#include "FS.h"

// Webserver
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// OTA
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// IR Libraries
#include <IRremoteESP8266.h>
#include <IRsend.h>

// Wifi
#include <WiFiClient.h>

// MQTT Nasty Hack in PubSubClient.h
// #define MQTT_MAX_PACKET_SIZE 256
#include <PubSubClient.h>

// Time
#include <Time.h>
#include <TimeAlarms.h>

// BME280 Sensor Libraries
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

//JSON Library
#include <ArduinoJson.h>

#define LED     D0        // Led in NodeMCU at pin GPIO16 (D0).
#define ir_channel0 D2
#define ir_channel1 D6

//Voltage & Battery Timer

//ADC_MODE(ADC_VCC);
unsigned long previousMillis = 0; // last time update
long battery_interval = 60000; // interval at which to do something (milliseconds)
char battery_status[256];

// BME Settings, Monitoring Sensor Interval
unsigned long bme_previousMillis = 0;
long bme_interval = 60000;

// BME Readings Topic Set here
const char* bme_topic = "/bedroom/climate";

// Wifi Settings & OTA Host

const char* ssid = my_ssid;
const char* password = my_password;
const char* DEVICE_NAME = "ESP8266-Bedroom";

// OTA Flag default is false
bool ota_flag = false;
char ota_publish[256];
// char ota_topic[256];
char ota_payload[256];
unsigned long ota_flag_set_Millis = 0;
unsigned long ota_flag_currentMillis = 0;
long ota_flag_interval = 5 * 60 * 1000; // Interval for OTA Flag to be active

// Server Settings
MDNSResponder mdns;

ESP8266WebServer server(80);

// espClient MQTT Settings & JSON Settings

const char* mqtt_server = "192.168.86.105";
// char* topic = "/bedroom/ac";     //  using wildcard to monitor all traffic from mqtt server
char* topic = "/";

// Topic for this device
String string_device_topic = "/devices/" + String(DEVICE_NAME);
String string_device_command_topic = string_device_topic + "/command";
char device_topic[256];
const char* all_devices_topic = "/devices";
char device_command_topic[256];

char message_buff[100];   // initialise storage buffer (i haven't tested to this capacity.)

// IR MQTT Topics
char temp_topic[256];
String string_channel0_return_topic = "/bedroom/ac/0/status";
String string_channel1_return_topic = "/bedroom/ac/1/status";


// IR Definitions

IRsend irsend_channel0(ir_channel0);  // An IR LED is controlled by GPIO pin 4 (D2)
IRsend irsend_channel1(ir_channel1);

/* **************************************************************
   Convert String to unsigned long
*/
unsigned long StrToUL(String inputString)
{
  unsigned long result = 0;
  for (int i = 0; i < inputString.length(); i++)
  {
    char c = inputString.charAt(i);
    if (c < '0' || c > '9') break;
    result *= 10;
    result += (c - '0');
  }
  return result;
}

// MQTT Setup

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);

// Temperature BME280 Variables
Adafruit_BME280 bme; // I2C

float h, t, p;
char temperatureCString[6];
char humidityString[6];
char pressureString[7];

// Read Sensor Data Function
void getBME280() {
  unsigned long bme_currentMillis = millis();
  if (bme_currentMillis - bme_previousMillis > bme_interval) {
    bme_previousMillis = bme_currentMillis;

    h = bme.readHumidity();
    t = bme.readTemperature();
    p = bme.readPressure()/100.0F;

    dtostrf(t, 5, 1, temperatureCString);
    dtostrf(h, 5, 1, humidityString);
    dtostrf(p, 6, 1, pressureString);
    
    Serial.println("Temperature: ");
    Serial.println(temperatureCString);
    Serial.println("Humidity: ");
    Serial.println(humidityString);
    Serial.println("Pressure: ");
    Serial.println(pressureString);
    Serial.println("Publishing on MQTT...");

    // JSON Variables
    DynamicJsonBuffer jsonBuffer;
    JsonObject& bme_data = jsonBuffer.createObject();
    
    bme_data["temperature"] = String(t);
    bme_data["humidity"] = String(h);
    bme_data["pressure"] = String(p);
    char temp_buffer[256];
    bme_data.printTo(temp_buffer, sizeof(temp_buffer));
    
    Serial.println("Topic: ");
    Serial.println(bme_topic);
    Serial.println("Payload: ");
    Serial.println(temp_buffer);
    Serial.println("Payload Size: ");
    Serial.println(sizeof(temp_buffer));
    client.publish(bme_topic, temp_buffer);
    delay(10);
  }
}

void JSONsendMeasures() {
    // JSON Variables
    DynamicJsonBuffer jsonBuffer;
    JsonObject& bme_data = jsonBuffer.createObject();
    
    bme_data["temperature"] = String(t);
    bme_data["humidity"] = String(h);
    bme_data["pressure"] = String(p);
    char temp_buffer[256];
    bme_data.printTo(temp_buffer, sizeof(temp_buffer));

    server.send(200, "application/json", temp_buffer);
    Serial.println("Sending measures via JSON");
}

void JSONsendConfig() {
        String local_ip =  WiFi.localIP().toString();

        // JSON Variables
        DynamicJsonBuffer jsonBuffer;
        JsonObject& device_json = jsonBuffer.createObject();
        
        device_json["device_name"] = DEVICE_NAME;
        device_json["ip_address"] = local_ip;
        device_json["update_mode"] = String(ota_flag);
        device_json["device_topic"] = string_device_topic;
        char temp_buffer[256];
        device_json.printTo(temp_buffer, sizeof(temp_buffer));
        
        Serial.println("Payload: ");
        Serial.println(temp_buffer);    
        Serial.println("Payload Size: ");
        Serial.println(sizeof(temp_buffer));
        client.publish(all_devices_topic, temp_buffer);
        server.send(200, "application/json", temp_buffer);
}

void JSONsendSPIFFS() {
    //if (spiffsActive) {
    FSInfo fs_info;
    SPIFFS.info(fs_info);
    Serial.print("Total Bytes: ");
    Serial.println(fs_info.totalBytes);
    Serial.print("Used Bytes: ");
    Serial.println(fs_info.usedBytes);
    Serial.print("Block Size: ");
    Serial.println(fs_info.blockSize);
    Serial.print("Page Size: ");
    Serial.println(fs_info.pageSize);
    Serial.print("Max Open Files: ");
    Serial.println(fs_info.maxOpenFiles);
    Serial.print("Max Path Length: ");
    Serial.println(fs_info.maxPathLength);

    // JSON Variables
    DynamicJsonBuffer jsonBuffer;
    JsonObject& spiffs_json = jsonBuffer.createObject();
        
    spiffs_json["total_bytes"] = String(fs_info.totalBytes);
    spiffs_json["used_bytes"] = String(fs_info.usedBytes);
    spiffs_json["block_size"] = String(fs_info.blockSize);
    spiffs_json["page_size"] = String(fs_info.pageSize);
    spiffs_json["max_open_files"] = String(fs_info.maxOpenFiles);
    spiffs_json["max_path_length"] = String(fs_info.maxPathLength);
    
    char temp_buffer[256];
    spiffs_json.printTo(temp_buffer, sizeof(temp_buffer));
    
    Serial.println("Payload: ");
    Serial.println(temp_buffer);    
    Serial.println("Payload Size: ");
    Serial.println(sizeof(temp_buffer));
    client.publish(device_topic, temp_buffer);
    server.send(200, "application/json", temp_buffer);
//}
}

// Webserver

void handleIr() {
  String channel = server.arg("channel");
  String string_code = server.arg("code");
  if (channel == "0") {
    for (uint8_t i = 0; i < server.args(); i++) {
      if (server.argName(i) == "code") {
        uint32_t code = strtoul(server.arg(i).c_str(), NULL, 10);
        irsend_channel0.sendNEC(code, 32);
        //temp_topic = channel0_return_topic;
        string_channel0_return_topic.toCharArray(temp_topic, 80);

        digitalWrite(LED, LOW);
        delay(10);
      }
    }
  }
  if (channel == "1") {
    for (uint8_t i = 0; i < server.args(); i++) {
      if (server.argName(i) == "code") {
        uint32_t code = strtoul(server.arg(i).c_str(), NULL, 10);
        irsend_channel1.sendNEC(code, 32);
        //temp_topic = channel1_return_topic;
        string_channel1_return_topic.toCharArray(temp_topic, 80);
       
        digitalWrite(LED, LOW);
        delay(10);
      }
    }
  }
  digitalWrite(LED, HIGH);
  // JSON Variables
    
  DynamicJsonBuffer jsonBuffer;
  JsonObject& code_json = jsonBuffer.createObject();
  code_json["channel"] = String(channel);
  code_json["code"] = String(string_code);
  code_json["success"] = "1";
  char temp_buffer[256];
  code_json.printTo(temp_buffer, sizeof(temp_buffer));  
  Serial.println("Topic: ");
  Serial.println(temp_topic);
  Serial.println("Payload: ");
  Serial.println(temp_buffer);    
  Serial.println("Payload Size: ");
  Serial.println(sizeof(temp_buffer));
  client.publish(temp_topic, temp_buffer);
  server.send(200, "application/json", temp_buffer);
}

void handleConfig() {
  String ota = server.arg("ota");
  //String string_code = server.arg("code");
  if (ota == "0") {
        digitalWrite(LED, LOW);
        delay(10);
        ota_flag = false;
        delay(10);
        firmwareUpdateHandler();
    }
  if (ota == "1") {
        digitalWrite(LED, LOW);
        delay(10);
        ota_flag = true;
        delay(10);
        firmwareUpdateHandler();  
  }
  digitalWrite(LED, HIGH);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  server.send(404, "text/plain", message);
}

void setup(void) {
  pinMode(LED, OUTPUT); // builtin LED
  digitalWrite(LED, LOW);



  // Begin IR Channel 0 & 1
  irsend_channel0.begin();
  irsend_channel1.begin();
  Serial.setDebugOutput(true);
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(10);

  // Begin Wire Library for I2C
  Wire.begin(D3, D4);
  Wire.setClock(100000);

  WiFi.mode(WIFI_STA); // do I need that?
  delay(10);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("");
  Serial.print("Attmempting to Connect to: ");
  Serial.println(ssid);
  delay(10);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // OTA Authentication
  //ArduinoOTA.setPassword((const char *)"8472");
  ArduinoOTA.setHostname(DEVICE_NAME);
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  String local_ip =  WiFi.localIP().toString();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  string_device_topic.toCharArray(device_topic, 80);

  Serial.println("Device Topic: ");
  Serial.print(device_topic);
  Serial.println("");

  // More OTA

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  // END OTA

  // Start SPIFFS
  if (!SPIFFS.begin())
  {
    // Serious problem
    Serial.println("SPIFFS Mount failed");
  } else {
    Serial.println("SPIFFS Mount succesfull");
  }

  // Start HTTP Server
  if (mdns.begin(DEVICE_NAME, WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }
  server.serveStatic("/js", SPIFFS, "/js");
  server.serveStatic("/", SPIFFS, "/index.html");
  //server.on("/", handleRoot);
  server.on("/ir", handleIr);
  server.on("/measures.json", JSONsendMeasures);
  server.on("/current_config.json", JSONsendConfig);
  server.on("/spiffs.json", JSONsendSPIFFS);
  server.on("/config", handleConfig);
  server.on("/inline", []() {
  server.send(200, "text/plain", "this works as well");
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  // MQTT Client
  if (client.connect("ESP8266_Bedroom")) {
    // JSON Variables    
    DynamicJsonBuffer jsonBuffer;
    JsonObject& device_json = jsonBuffer.createObject();
    device_json["device_name"] = DEVICE_NAME;
    device_json["ip_address"] = local_ip;
    device_json["update_mode"] = String(ota_flag);
    device_json["device_topic"] = string_device_topic;
    char temp_buffer[256];
    device_json.printTo(temp_buffer, sizeof(temp_buffer));
    
    Serial.println("Topic: ");
    Serial.println(all_devices_topic);
    Serial.println("Payload: ");
    Serial.println(temp_buffer);    
    Serial.println("Payload Size: ");
    Serial.println(sizeof(temp_buffer));
    client.publish(all_devices_topic, temp_buffer);
    JSONsendSPIFFS();
    client.subscribe("/bedroom/ac/channel0");   // subscribe to channel 0 commends
    client.subscribe("/bedroom/ac/channel1");   // subscribe to channel 1 commends
    string_device_command_topic.toCharArray(device_command_topic, 80);
    client.subscribe(device_command_topic);   // subscribe to OTA & Device Commands
  }
  digitalWrite(LED, HIGH);

  // Initialize Sensor
  if (!bme.begin()) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
}

  //float getVcc = (float)ESP.getVcc();

  //Serial.println("Voltage: ");
  //Serial.println(getVcc);


void reconnect() {
  // Loop until we're reconnected
  digitalWrite(LED, LOW);

  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266AC")) {
      Serial.println("connected");
      // Subscribe or resubscribe to a topic
      // You can subscribe to more topics (to control more LEDs in this example)
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 4 seconds before retrying
      digitalWrite(LED, HIGH);
      delay(1000);
      digitalWrite(LED, LOW);
      delay(1000);
      digitalWrite(LED, HIGH);
      delay(1000);
      digitalWrite(LED, LOW);
      delay(1000);
      digitalWrite(LED, HIGH);

    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  int i = 0;

  Serial.println("Message arrived:  topic: " + String(topic));

  char messageBuf[MQTT_MAX_PACKET_SIZE];
  for (unsigned int i = 0; i < length; i++)
  {
    char tempString[2];
    tempString[0] = (char)payload[i];
    tempString[1] = '\0';
    if (i == 0)
      strcpy(messageBuf, tempString);
    else
      strcat(messageBuf, tempString);
  }

  String msgString = String(messageBuf);
  String topicString = String(topic);

  Serial.println("Topic: " + topicString);
  Serial.println("Payload: " + msgString);

  if (topicString == "/bedroom/ac/channel0") {
    if (msgString == "tempup") {
      irsend_channel0.sendNEC(0x8166A15E, 32);
      Serial.println("Temperature Up");
      client.publish("/bedroom/ac/0/status", "Channel 0: TempUp");

      digitalWrite(LED, LOW);
      delay(10);
    }
    else if (msgString == "tempdown") {
      irsend_channel0.sendNEC(0x816651AE, 32);
      Serial.println("Temperature Down");
      client.publish("/bedroom/ac/0/status", "Channel 0: TempDown");

      digitalWrite(LED, LOW);
      delay(10);
    }
    else if (msgString == "fan") {
      irsend_channel0.sendNEC(0x81669966, 32);
      Serial.println("Fan Switch");
      client.publish("/bedroom/ac/0/status", "Channel 0: Fan");

      digitalWrite(LED, LOW);
      delay(10);
    }
    else if (msgString == "timer") {
      irsend_channel0.sendNEC(0x8166F906, 32);
      Serial.println("Timer");
      client.publish("/bedroom/ac/0/status", "Channel 0: Timer");

      digitalWrite(LED, LOW);
      delay(10);
    }
    else if (msgString == "mode") {
      irsend_channel0.sendNEC(0x8166D926, 32);
      Serial.println("Mode");
      client.publish("/bedroom/ac/0/status", "Channel 0: Mode");

      digitalWrite(LED, LOW);
      delay(10);
    }
    else if (msgString == "swing") {
      irsend_channel0.sendNEC(0x8166C13E, 32);
      Serial.println("Swing");
      client.publish("/bedroom/ac/0/status", "Channel 0: Swing");

      digitalWrite(LED, LOW);
      delay(10);
    }
    else if (msgString == "power") {
      irsend_channel0.sendNEC(0x8166817E, 32);
      Serial.println("Power");
      client.publish("/bedroom/ac/0/status", "Channel 0: Power");

      digitalWrite(LED, LOW);
      delay(10);
    }
    else {
      Serial.println("Unknown Command");
      digitalWrite(LED, LOW);
      delay(100);
      digitalWrite(LED, HIGH);
      delay(100);
      digitalWrite(LED, LOW);
      delay(100);
      digitalWrite(LED, HIGH);
      delay(100);
      digitalWrite(LED, LOW);
      delay(100);
      digitalWrite(LED, HIGH);
      client.publish("/bedroom/ac/0/status", "Channel 0: Unknown Command");
    }
  }
  else if (topicString == "/bedroom/ac/channel1") {
    if (msgString == "tempup") {
      irsend_channel1.sendNEC(nec_tempup, 32);
      Serial.println("Temperature Up");
      Serial.println(nec_tempup);
      client.publish("/bedroom/ac/1/status", "Channel 1: TempUp");

      digitalWrite(LED, LOW);
      delay(10);
    }
    else if (msgString == "tempdown") {
      irsend_channel1.sendNEC(nec_tempdown, 32);
      Serial.println("Temperature Down");
      Serial.println(nec_tempdown);
      client.publish("/bedroom/ac/1/status", "Channel 1: TempDown");

      digitalWrite(LED, LOW);
      delay(10);
    }
    else if (msgString == "fan") {
      irsend_channel1.sendNEC(nec_fan, 32);
      Serial.println("Fan Switch");
      Serial.println(nec_fan);
      client.publish("/bedroom/ac/1/status", "Channel 1: Fan");

      digitalWrite(LED, LOW);
      delay(10);
    }
    else if (msgString == "timer") {
      irsend_channel1.sendNEC(nec_timer, 32);
      Serial.println("Timer");
      Serial.println(nec_timer);

      client.publish("/bedroom/ac/1/status", "Channel 1: Timer");

      digitalWrite(LED, LOW);
      delay(10);
    }
    else if (msgString == "mode") {
      irsend_channel1.sendNEC(nec_mode, 32);
      Serial.println("Mode");
      Serial.println(nec_mode);

      client.publish("/bedroom/ac/1/status", "Channel 1: Mode");

      digitalWrite(LED, LOW);
      delay(10);
    }
    else if (msgString == "swing") {
      irsend_channel1.sendNEC(nec_swing, 32);
      Serial.println("Swing");
      Serial.println(nec_swing);
      client.publish("/bedroom/ac/1/status", "Channel 1: Swing");

      digitalWrite(LED, LOW);
      delay(10);
    }
    else if (msgString == "power") {
      irsend_channel1.sendNEC(nec_power, 32);
      Serial.println(nec_power);

      Serial.println("Power");
      client.publish("/bedroom/ac/1/status", "Channel 1: Power");

      digitalWrite(LED, LOW);
      delay(10);
    }
    else {
      Serial.println("Unknown Command");
      digitalWrite(LED, LOW);
      delay(100);
      digitalWrite(LED, HIGH);
      delay(100);
      digitalWrite(LED, LOW);
      delay(100);
      digitalWrite(LED, HIGH);
      delay(100);
      digitalWrite(LED, LOW);
      delay(100);
      digitalWrite(LED, HIGH);
      client.publish("/bedroom/ac/1/status", "Unknown Command");
    }
  }
  else if (topicString == string_device_command_topic) {
      if (msgString == "OTA_Enable") {
          digitalWrite(LED, LOW);
          delay(10);
          ota_flag = true;
          delay(10);
          firmwareUpdateHandler();

    }
        else if (msgString == "OTA_Disable") {
          digitalWrite(LED, LOW);
          delay(10);
          ota_flag = false;
          delay(10);
          firmwareUpdateHandler();
    }
        else if (msgString == "Update_SPIFFS") {
          digitalWrite(LED, LOW);
          delay(10);
          JSONsendSPIFFS();
        }
        else if (msgString == "Config") {
          digitalWrite(LED, LOW);
          delay(10);
          JSONsendConfig();
        }
  }

  //  int state = digitalRead(2);  // get the current state of GPIO1 pin
  //  if (msgString == "1"){    // if there is a "1" published to any topic (#) on the broker then:
  //    digitalWrite(2, !state);     // set pin to the opposite state
  //    Serial.println("Switching LED");
  //  }
  digitalWrite(LED, HIGH);
}

void battery_handler() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > battery_interval) {
    previousMillis = currentMillis;

    //float getVcc = (float)ESP.getVcc();
    //Adjustment Value = 0.025
    float BatteryValue = ((float)analogRead(A0) * 0.025);

    Serial.println("Voltage: ");
    Serial.println(BatteryValue);

      // JSON Variables
    
    DynamicJsonBuffer jsonBuffer;
    JsonObject& device_json = jsonBuffer.createObject();
    device_json["battery"] = String(BatteryValue);
    char temp_buffer[256];
    device_json.printTo(temp_buffer, sizeof(temp_buffer));
    
    Serial.println("Topic: ");
    string_device_topic.toCharArray(device_topic, 80);
    Serial.println(device_topic);
    Serial.println("Payload: ");
    Serial.println(temp_buffer);    
    Serial.println("Payload Size: ");
    Serial.println(sizeof(temp_buffer));
    client.publish(device_topic, temp_buffer);
    
  }
}
void firmwareUpdateHandler() {
        // OTA Flag set in Callback function & Server Function
        String ota_flag_string = "OTA Flag Unknown";
        if(ota_flag) {
          ota_flag_set_Millis = millis();
          ota_flag_string = "OTA Flag Enabled";        }
        if (!ota_flag) {
          ota_flag_string ="OTA Flag Disabled";
        }
        Serial.println(ota_flag_string);
        String local_ip =  WiFi.localIP().toString();

        // JSON Variables
        DynamicJsonBuffer jsonBuffer;
        JsonObject& device_json = jsonBuffer.createObject();
        
        device_json["device_name"] = DEVICE_NAME;
        device_json["ip_address"] = local_ip;
        device_json["update_mode"] = String(ota_flag);
        device_json["ota_status"] = String(ota_flag_string);
        device_json["device_topic"] = string_device_topic;
        char temp_buffer[256];
        device_json.printTo(temp_buffer, sizeof(temp_buffer));
        
        Serial.println("Topic: ");
        Serial.println(all_devices_topic);
        Serial.println("Payload: ");
        Serial.println(temp_buffer);    
        Serial.println("Payload Size: ");
        Serial.println(sizeof(temp_buffer));
        client.publish(all_devices_topic, temp_buffer);
        server.send(200, "application/json", temp_buffer);       
}

void otaFlagHandler() {
  ota_flag_currentMillis = millis();
  if (ota_flag_currentMillis - ota_flag_set_Millis > ota_flag_interval) {
    //ota_flag_previousMillis = ota_flag_currentMillis;

    ota_flag = false;
    firmwareUpdateHandler();
  }
}

void loop(void) {
  if(ota_flag) // While Loop??
  {
    ArduinoOTA.handle();
    otaFlagHandler();
  }
  server.handleClient();
  client.loop();
  battery_handler();
  getBME280();
}
