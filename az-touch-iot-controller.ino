/*
 * Title
 *
 * Description 1
 * Description 2
*/

// Enable ArduinoJson comment support, to better understand the configuration file
#define ARDUINOJSON_ENABLE_COMMENTS 1

// Include the necessary libraries
#include <Arduino.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <XPT2046_Touchscreen.h>
#include <MiniGrafx.h>
#include <ILI9341_SPI.h>
#include <simpleDSTadjust.h>
#undef min // This is essential to avoid the compilation error "expected unqualified-id before '(' token" when including the ArduinoJson library
#include <ArduinoJson.h>
#include <SD.h>
#include "ArialRounded.h"
#include "TouchControllerWS.h"
#include "icons.cpp"

// Include the configuration file
#include "settings.h"

// Define the pins used by the system
#define TFT_DC D2
#define TFT_CS D1
#define TFT_LED D8
#define TOUCH_CS D3
#define TOUCH_IRQ D4
#define SD_CS D0

// Initialize the variables for the device
const char* DEVICE = "TouchController";
const char* DELIMITER = "/";

// Configure the screen
#define HAVE_TOUCHPAD
int BITS_PER_PIXEL = 2; // 2^2 =  4 colors
uint16_t palette[] = {
  ILI9341_BLACK, // 0
  ILI9341_WHITE, // 1
  ILI9341_BLUE, //2
  ILI9341_YELLOW // 3
};

// Initialize the variables used by the sketch
int currentScreen;
int currentPage;
unsigned long int lastTouch;

// Initialize the struct for the configuration and define the file name
struct Wifi {
  char ssid[32];
  char password[64];
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;
};
struct Mqtt {
  char host[32];
  char user[64];
  char password[64];
};
struct Device {
  char id[8];
};
struct Grid {
  int cols;
  int rows;
  int width;
  int height;
  int cellWidth;
  int cellHeight;
};
struct Colors {
  int mainBackground;
  int mainForeground;
  int secondaryBackground;
  int secondaryForeground;
};
struct Screen {
  bool landscape;
  int iconsSize;
  int timeout;
  int headerHeight;
  int width;
  int height;
  struct Grid grid;
  struct Colors colors;
};
struct Iot {
  char name[32];
  char icon[32];
  char topic[64];
};
struct Config {
  struct Wifi wifi;
  struct Mqtt mqtt;
  struct Device device;
  struct Screen screen;
  struct Iot iot[64];
  int iotList;
};
Config config;
const char* filename = "/config.txt";

// This line is needed to avoid the compile error for callback function not (yet) defined
extern void callback(char* topic, byte* payload, unsigned int length);

// Create a wifi client and bind the MQTT client
WiFiClient wifiClient;
PubSubClient client("127.0.0.1", 1883, callback, wifiClient); // Using a fake address, because configuration hasn't loaded yet

// Initialize the LCD screen
ILI9341_SPI tft = ILI9341_SPI(TFT_CS, TFT_DC);
MiniGrafx gfx = MiniGrafx(&tft, BITS_PER_PIXEL, palette);

// Initialize touch detection and its calibration
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
TouchControllerWS touchController(&ts);
void calibrationCallback(int16_t x, int16_t y);
CalibrationCallback calibration = &calibrationCallback;

// Initialize the controller for time synchronization
simpleDSTadjust dstAdjusted(StartRule, EndRule);

