/*
 * Title
 *
 * Description 1
 * Description 2
*/

/*
 * Screen helper:
 * -2: Loading something
 * -1: Home
 * 0~99: Devices list
 * 100~199: Device actions
 */

// Include the necessary libraries
#include <Arduino.h>
#include <SPI.h>
#ifdef ESP32
  #include <WiFi.h>
#endif
#ifdef ESP8266
  #include <ESP8266WiFi.h>
#endif
#include <PubSubClient.h>
#include <simpleDSTadjust.h>
#undef min // This is essential to avoid the compilation error "expected unqualified-id before '(' token" when including the ArduinoJson library
#include <ArduinoJson.h>
#include <SD.h>

/***** Advanced Configuration *****/
  struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600};
  struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};
  int configSize = 2395;
  int mqttSize = 256;
  const int maxDevices = 8;
  const char* filename = "/config.txt";
  bool debug = true;
/***** Advanced Configuration *****/

// Include local files
#include "TouchControllerWS.h"
#include "config.h"
#include "draw.h"
#include "icons.cpp"

// Define the pins used by the system
#ifdef ESP32
  #define TFT_LED 15
  #define TOUCH_CS 14
  #define TOUCH_IRQ 27
  #define SD_CS 21
#endif
#ifdef ESP8266
  #define TFT_LED D8
  #define TOUCH_CS D3
  #define TOUCH_IRQ D4
  #define SD_CS D0
#endif

// Initialize the variables for the device
const char* DEVICE = "TouchController";
const char* DELIMITER = "/";

// Define the appearance of the screen
#define HAVE_TOUCHPAD

// Initialize the variables used by the sketch
int currentScreen;
int currentPage;
unsigned long int lastTouch;

// Initialize the struct for the configuration
Config config;

// Initialize the struct for individual IoT devices
struct Statuses {
  char name[32];
  char topic[64];
};
struct IotDevice {
  int id;
  char name[32];
  char icon[16];
  char color[8];
};
struct Devices {
  IotDevice iot[maxDevices];
  int iotList;
};
Devices currentDevices;

// This line is needed to avoid the compile error for callback function not (yet) defined
extern void callback(char* topic, byte* payload, unsigned int length);

// Create a wifi client and bind the MQTT client
WiFiClient wifiClient;
PubSubClient client("127.0.0.1", 1883, callback, wifiClient); // Using a fake address, because configuration hasn't loaded yet

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

  // Set default size, orientation and background. They will be updated as soon as the configuration is loaded.
  config.screen.landscape = false;
  config.screen.width = 240;
  config.screen.height = 320;
  config.screen.colors.mainBackground = 0;
  config.screen.colors.mainForeground = 1;
  config.screen.colors.secondaryBackground = 2;
  config.screen.colors.secondaryForeground = 3;

  // Start the LCD display and print a message if debug is true
  pinMode(TFT_LED, OUTPUT);
  lcdOn();
  displayInit(config.screen.width, config.screen.height);
  displaySetRotation(config.screen.landscape ? 3 : 2);
  displayFill(config.screen.colors.mainBackground);
  displayCommit(config.screen.width, config.screen.height);
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
    Serial.println("SD card reader initialized");
  }

  // If a configuration file exists on the card, copy it to SPIFFS memory and print a message if debug is true
  File sourceFile = SD.open(filename);
  if (sourceFile) {
    SPIFFS.remove(filename);
    File destFile = SPIFFS.open(filename, "w");
    uint8_t buf[configSize];
    int i = 0;
    while (sourceFile.read(buf, configSize)) {
      destFile.write(buf, configSize);
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
  else {
    Serial.println("No file");
  }

  // Try to open the file and if it doesn't exist write it on the screen and print a message if debug is true
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    drawProgress(50, "Configuration file missing!");
    if (debug) {
      Serial.println("Configuration file missing!");
    }
    while(true) { delay(10); } // Stay here forever
  }

  // Load the configuration and close the file
  drawProgress(50, "Loading configuration");
  if (!loadConfiguration(config, file, configSize)) {
    drawProgress(50, "Can't read configuration");
    if (debug) {
      Serial.println("Can't read configuration");
    }
    while(true) { delay(10); } // Stay here forever
  }
  drawProgress(100, "Configuration loaded");
  file.close();

  // Set the correct screen rotation
  displaySetRotation(config.screen.landscape ? 3 : 2);
  
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
  configTime(config.time.utcOffset * 3600, 0, config.time.ntpServer);

  // Set the home screen to show
  currentScreen = -1;
  currentPage = 1;
}

