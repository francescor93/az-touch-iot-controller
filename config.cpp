#include "config.h"

// Function to load the configuration into the Config struct
bool loadConfiguration(Config &config, File &file, int jsonSize) {

  // Initialize a json document and deserialize the configuration file
  DynamicJsonDocument doc(jsonSize);
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.print("Error during config deserialization: ");
    Serial.println(error.c_str());
    return false;
  }

  // Write the configuration parameters into the Config struct
  strlcpy(config.wifi.ssid, doc["wifi"]["ssid"], sizeof(config.wifi.ssid));
  strlcpy(config.wifi.password, doc["wifi"]["password"], sizeof(config.wifi.password));
  config.wifi.ip.fromString(doc["wifi"]["ip"].as<String>());
  config.wifi.gateway.fromString(doc["wifi"]["gateway"].as<String>());
  config.wifi.subnet.fromString(doc["wifi"]["subnet"].as<String>());
  config.wifi.dns.fromString(doc["wifi"]["dns"].as<String>());
  strlcpy(config.mqtt.host, doc["mqtt"]["host"], sizeof(config.mqtt.host));
  strlcpy(config.mqtt.user, doc["mqtt"]["user"], sizeof(config.mqtt.user));
  strlcpy(config.mqtt.password, doc["mqtt"]["password"], sizeof(config.mqtt.password));
  strlcpy(config.device.id, doc["device"]["id"], sizeof(config.device.id));
  config.time.utcOffset = doc["time"]["utcOffset"];
  strlcpy(config.time.ntpServer, doc["time"]["ntpServer"], sizeof(config.time.ntpServer));
  config.screen.landscape = doc["screen"]["landscape"];
  config.screen.iconsSize = doc["screen"]["iconsSize"];
  config.screen.timeout = doc["screen"]["timeout"];
  config.screen.headerHeight = doc["screen"]["headerHeight"];

  // According to the configuration, set the screen values 
  config.screen.width = (config.screen.landscape ? 320 : 240);
  config.screen.height = (config.screen.landscape ? 240 : 320);
  config.screen.grid.width = config.screen.width;
  config.screen.grid.height = config.screen.height - config.screen.headerHeight;
  config.screen.grid.cols = (config.screen.landscape ? doc["screen"]["maxColsLandscape"] : doc["screen"]["maxRowsLandscape"]);
  config.screen.grid.rows = (config.screen.landscape ? doc["screen"]["maxRowsLandscape"] : doc["screen"]["maxColsLandscape"]);
  config.screen.grid.cellWidth = config.screen.grid.width / config.screen.grid.cols;
  config.screen.grid.cellHeight = config.screen.grid.height / config.screen.grid.rows;
  config.screen.colors.mainBackground = (doc["screen"]["darkMode"] ? 0 : 1);
  config.screen.colors.mainForeground = (doc["screen"]["darkMode"] ? 1 : 0);
  config.screen.colors.secondaryBackground = (doc["screen"]["darkMode"] ? 2 : 3);
  config.screen.colors.secondaryForeground = (doc["screen"]["darkMode"] ? 3 : 2);

  // Write each IoT object in its own struct and add it to the main Config struct
  int i = 0;
  for (JsonObject iotObj : doc["iot"].as<JsonArray>()) {
    Iot iot;
    strlcpy(iot.name, iotObj["name"], sizeof(iot.name));
    strlcpy(iot.icon, iotObj["icon"], sizeof(iot.icon));
    strlcpy(iot.topic.listRequest, iotObj["topic"]["listRequest"], sizeof(iot.topic.listRequest));
    strlcpy(iot.topic.listResponse, iotObj["topic"]["listResponse"], sizeof(iot.topic.listResponse));
    config.iot[i] = iot;
    i += 1;
  }
  config.iotList = i;

  // Return positive result
  return true;
}