// Setup function, performed only once at startup
void setup() {

  // If debug is true, start the serial communication and print a message
  if (debug) {
    Serial.begin (115200);
    Serial.println("Serial started");
  }

  // Set default orientation and background. They will be updated as soon as the configuration is loaded.
  config.screen.landscape = false;
  config.screen.colors.mainBackground = 0;
  config.screen.colors.mainForeground = 1;
  config.screen.colors.secondaryBackground = 2;
  config.screen.colors.secondaryForeground = 3;

  // Start the LCD display and print a message if debug is true
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);
  gfx.init();
  gfx.setRotation(config.screen.landscape ? 3 : 2);
  gfx.fillBuffer(config.screen.colors.mainBackground);
  gfx.commit();
  if (debug) {
    Serial.println("LCD initialized");
  }

  // Start touch detection and print a message if debug is true
  ts.begin();
  if (debug) {
    Serial.println("Touch initialized");
  }

  // Load the flash file system and print a message if debug is true
  bool isFSMounted = SPIFFS.begin();
  if (!isFSMounted) {
    drawProgress(50, "Formatting file system");
    SPIFFS.format();
    drawProgress(100, "Formatting done");
  }
  if (debug) {
    Serial.println("SPIFFS file system loaded");
  }

  // Start reading the SD card and print a message if debug is true
  int i = 0;
  while (!SD.begin(SD_CS)) {
    if (i > 100) {
      drawProgress(i, "No SD card found");
      if (debug) {
        Serial.println("Cannot read SD card");
      }
      break;
    }
    drawProgress(i, "Loading SD card");
    i += 10;
    delay(500);
  }
  if (debug) {
    Serial.println("SD card loaded");
  }

  // If a configuration file exists on the card, copy it to SPIFFS memory and print a message if debug is true
  File sourceFile = SD.open(filename);
  if (sourceFile) {
    SPIFFS.remove(filename);
    File destFile = SPIFFS.open(filename, "w");
    static uint8_t buf[512];
    int i = 0;
    while (sourceFile.read(buf, 512)) {
      destFile.write(buf, 512);
      if (i > 80) {
        i = 0;
      }
      drawProgress(i, "Copying new configuration");
      i += 10;
      delay(500);
    }
    destFile.close();
    sourceFile.close();
    if (debug) {
      Serial.println("Configuration copied to SPIFFS memory");
    }
  }

  // Load the configuration
  loadConfiguration();

  // Set the correct screen rotation
  gfx.setRotation(config.screen.landscape ? 3 : 2);

  // Start the wifi and call the wifi connection and MQTT connection function
  WiFi.config(config.wifi.ip, config.wifi.gateway, config.wifi.subnet, config.wifi.dns);
  WiFi.begin(config.wifi.ssid, config.wifi.password);
  client.setServer(config.mqtt.host, 1883);
  reconnect();

  // Load the touchscreen calibration
  boolean isCalibrationAvailable = touchController.loadCalibration();
  if (!isCalibrationAvailable) {
    calibrateTouchScreen();
  }

  // Synchronize time with NTP server
  configTime(UTC_OFFSET * 3600, 0, NTP_SERVERS);

  // Set the home screen to show
  currentScreen = 0;
}

// Loop function, performed cyclically as long as the device is active
void loop() {

  // If the connection is lost, reconnect
  if ((WiFi.status() != WL_CONNECTED) || (!client.connected())) {
    reconnect();
  }

  // Update the MQTT connection
  client.loop();

  // Create the updated header
  drawHeader();

  // Shows the screen we need based on the currentScreen variable
  if (currentScreen == 0) {
    drawHome();
  }

  // When a touch is detected, identify the touched point
  if (touchController.isTouched(250)) {
    TS_Point p = touchController.getPoint();

    // If the screen was already on
    if (digitalRead(TFT_LED) == HIGH) {

      // Get the cell touched
      int cellNumber = calculateTouchedCell(p.x, p.y);

      // Perform the associated action
      executeCellAction(cellNumber);
    }

    // Make sure the backlight is on
    digitalWrite(TFT_LED, HIGH);

    // Update the last touch time
    lastTouch = millis();
  }

  // If the screen has been on for too long without touching, turn it off
  if ((millis() > (lastTouch + config.screen.timeout * 1000)) && (digitalRead(TFT_LED) == HIGH)) {
    digitalWrite(TFT_LED, LOW);
  }

  // Write the created data on the screen
  gfx.commit();

  // Add a small delay to allow the MQTT loop to run correctly
  delay(10);
}

// Callback function executed whenever an MQTT message is received
void callback(char* topic, byte* payload, unsigned int length) { //FixMe -- Start here

  // Print a message if debug is true
  if (debug) {
    Serial.print("Received command: "); Serial.println(topic);
    Serial.print("With payload: "); Serial.println(payload[0]);
  }
} //FixMe: -- End here