// Loop function, performed cyclically as long as the device is active
void loop() {

  // If the connection is lost, reconnect
  if ((WiFi.status() != WL_CONNECTED) || (!client.connected())) {
    reconnect();
  }

  // Update the MQTT connection
  client.loop();

  // If not waiting loading
  if (currentScreen != -2) {

    // Create the updated header
    drawHeader();

    // Show the screen we need based on the currentScreen variable
    if (currentScreen == -1) {
      drawHome();
    }
    else if (currentScreen >= 0) {
      drawIotScreen(currentScreen);
    }
  }

  // When a touch is detected, identify the touched point
  if (touchController.isTouched(250)) {
    TS_Point p = touchController.getPoint();

    // If the screen was already on
    if (isLcdOn()) {

      // Get the cell touched
      int cellNumber = calculateTouchedCell(p.x, p.y);

      // Perform the associated action
      executeCellAction(cellNumber);
    }

    // Make sure the backlight is on
    lcdOn();

    // Update the last touch time
    lastTouch = millis();
  }

  // If the screen has been on for too long without touching, turn it off and return to the home screen
  if ((millis() > (lastTouch + config.screen.timeout * 1000)) && (isLcdOn())) {
    lcdOff();
    currentScreen = -1;
    currentPage = 1;
  }

  // Write the created data on the screen
  displayCommit(config.screen.width, config.screen.height);

  // Add a small delay to allow the MQTT loop to run correctly
  delay(10);
}

// Callback function executed whenever an MQTT message is received
void callback(char* topic, byte* payload, unsigned int length) {

  // Print a message if debug is true
  if (debug) {
    Serial.print("Received command: "); Serial.println(topic);
    Serial.print("With payload: "); Serial.println((char*)payload);
  }

  // Only proceed if on a loading screen
  if (currentScreen == -2) {

    // Convert the received string to an array
    DynamicJsonDocument devices(mqttSize);
    DeserializationError error = deserializeJson(devices, (char*)payload);
    if (error) {
      if (debug) {
        Serial.print("Error during MQTT deserialization: ");
        Serial.println(error.c_str());
      }
      return;
    }
  
    // Save each element of the array as a device in the appropriate struct
    int i = 0;
    for (JsonObject iotObj : devices.as<JsonArray>()) {
      IotDevice device;
      device.id = iotObj["id"];
      strlcpy(device.name, iotObj["name"], sizeof(device.name));
      /*if (iotObj["status"].is<int>()) {
        itoa(iotObj["status"], device.status, 10);
      }
      else {
        strlcpy(device.status, iotObj["status"], sizeof(device.status));
      }*/
      strlcpy(device.icon, iotObj["icon"] | "", sizeof(device.icon));
      strlcpy(device.color, iotObj["color"] | "", sizeof(device.color));
      currentDevices.iot[i] = device;
      i += 1;
    }
    currentDevices.iotList = i;
  
    // Set screen and page based on the received topic and print a message if debug is true
    int iotIndex;
    for (int i = 0; i < config.iotList; i++) {
      if (strcmp(topic, config.iot[i].topic.listResponse) == 0) {
        currentScreen = i;
        currentPage = 1;
        if (debug) {
          Serial.print("Received topic for IoT "); Serial.println(config.iot[i].name);
        }
        break;
      }
    }
  
    // Make sure the backlight is on
    lcdOn();
  
    // Update the last touch time
    lastTouch = millis();
  }
}

// Function to turn on the touchscreen
void lcdOn() {
  #ifdef ESP32
    digitalWrite(TFT_LED, LOW);
  #endif
  #ifdef ESP8266
    digitalWrite(TFT_LED, HIGH);
  #endif
}

// Function to turn off the touchscreen
void lcdOff() {
  #ifdef ESP32
    digitalWrite(TFT_LED, HIGH);
  #endif
  #ifdef ESP8266
    digitalWrite(TFT_LED, LOW);
  #endif
}

