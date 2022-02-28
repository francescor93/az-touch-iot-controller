#include "TouchControllerWS.h"
#ifdef ESP32
  #include "FS.h"
  #include "SPIFFS.h"
#endif

TouchControllerWS::TouchControllerWS(XPT2046_Touchscreen *touchScreen) {
  this->touchScreen = touchScreen;
}

bool TouchControllerWS::loadCalibration(int w, int h) {
  displayWidth = w;
  displayHeight = h;
  bool result = SPIFFS.begin();
  if (!SPIFFS.exists("/calibration.txt")) {
    return false;
  }
  else {
    File f = SPIFFS.open("/calibration.txt", "r");
    String dxStr = f.readStringUntil('\n');
    String dyStr = f.readStringUntil('\n');
    String axStr = f.readStringUntil('\n');
    String ayStr = f.readStringUntil('\n');
    dx = dxStr.toFloat();
    dy = dyStr.toFloat();
    ax = axStr.toInt();
    ay = ayStr.toInt();
    f.close();
  }
}

bool TouchControllerWS::saveCalibration() {
  bool result = SPIFFS.begin();

  // open the file in write mode
  File f = SPIFFS.open("/calibration.txt", "w");
  if (!f) {
    Serial.println("file creation failed");
  }
  // now write two lines in key/value style with  end-of-line characters
  f.println(dx);
  f.println(dy);
  f.println(ax);
  f.println(ay);

  f.close();
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