// Functions to calibrate the touchscreen at first power up
void calibrateTouchScreen() {
  touchController.startCalibration(&calibration);
  while (!touchController.isCalibrationFinished()) {
    gfx.fillBuffer(0);
    gfx.setColor(1);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.drawString(120, 160, "Please calibrate\ntouch screen by\ntouch point");
    touchController.continueCalibration();
    gfx.commit();
    yield();
  }
  touchController.saveCalibration();
}
void calibrationCallback(int16_t x, int16_t y) {
  gfx.setColor(1);
  gfx.fillCircle(x, y, 10);
}

// Function to show the loading screen, with the logo, a message provided and the progress bar
void drawProgress(uint8_t percentage, String text) {

  // Set the background and center-aligned Arial 14 font
  gfx.fillBuffer(config.screen.colors.mainBackground);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);

  // Create the logo
  gfx.setColor(config.screen.colors.secondaryForeground);
  gfx.drawXbm(20, 5, 200, 80, mainLogo);

  // Create the url
  gfx.setColor(config.screen.colors.mainForeground);
  gfx.drawString(120, 85, "https://www.regafamilysite.ga");

  // Create the text provided
  gfx.setColor(config.screen.colors.secondaryForeground);
  gfx.drawString(120, 146, text);

  // Create the progress bar
  gfx.setColor(config.screen.colors.mainForeground);
  gfx.drawRect(10, 168, 240 - 20, 15);
  gfx.setColor(config.screen.colors.secondaryForeground);
  gfx.fillRect(12, 170, 216 * percentage / 100, 11);

  // Write the result
  gfx.commit();
}

// Function to load the configuration into the Config struct
void loadConfiguration() {

  // Try to open the file and if it doesn't exist write it on the screen and print a message if debug is true
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    drawProgress(50, "Configuration file missing!");
    if (debug) {
      Serial.println("Configuration file missing!");
    }
    while(true) {} // Stay here forever
  }

  // Initialize a json document and deserialize the configuration file
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, file);

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
    strlcpy(iot.topic, iotObj["topic"], sizeof(iot.topic));
    config.iot[i] = iot;
    i += 1;
  }
  config.iotList = i;
  
  file.close();
}

// Function to generate the header bar with time and signal quality
void drawHeader() {

  // Set the background and left-aligned Arial 10 font
  gfx.fillBuffer(config.screen.colors.mainBackground);
  gfx.setColor(config.screen.colors.mainForeground);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);

  // Get the current time
  char time_str[11];
  char *dstAbbrev;
  time_t now = dstAdjusted.time(&dstAbbrev);
  struct tm * timeinfo = localtime (&now);

  // Create the time
  sprintf(time_str, "%02d:%02d",timeinfo->tm_hour, timeinfo->tm_min);
  gfx.drawString(3, 2, time_str);

  // Calculate the quality of the wifi signal as a percentage
  int32_t dbm = WiFi.RSSI();
  int8_t quality;
  if (dbm <= -100) {
    quality = 0;
  }
  else if (dbm >= -50) {
    quality = 100;
  }
  else {
    quality = 2 * (dbm + 100);
  }

  // Create signal quality
  gfx.drawString(205, 2, String(quality) + "%");
  for (int8_t i = 0; i < 4; i++) {
    for (int8_t j = 0; j < 2 * (i + 1); j++) {
      if (quality > i * 25 || j == 0) {
        gfx.setPixel(230 + 2 * i, 11 - j);
      }
    }
  }
}

// Function to create a grid consisting of the configured number of rows and columns
void drawGrid() {

  // Create the separator between header and table
  gfx.drawHorizontalLine(0, config.screen.headerHeight, config.screen.grid.width);

  // Create the columns
  int startX = 0;
  while (true) {
    startX = startX + config.screen.grid.cellWidth;
    if (startX > config.screen.grid.width) {
      break;
    }
    gfx.drawVerticalLine(startX, config.screen.headerHeight, config.screen.grid.height);
  }

  // Create the rows
  int startY = config.screen.headerHeight;
  while (true) {
    startY = startY + config.screen.grid.cellHeight;
    if (startY > config.screen.grid.width) {
      break;
    }
    gfx.drawHorizontalLine(0, startY, config.screen.grid.width);
  }
}