// Function to check if the touchscreen is on
bool isLcdOn() {
  bool lcdStatus = digitalRead(TFT_LED);
  #ifdef ESP32
    return (lcdStatus == HIGH) ? false : true;
  #endif
  #ifdef ESP8266
    return (lcdStatus == HIGH) ? true : false;
  #endif
}

// Functions to calibrate the touchscreen at first power up
void calibrateTouchScreen() {
  touchController.startCalibration(&calibration);
  while (!touchController.isCalibrationFinished()) {
    displayFill(0);
    displayAlignCenter();
    displaySetColor(1);
    displayWrite("Please calibrate\ntouch screen by\ntouch point", 120, 160);
    touchController.continueCalibration();
    displayCommit(config.screen.width, config.screen.height);
    yield();
  }
  touchController.saveCalibration();
}
void calibrationCallback(int16_t x, int16_t y) {
  displayFillCircle(x, y, 10, palette[1]);
}

// Function to show the loading screen, with the logo, a message provided and the progress bar
void drawProgress(uint8_t percentage, String text) {

  // Set the background and title font
  displayFill(config.screen.colors.mainBackground);
  displayFontTitle();
  displayAlignCenter();

  // Create the logo
  displayDrawBitmap(mainLogo, 20, 5, 200, 80, config.screen.colors.secondaryForeground);

  // Create the url
  displaySetColor(config.screen.colors.mainForeground);
  displayWrite("https://www.regafamilysite.ga", 120, 85);

  // Create the text provided
  displaySetColor(config.screen.colors.secondaryForeground);
  displayWrite(text, 120, 146);

  // Create the progress bar
  displayDrawRect(10, 168, 240 - 20, 15, config.screen.colors.mainForeground);
  displayFillRect(12, 170, 216 * percentage / 100, 11, config.screen.colors.secondaryForeground);

  // Write the result
  displayCommit(config.screen.width, config.screen.height);
}

// Function to generate the header bar with time and signal quality
void drawHeader() {

  // Set the background
  #ifdef ESP32
    displayFillRect(0, 0, config.screen.width, config.screen.headerHeight, config.screen.colors.mainBackground);
  #endif
  #ifdef ESP8266
    displayFill(config.screen.colors.mainBackground);
  #endif

  // Get the current time
  char time_str[11];
  char *dstAbbrev;
  time_t now = dstAdjusted.time(&dstAbbrev);
  struct tm * timeinfo = localtime (&now);

  // Create the time
  sprintf(time_str, "%02d:%02d",timeinfo->tm_hour, timeinfo->tm_min);
  displayFontText();
  displayAlignLeft();
  displaySetColor(config.screen.colors.mainForeground);
  displayWrite(time_str, 3, 2);

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
  displayWrite(String(quality) + "%", 205, 2);
  for (int8_t i = 0; i < 4; i++) {
    for (int8_t j = 0; j < 2 * (i + 1); j++) {
      if (quality > i * 25 || j == 0) {
        displayDrawPixel(230 + 2 * i, 11 - j, config.screen.colors.mainForeground);
      }
    }
  }
}

