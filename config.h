/***** Advanced Configuration *****/
  const int maxDeviceTypes = 8;
/***** Advanced Configuration *****/

#ifdef ESP32
  #include <WiFi.h>
  #include "FS.h"
#endif
#ifdef ESP8266
  #include <ESP8266WiFi.h>
#endif
#include <ArduinoJson.h>

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

struct Time {
  int utcOffset;
  char ntpServer[32];
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

struct Topics {
  char listRequest[64];
  char listResponse[64];
  char statusRequest[64];
  char statusResponse[64];
};

struct Iot {
  char name[32];
  char icon[16];
  struct Topics topic;
};

struct Config {
  struct Wifi wifi;
  struct Mqtt mqtt;
  struct Device device;
  struct Time time;
  struct Screen screen;
  struct Iot iot[maxDeviceTypes];
  int iotList;
};

extern bool loadConfiguration(Config &config, File &file, int configSize);