// Function to get row and column starting from the cell number 
void cellToGrid(int cellNumber, int &row, int &col) {
  float r = (float)cellNumber / (float)config.screen.grid.cols;
  r = r + 0.5;
  row = (int)r;
  col = cellNumber / row;
}

// Function to get the position of an image in the image array starting from its name
int getImageIndex(char* name) {
  int i = 0;
  for (char* iconName : iconNames) {
    if (strcmp(iconName, name) == 0) {
      return i;
    }
    i++;
  }
  return -1;
}

// Functions to update the contents of a cell with text or an image
void updateCell(int cellNumber, String text, int offsetX = 0, int offsetY = 0) {

  // Get the row and column number from the cell number 
  int row = 0;
  int col = 0;
  cellToGrid(cellNumber, row, col);

  // Get the coordinates of the cell vertices
  int minX = config.screen.grid.cellWidth * (col - 1);
  int maxX = minX + config.screen.grid.cellWidth;
  int minY = config.screen.grid.cellHeight * (row - 1) + config.screen.headerHeight;
  int maxY = minY + config.screen.grid.cellHeight;

  // Calculate the center of the cell
  int centerX = config.screen.grid.cellWidth / 2 + minX;
  int centerY = config.screen.grid.cellHeight / 2 + minY;

  // Create the text provided
  gfx.setColor(config.screen.colors.mainForeground);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  gfx.drawString(centerX + offsetX, centerY + offsetY, text);
}
void updateCell(int cellNumber, int index) {

  // Get the row and column number from the cell number 
  int row = 0;
  int col = 0;
  cellToGrid(cellNumber, row, col);

  // Get the coordinates of the cell vertices
  int minX = config.screen.grid.cellWidth * (col - 1);
  int maxX = minX + config.screen.grid.cellWidth;
  int minY = config.screen.grid.cellHeight * (row - 1) + config.screen.headerHeight;
  int maxY = minY + config.screen.grid.cellHeight;

  // Calculate the center of the cell
  int centerX = config.screen.grid.cellWidth / 2 + minX;
  int centerY = config.screen.grid.cellHeight / 2 + minY;

  // Create the image provided
  gfx.setColor(config.screen.colors.mainForeground);
  gfx.drawXbm(centerX - (config.screen.iconsSize / 2), centerY - (config.screen.iconsSize / 2), config.screen.iconsSize, config.screen.iconsSize, myIcons[index]);
}

// Function to show the main screen with the grid and icons of the different cells
void drawHome() {
  drawGrid();

  // Calculate the maximum number of cells on a page and the maximum number of devices to show 
  int maxCells = config.screen.grid.rows * config.screen.grid.cols;
  int maxIot = config.iotList;
  
  // Check if pagination is required
  bool needsPagination = config.iotList > maxCells;

  // Define the variable for the cell we are currently updating
  int currentCell = 1;

  // If pagination is required and the current page is not the first one, show the "Back" icon in the first cell and decrease the maximum number of devices that can be shown 
  if ((needsPagination) && (currentPage > 1)) {
    updateCell(currentCell, getImageIndex("back"));
    maxIot--;
    currentCell++;
  }

  // Calculate the lesser of the total number of displayable devices and the total number of configured devices
  int limit = min(maxIot, config.iotList);

  // For each real IoT device configured 
  for (int i = 0; i < limit; i++) {

    // Look for its image
    int imgIndex = getImageIndex(config.iot[i].icon);
    bool hasImage = imgIndex > -1;

    // If an image is available show it together with the name, otherwise show only the center-aligned name 
    if (hasImage) {
      updateCell(currentCell, imgIndex);
      updateCell(currentCell, String(config.iot[i].name), 0, 40);
    }
    else {
      updateCell(currentCell, String(config.iot[i].name));
    }

    // Increment the current cell number 
    currentCell++;
    
    
    

    
    
  }
}

