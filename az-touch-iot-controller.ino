/*
 * Title
 *
 * Description 1
 * Description 2
*/

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
#include "icons.h"

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
const int SCREEN_WIDTH = (isLandscape ? 320 : 240);
const int SCREEN_HEIGHT = (isLandscape ? 240 : 320);
const int GRID_WIDTH = SCREEN_WIDTH;
const int GRID_HEIGHT = SCREEN_HEIGHT - SCREEN_HEADER;
const int GRID_COLS = (isLandscape ? gridCols : gridRows);
const int GRID_ROWS = (isLandscape ? gridRows : gridCols);
#define HAVE_TOUCHPAD
#define MAIN_BACKGROUND (isDarkMode ? 0 : 1)
#define MAIN_FOREGROUND (isDarkMode ? 1 : 0)
#define SECONDARY_FOREGROUND (isDarkMode ? 2 : 3)
#define SECONDARY_BACKGROUND (isDarkMode ? 3 : 2)
int BITS_PER_PIXEL = 2; // 2^2 =  4 colors
uint16_t palette[] = {
  ILI9341_BLACK, // 0
  ILI9341_WHITE, // 1
  ILI9341_YELLOW, // 2
  0x7E3C //3
};
const char* icons[] = {
  relayIcon,
  curtainIcon,
  airconditionerIcon,
  ledstripIcon,
  environmentsensorIcon
};

// Initialize the variables used by the sketch
int currentScreen;
unsigned long int lastTouch;
int cellWidth = GRID_WIDTH / GRID_COLS;
int cellHeight = GRID_HEIGHT / GRID_ROWS;

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
struct Config {
  struct Wifi wifi;
  struct Mqtt mqtt;
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

  // Start the LCD display and print a message if debug is true
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);
  gfx.init();
  gfx.setRotation(isLandscape ? 3 : 2);
  gfx.fillBuffer(MAIN_BACKGROUND);
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
    if (i > 80) {
      i = 0;
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
  if ((millis() > (lastTouch + screenTimeout * 1000)) && (digitalRead(TFT_LED) == HIGH)) {
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
    gfx.setColor(SECONDARY_FOREGROUND);
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
  gfx.fillBuffer(MAIN_BACKGROUND);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);

  // Create the logo
  gfx.setColor(SECONDARY_FOREGROUND);
  gfx.drawXbm(20, 5, 200, 80, mainLogo);

  // Create the url
  gfx.setColor(MAIN_FOREGROUND);
  gfx.drawString(120, 85, "https://www.regafamilysite.ga");

  // Create the text provided
  gfx.setColor(SECONDARY_FOREGROUND);
  gfx.drawString(120, 146, text);

  // Create the progress bar
  gfx.setColor(MAIN_FOREGROUND);
  gfx.drawRect(10, 168, 240 - 20, 15);
  gfx.setColor(SECONDARY_FOREGROUND);
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
  StaticJsonDocument<512> doc;
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

  file.close();
}

// Function to generate the header bar with time and signal quality
void drawHeader() {

  // Set the background and left-aligned Arial 10 font
  gfx.fillBuffer(MAIN_BACKGROUND);
  gfx.setColor(MAIN_FOREGROUND);
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
  gfx.drawHorizontalLine(0, SCREEN_HEADER, GRID_WIDTH);

  // Create the columns
  int startX = 0;
  while (true) {
    startX = startX + cellWidth;
    if (startX > GRID_WIDTH) {
      break;
    }
    gfx.drawVerticalLine(startX, SCREEN_HEADER, GRID_HEIGHT);
  }

  // Create the rows
  int startY = SCREEN_HEADER;
  while (true) {
    startY = startY + cellHeight;
    if (startY > GRID_WIDTH) {
      break;
    }
    gfx.drawHorizontalLine(0, startY, GRID_WIDTH);
  }
}

// Functions to update the contents of a cell with text or an image
void updateCell(int row, int col, String text, int offsetX = 0, int offsetY = 0) {

  // Get the coordinates of the cell vertices
  int minX = cellWidth * (col - 1);
  int maxX = minX + cellWidth;
  int minY = cellHeight * (row - 1) + SCREEN_HEADER;
  int maxY = minY + cellHeight;

  // Calculate the center of the cell
  int centerX = cellWidth / 2 + minX;
  int centerY = cellHeight / 2 + minY;

  // Create the text provided
  gfx.setColor(MAIN_FOREGROUND);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  gfx.drawString(centerX + offsetX, centerY + offsetY, text);
}
void updateCell(int row, int col, int imgNum) {

  // Get the coordinates of the cell vertices
  int minX = cellWidth * (col - 1);
  int maxX = minX + cellWidth;
  int minY = cellHeight * (row - 1) + SCREEN_HEADER;
  int maxY = minY + cellHeight;

  // Calculate the center of the cell
  int centerX = cellWidth / 2 + minX;
  int centerY = cellHeight / 2 + minY;

  // Create the image provided
  gfx.setColor(MAIN_FOREGROUND);
  gfx.drawXbm(centerX - (iconsSize / 2), centerY - (iconsSize / 2), iconsSize, iconsSize, icons[imgNum]);
}

// Function to show the main screen with the grid and icons of the different cells
void drawHome() {
  drawGrid();
  updateCell(1, 1, 0);
  updateCell(1, 1, "Relè", 0, 40);
  updateCell(1, 2, 1);
  updateCell(1, 2, "Tende", 0, 40);
  updateCell(2, 1, 2);
  updateCell(2, 1, "Condizionatori", 0, 40);
  updateCell(2, 2, 3);
  updateCell(2, 2, "Strisce LED", 0, 40);
  updateCell(3, 1, 4);
  updateCell(3, 1, "Sensori ambientali", 0, 40);
}

// Function to obtain the number of the cell touched, given the coordinates of the point
int calculateTouchedCell(int x, int y) {

  // Print a message if debug is true
  if (debug) {
    Serial.print("X: "); Serial.print(x); Serial.print("; Y: "); Serial.println(y);
  }

  // Create an array to get cell number quickly given row and column number
  int cells[GRID_ROWS][GRID_COLS] = {};
  int n = 1;
  for (int r = 0; r < GRID_ROWS; r++) {
    for (int c = 0; c < GRID_COLS; c++) {
      cells[r][c] = n;
      n++;
    }
  }

  // Calculate row and column touched
  int touchedRow = (y / cellHeight);
  int touchedCol = (x / cellWidth);

  // If the calculated values are beyond the limits, normalize them
  if (touchedRow < 0) { touchedRow = 0; }
  if (touchedRow > GRID_ROWS - 1) { touchedRow = GRID_ROWS - 1; }
  if (touchedCol < 0) { touchedCol = 0; }
  if (touchedCol > GRID_COLS - 1) { touchedCol = GRID_COLS - 1; }

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
    char mqttClientID[sizeof(DEVICE) + sizeof(CLIENTID) + 3];
    strcpy(mqttClientID, DEVICE); strcat(mqttClientID, CLIENTID);
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
