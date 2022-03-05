#include "TouchControllerWS.h"
#ifdef ESP32
  #include "FS.h"
  #include "SPIFFS.h"
#endif
#include "../advancedconfig.cpp"

TouchControllerWS::TouchControllerWS(XPT2046_Touchscreen *touchScreen) {
  this->touchScreen = touchScreen;
}

int TouchControllerWS::getCalibration(JsonObject &calibration) {

  // Search for calibration file in SPIFFS and exit if not found
  SPIFFS.begin();
  if (!SPIFFS.exists("/calibration.txt")) {
    if (debug) {
      Serial.println("No calibration file found");
    }
    return 1;
  }

  // If file is found, try to decode it and exit on error
  File f = SPIFFS.open("/calibration.txt", "r");
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, f);
  if (error) {
    if (debug) {
      Serial.print("Error during calibration deserialization: ");
      Serial.println(error.c_str());
    }
    f.close();
    return 2;
  }

  // If decoding went well, search for current orientation object and exit if not found
  String orientation = (displayWidth > displayHeight) ? "landscape" : "portrait";
  JsonObject orientationObj = doc.as<JsonObject>();
  if (!orientationObj.containsKey(orientation)) {
    if (debug) {
      Serial.println("No valid orientation data found");
    }
    f.close();
    return 3;
  }

  // If object is found, write it into the given variable and terminate
  calibration = orientationObj[orientation].as<JsonObject>();
  f.close();
  return 0;
}

bool TouchControllerWS::loadCalibration(int w, int h) {

  // Save given sizes in object properties
  displayWidth = w;
  displayHeight = h;

  // Try to get calibration object and exit if not found
  JsonObject calibration;
  if (getCalibration(calibration) != 0) {
    return false;
  }

  // Save read calibration values in object properties
  dx = calibration["dx"];
  dy = calibration["dy"];
  ax = calibration["ax"];
  ay = calibration["ax"];
  return true;
}

bool TouchControllerWS::saveCalibration() {

  // Initialize SPIFFS
  SPIFFS.begin();

  // Try to get calibration data for current orientation
  JsonObject data;
  int result = getCalibration(data);

  // Initialize a JSON document
  DynamicJsonDocument calibration(256);

  // Only if returned result is 3 (so, a valid json is present, but not for the current orientation), deserialize other data
  if (result == 3) {
    File temp = SPIFFS.open("/calibration.txt", "r");
    deserializeJson(calibration, temp);
    temp.close();
  }

  // Only if returned result is not 0 (so, there's no data for current orientation already), create a new child object.
  // If 0 is returned, a valid object will already be present on data as a result of the getCalibration function
  if (result != 0) {
    String direction = (displayWidth > displayHeight) ? "landscape" : "portrait";
    data = calibration.createNestedObject(direction);
  }

  // Open calibration file and exit on error
  File calibrationFile = SPIFFS.open("/calibration.txt", "w");
  if (!calibrationFile) {
    if (debug) {
      Serial.println("Cannot write to calibration file");
    }
    return false;
  }

  // Now, let's populate the data and write back to the file
  data["dx"] = dx;
  data["dy"] = dy;
  data["ax"] = ax;
  data["ay"] = ay;
  serializeJson(calibration, calibrationFile);
  if (debug) {
    Serial.print("Calibration data updated: ");
    serializeJson(calibration, Serial);
    Serial.println("");
  }
  calibrationFile.close();
}

void TouchControllerWS::startCalibration(CalibrationCallback *calibrationCallback) {
  state = 0;
  this->calibrationCallback = calibrationCallback;
}

void TouchControllerWS::continueCalibration(int w, int h) {
    TS_Point p = touchScreen->getPoint();

    displayWidth = w;
    displayHeight = h;

    if (state == 0) {
      (*calibrationCallback)(10, 10);
      if (touchScreen->touched()) {
        p1 = p;
        state++;
        lastStateChange = millis();
      }

    } else if (state == 1) {
      (*calibrationCallback)(displayWidth - 10, displayHeight - 10);
      if (touchScreen->touched() && (millis() - lastStateChange > 1000)) {

        p2 = p;
        state++;
        lastStateChange = millis();
        dx = (float)displayWidth / abs(p1.y - p2.y);
        dy = (float)displayHeight / abs(p1.x - p2.x);
        ax = p1.y < p2.y ? p1.y : p2.y;
        ay = p1.x < p2.x ? p1.x : p2.x;
      }

    }
}
bool TouchControllerWS::isCalibrationFinished() {
  return state == 2;
}

bool TouchControllerWS::isTouched() {
  touchScreen->touched();
}

bool TouchControllerWS::isTouched(int16_t debounceMillis) {
  if (touchScreen->touched() && millis() - lastTouched > debounceMillis) {
    lastTouched = millis();
    return true;
  }
  return false;
}
TS_Point TouchControllerWS::getPoint() {
    TS_Point p = touchScreen->getPoint();
    int x = (p.y - ax) * dx;
    int y = displayHeight - (p.x - ay) * dy;
    p.x = x;
    p.y = y;
    return p;
}