// Function to create a grid consisting of the configured number of rows and columns
void drawGrid() {

  // Create the separator between header and table
  #ifdef ESP32
    displayFillRect(0, config.screen.headerHeight, config.screen.grid.width, config.screen.grid.height, config.screen.colors.mainBackground);
  #endif
  displayDrawHLine(0, config.screen.headerHeight, config.screen.grid.width, config.screen.colors.mainForeground);

  // Create the columns
  int startX = 0;
  while (true) {
    startX = startX + config.screen.grid.cellWidth;
    if (startX > config.screen.grid.width) {
      break;
    }
    displayDrawVLine(startX, config.screen.headerHeight, config.screen.grid.height, config.screen.colors.mainForeground);
  }

  // Create the rows
  int startY = config.screen.headerHeight;
  while (true) {
    startY = startY + config.screen.grid.cellHeight;
    if (startY > config.screen.grid.width) {
      break;
    }
    displayDrawHLine(0, startY, config.screen.grid.width, config.screen.colors.mainForeground);
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

// Functions to update the contents of a cell with text, image or color
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

  // Write the text provided
  displayFontText();
  displayAlignCenterMiddle();
  displaySetColor(config.screen.colors.mainForeground);
  displayWrite(text, centerX + offsetX, centerY + offsetY);
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

  // Draw the image provided
  displayDrawBitmap(myIcons[index], centerX - (config.screen.iconsSize / 2), centerY - (config.screen.iconsSize / 2), config.screen.iconsSize, config.screen.iconsSize, config.screen.colors.mainForeground);
}
void updateCell(int cellNumber, char* color) {

  // Get the row and column number from the cell number
  int row = 0;
  int col = 0;
  cellToGrid(cellNumber, row, col);

  // Get the coordinates of the cell vertices
  int minX = config.screen.grid.cellWidth * (col - 1);
  int maxX = minX + config.screen.grid.cellWidth;
  int minY = config.screen.grid.cellHeight * (row - 1) + config.screen.headerHeight;
  int maxY = minY + config.screen.grid.cellHeight;

  // Fill with the color provided
  int i = 0;
  for (char* colorName : colorNames) {
    if (strcmp(colorName, color) == 0) {
      break;
    }
    i++;
  }
  displayFillRect(minX, minY, config.screen.grid.cellWidth, config.screen.grid.cellHeight, i);
}

// Function to show the main screen with the grid and icons of the different cells
void drawHome() {
  drawGrid();

  // Calculate the maximum number of cells on a page
  int maxCells = config.screen.grid.rows * config.screen.grid.cols;

  // Check if pagination is required
  bool needsPagination = config.iotList > maxCells;

  // Define the variable for the cell we are currently updating
  int currentCell = 1;

  // If pagination is required and the current page is not the first one, show the "Back" icon in the first cell and decrease the maximum number of cells that can be used
  if ((needsPagination) && (currentPage > 1)) {
    updateCell(currentCell, getImageIndex("back"));
    maxCells--;
    currentCell++;
  }

  // Determine which device to view from
  int minIot = maxCells * (currentPage - 1);
  if (currentPage > 2) { minIot--; }

  // Calculate the lesser of the total number of configured devices and the total number of displayable cells
  int limit = min(config.iotList, maxCells);

  // For each real IoT device configured
  for (int i = 0; i < limit; i++) {

    // If this is the last cell on the page and there are subsequent devices, show the "Next" icon
    if ((i == (limit - 1)) && (minIot < (config.iotList - 1))) {
      updateCell(currentCell, getImageIndex("next"));
    }

    else {

      // Look for the image
      int imgIndex = getImageIndex(config.iot[minIot].icon);
      bool hasImage = imgIndex > -1;

      // If an image is available show it together with the name, otherwise show only the center-aligned name
      if (hasImage) {
        updateCell(currentCell, imgIndex);
        updateCell(currentCell, String(config.iot[minIot].name), 0, 40);
      }
      else {
        updateCell(currentCell, String(config.iot[minIot].name));
      }
    }

    // Increment the current cell number and the current IoT device number
    currentCell++;
    minIot++;
  }
}

void drawIotScreen(int currentScreen) {
  drawGrid();

  // Calculate the maximum number of cells on a page and the maximum number of devices to show
  int maxCells = config.screen.grid.rows * config.screen.grid.cols;

  // Check if pagination is required
  bool needsPagination = currentDevices.iotList > maxCells;

  // Define the variable for the cell we are currently updating
  int currentCell = 1;

  // If pagination is required and the current page is not the first one, show the "Back" icon in the first cell and decrease the maximum number of cells that can be used
  if ((needsPagination) && (currentPage > 1)) {
    updateCell(currentCell, getImageIndex("back"));
    maxCells--;
    currentCell++;
  }

  // Determine which device to view from
  int minIot = maxCells * (currentPage - 1);
  if (currentPage > 2) { minIot--; }

  // Calculate the lesser of the total number of received devices and the total number of displayable cells
  int limit = min(currentDevices.iotList, maxCells);

  // For each real IoT device received
  for (int i = 0; i < limit; i++) {

      // Look for the device image
      int imgIndex = -1;
      imgIndex = getImageIndex(currentDevices.iot[minIot].icon);
      if (imgIndex == -1) {
        imgIndex = getImageIndex(config.iot[currentScreen].icon);
      }

    // If this is the last cell on the page and there are subsequent devices, show the "Next" icon
    if ((i == (limit - 1)) && (minIot < (currentDevices.iotList - 1))) {
      updateCell(currentCell, getImageIndex("next"));
    }
    else {

      // Only on ESP32 (because of RAM limitations), if a background color is available, fill the cell
      #ifdef ESP32
        if (strcmp(currentDevices.iot[minIot].color, "") != 0) {
          updateCell(currentCell, currentDevices.iot[minIot].color);
        }
      #endif

      // If an image is available show it together with the name, otherwise show only the center-aligned name
      if (imgIndex > -1) {
        updateCell(currentCell, imgIndex);
        updateCell(currentCell, String(currentDevices.iot[minIot].name), 0, 40);
      }
      else {
        updateCell(currentCell, String(currentDevices.iot[minIot].name));
      }
    }

    // Increment the current cell number and the current IoT device number
    currentCell++;
    minIot++;
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

  // Get the cell number from the array, print a message if the debug is true, and then return it
  int cellNumber = cells[touchedRow][touchedCol];
  if (debug) {
    Serial.print("Touched row "); Serial.print(touchedRow + 1); Serial.print(" and col "); Serial.println(touchedCol + 1);
    Serial.print("Cell number is: "); Serial.println(cellNumber);
  }
  return cellNumber;
}

// Function to perform an action based on the cell touched and the current screen
void executeCellAction(int cellNumber) {

  // Calculate the maximum number of cells on a page
  int maxCells = config.screen.grid.rows * config.screen.grid.cols;
  int lastCell = maxCells;

  // Check if pagination is required
  bool needsPagination = config.iotList > maxCells;

  // If pagination is required, the current page is not the first one and the first cell (corresponding to "Back") has been touched, decrease the number of the current page and print a message if debug is true
  if ((needsPagination) && (currentPage > 1) && (cellNumber == 1)) {
    if (debug) {
      Serial.println("Back cell touched: decreasing page");
    }
    currentPage--;
    return;
  }

  // If pagination is required, the current page is not the last one and the last cell (corresponding to "Next") has been touched, increase the number of the current page and print a message if debug is true
  if ((needsPagination) && (currentPage > 1)) { maxCells--; }
  int minIot = maxCells * (currentPage - 1);
  if (currentPage > 2) { minIot--; }
  bool isLastPage = (minIot + maxCells) >= config.iotList;
  if ((needsPagination) && (!isLastPage) && (cellNumber == lastCell)) {
    if (debug) {
      Serial.println("Next cell touched: increasing page");
    }
    currentPage++;
    return;
  }

  // Get the touched element number
  int currentElement = minIot + cellNumber;
  if (currentPage > 1) { currentElement--; }
  if (debug) {
    Serial.print("Touched IoT device "); Serial.println(currentElement);
  }

  // If the current screen is the home screen, send the device request on the topic associated with the cell, show loading screen and print a message if debug is true
  if (currentScreen == -1) {
    const char* topic = config.iot[currentElement - 1].topic.listRequest;
    if (strcmp(topic, "") != 0) {
      client.publish(topic, "");
      currentScreen = -2;
      drawProgress(50, "Loading devices");
      if (debug) {
        Serial.print("Sending request to "); Serial.println(topic);
      }
    }
  }
  // If current screen is one of the device lists, send the status request on the topic associated with the cell, show loading screen and print a message if debug is true
  if ((currentScreen >= 0) && (currentScreen < 100)) {
    const char* templateTopic = config.iot[currentScreen].topic.statusRequest;
    if (strcmp(templateTopic, "") != 0) {
      char topic[64];
      char id[8];
      sprintf(id, "%d", currentDevices.iot[currentElement - 1].id);
      sprintf(topic, templateTopic, id);
      client.publish(topic, "");
      currentScreen = -2;
      drawProgress(50, "Loading statuses");
      if (debug) {
        Serial.print("Sending request to "); Serial.println(topic);
      }
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

        // Subscribe to each "*Response" topic
        for (int i = 0; i < config.iotList; i++) {
          client.subscribe(config.iot[i].topic.listResponse);
          char topic[64];
          const char* templateTopic = config.iot[i].topic.statusResponse;
          sprintf(topic, templateTopic, "+");
          client.subscribe(topic);
        }

        // Increase buffer size
        client.setBufferSize(mqttSize);
      }
      delay(500);
    }
    if (debug) {
      Serial.println("Connected to MQTT");
      Serial.print("Buffer size is: "); Serial.println(client.getBufferSize());
    }
  }
}
