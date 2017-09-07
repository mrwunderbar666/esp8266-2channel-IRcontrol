/* ***********************************************
Settings File Handling SPIFFS - JSON
**************************************************/

char JSON_SETTINGS[1024];

const char* char_channel0_name;
const char* char_channel1_name;

String channel0_name;
String channel1_name;

int channel0_calibration;
int channel1_calibration;

unsigned long saveSettings_previousMillis = 0;
unsigned long saveSettings_currentMillis = 0;
long saveSettings_interval = 5 * 60 * 1000; // Interval between saving settings



bool loadSettings() {
  File settingsFile = SPIFFS.open(SETTINGS_FILE, "r");
  if (!settingsFile) {
    Serial.println("Failed to open settings file");
    return false;
  }

  size_t size = settingsFile.size();
  if (size > 1024) {
    Serial.println("Settings file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  settingsFile.readBytes(buf.get(), size);

  DynamicJsonBuffer jsonBuffer;
  JsonObject& SettingsObject = jsonBuffer.parseObject(buf.get());

  if (!SettingsObject.success()) {
    Serial.println("Failed to parse settings file");
    return false;
  }

  Serial.println("Settings loaded. Raw JSON Data: ");
  SettingsObject.printTo(JSON_SETTINGS);

  SettingsObject.prettyPrintTo(Serial);

  Serial.println("\n");

  char_channel0_name = SettingsObject["channel0_name"];
  channel0_name = String(char_channel0_name);
  Serial.println("JSON - Channel 0 Name: ");
  Serial.println(channel0_name);

  channel0_calibration = SettingsObject["channel0_calibration"];
  Serial.println("JSON - Channel 0 Calibration: ");
  Serial.println(channel0_calibration);

  char_channel1_name = SettingsObject["channel1_name"];
  channel1_name = String(char_channel1_name);
  Serial.println("JSON - Channel 1 Name: ");
  Serial.println(channel1_name);

  channel1_calibration = SettingsObject["channel1_calibration"];
  Serial.println("JSON - Channel 1 Calibration: ");
  Serial.println(channel1_calibration);

  settingsFile.close();
  return true;
}

bool saveSettings() {
  Serial.println("Saving Settings...");
  Serial.println("Following Values will be stored: ");
  Serial.println(channel0_name);
  Serial.println(channel0_calibration);
  Serial.println(channel1_name);
  Serial.println(channel1_calibration);

  DynamicJsonBuffer jsonBuffer;
  JsonObject& SettingsObject = jsonBuffer.createObject();

  SettingsObject["channel0_name"] = channel0_name;
  SettingsObject["channel1_name"] = channel1_name;
  SettingsObject["channel0_calibration"] = channel0_calibration;
  SettingsObject["channel1_calibration"] = channel1_calibration;

  File settingsFile = SPIFFS.open("/settings.json", "w");
  if (!settingsFile) {
    Serial.println("Failed to open settings file for writing");
    return false;
  }

  SettingsObject.printTo(settingsFile);
  settingsFile.close();
  return true;
}


void saveSettings_handler() {
 saveSettings_currentMillis = millis();

 if (saveSettings_currentMillis - saveSettings_previousMillis > saveSettings_interval) {
   saveSettings_previousMillis = saveSettings_currentMillis;

   Serial.println("Saving Settings to SPIFFS");
   if (!saveSettings()) {
     Serial.println("Big Problem! Couldn't write settings to SPIFFS");
   }

 }
}
