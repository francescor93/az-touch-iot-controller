#include <simpleDSTadjust.h>
#ifndef ADVANCEDCONFIG
#define ADVANCEDCONFIG

/***** Advanced Configuration *****/
  static struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600};
  static struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};
  static int configSize = 2395;
  static int mqttSize = 512;
  static const int maxDevices = 8;
  static const int maxDeviceTypes = 8;
  static const int maxDeviceStatuses = 8;
  static const char* filename = "/config.txt";
  static const char* DEVICE = "TouchController";
  static const char* DELIMITER = "/";
  static bool debug = true;
/***** Advanced Configuration *****/

#endif