// Function to obtain the number of the cell touched, given the coordinates of the point
int calculateTouchedCell(int x, int y) {

  // Print a message if debug is true
  if (debug) {
    Serial.print("X: "); Serial.print(x); Serial.print("; Y: "); Serial.println(y);
  }

  // Create an array to get cell number quickly given row and column number
  int cells[config.screen.grid.rows][config.screen.grid.cols] = {};
  int n = 1;
  for (int r = 0; r < config.screen.grid.rows; r++) {
    for (int c = 0; c < config.screen.grid.cols; c++) {
      cells[r][c] = n;
      n++;
    }
  }

  // Calculate row and column touched
  int touchedRow = (y / config.screen.grid.cellHeight);
  int touchedCol = (x / config.screen.grid.cellWidth);

  // If the calculated values are beyond the limits, normalize them
  if (touchedRow < 0) { touchedRow = 0; }
  if (touchedRow > config.screen.grid.rows - 1) { touchedRow = config.screen.grid.rows - 1; }
  if (touchedCol < 0) { touchedCol = 0; }
  if (touchedCol > config.screen.grid.cols - 1) { touchedCol = config.screen.grid.cols - 1; }

  // Print a message if debug is true
  if (debug) {
    Serial.print("Touched row "); Serial.print(touchedRow + 1); Serial.print(" and col "); Serial.println(touchedCol + 1);
  }

  // Get the cell number from the array, print a message if the debug is true, and then return it
  int cellNumber = cells[touchedRow][touchedCol];
  if (debug) {
    Serial.print("Cell number is: "); Serial.println(cellNumber);
  }
  return cellNumber;
}

// Function to perform an action based on the cell touched and the current screen
void executeCellAction(int cellNumber) {

  // If the current screen is the home screen, send the status request on the topic associated with the cell and print a message if debug is true
  if (currentScreen == 0) {
    const char* topic = topics[cellNumber - 1];
    if (strcmp(topic, "") != 0) {
      if (debug) {
        Serial.print("Sending request to "); Serial.println(topic);
      }
      client.publish(topics[cellNumber - 1], "");
    }
  }
}

// Function for (re) connection to the wifi and MQTT broker
void reconnect() {

  // If the wifi is disconnected reconnect showing it on the screen, and print a message if the debug is true
  int i = 0;
  if (WiFi.status() != WL_CONNECTED) {
    while (WiFi.status() != WL_CONNECTED) {
      if (i > 80) {
        i = 0;
      }
      drawProgress(i, "Connecting to WiFi '" + String(config.wifi.ssid) + "'");
      i += 10;
      delay(500);
    }
    if (debug) {
      Serial.println("Connected to WiFi");
    }
  }

  // If the wifi is connected, but the MQTT broker is disconnected, reconnect and subscribe to the topics showing it on the screen, and print a message if the debug is true
  if ((WiFi.status() == WL_CONNECTED) && (!client.connected())) {
    char mqttClientID[sizeof(DEVICE) + sizeof(config.device.id) + 3];
    strcpy(mqttClientID, DEVICE); strcat(mqttClientID, config.device.id);
    if (debug) {
      Serial.print("Set MQTT client ID: "); Serial.println(mqttClientID);
    }
    int i = 0;
    while (!client.connected()) {
      if (i > 80) {
        i = 0;
      }
      drawProgress(i, "Connecting to MQTT '" + String(config.mqtt.host) + "'");
      i += 10;
      if (client.connect(mqttClientID, config.mqtt.user, config.mqtt.password)) {
        //client.subscribe(topic-here); //FixMe
        //client.setBufferSize(512); //FixMe
      }
      delay(500);
    }
    if (debug) {
      Serial.println("Connected to MQTT");
      //Serial.print("Buffer size is: "); Serial.println(client.getBufferSize()); //FixMe
    }
  }
}
