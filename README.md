# AZ-Touch IoT Controller

This project allows you to create a controller for IoT devices using the AZ-Delivery ArduiTouch kit ([buy it here](https://www.az-delivery.de/en/products/az-touch-wandgehauseset-mit-touchscreen-fur-esp8266-und-esp32))

## Configuration

The main configuration is done via a json file. Copy the *config.sample.json* file available in this repository, save it to an SD card and rename it to *config.txt*; now change the parameters contained in it according to your needs.

The file consists of five main sections:

### Wifi

Here you can configure the connection to your local network.

```json
"wifi": {
  "ssid": "ssid",
  "password": "password",
  "ip": "127.0.0.1",
  "gateway": "127.0.0.1",
  "subnet": "127.0.0.1",
  "dns": "127.0.0.1"
 }
 ```

- **ssid**: This is the name of your local wifi network. (Please note: maximum length supported: 32 characters)
- **password**: This is the password to allow connection to your local network. Please note: maximum length supported: 64 characters)
- **ip**: If you want to assign a static IP to your device you can enter it here
- **gateway**: If you have configured a static IP, you must enter your gateway address here.
- **subnet**: If you have configured a static IP, you must enter the subnet mask here.
- **dns**: If you are having trouble connecting to the world wide web, enter a public DNS server address here (e.g. Google's, 8.8.8.8).

### MQTT

Here you can configure the connection to the MQTT broker to which the devices you want to control are connected.

```json
 "mqtt": {
  "host": "127.0.0.1",
  "user": "user",
  "password": "password"
 }
 ```

- **host**: This is the IP address or hostname of the server on which the MQTT broker is listening. The port used for the connection is always the standard 1883 one. (Please note: maximum length supported: 32 characters)
- **user**: This is the username used to authenticate to the broker if needed. (Please note: maximum length supported: 64 characters)
- **password**: This is the password used to authenticate to the broker if needed. (Please note: maximum length supported: 64 characters)

### Device

Here you can uniquely configure the device to distinguish it from any other similar ones.

```json
 "device": {
  "id": "1"
 }
 ```

- **id**: This must be a numeric id of your choice different from any other id used in devices with this same code on the same network. (Please note: maximum length supported: 8 characters)

### Time

Here you can configure the settings for time synchronization on your device

```json
"time": {
  "utcOffset": 0,
  "ntpServer": "your.ntp.server"
 }
```

- **utcOffset**: An integer representing the difference in hours between your local time and UTC
- **ntpServer**: This is the NTP server to connect to for time synchronization. Choose one geographically as close to you as possible for better accuracy. (Please note: maximum length supported: 32 characters)

### Screen

Here you can configure the appearance of your device's screen.

```json
"screen": {
  "landscape": true,
  "darkMode": true,
  "maxColsLandscape": 1,
  "maxRowsLandscape": 1,
  "iconsSize": 1,
  "timeout": 1,
  "headerHeight": 1
 }
 ```

- **landscape**: A boolean value. Define whether your screen is in landscape (true) or portrait (false) mode.
- **darkMode**: A boolean value. Define whether your screen should use the dark (true) or light (false) color set.
- **maxColsLandscape** and **maxRowsLandscape**: Integers indicating respectively the maximum number of columns and rows used by the display grid when the device is in landscape mode. If used in portrait mode, these two values will be automatically swapped.
- **iconsSize**: An integer representing the length in pixels of the side of each icon displayed on the screen (icons must be square).
- **timeout**: An integer representing the number of seconds after which the backlight will turn off if no action is taken.
- **headerHeight**: An integer representing the height in pixels of the header bar containing the title, time and signal quality.

### IoT

This is the main element of the configuration. An array containing an object for each type of device that you want to manage (e.g. an object for sockets, an object for lights, an object for curtains, etc.).

```json
"iot": [
  {
   "name": "Object name",
   "icon": "icon-name",
   "topic": {
    "listRequest": "/Your/Topic/Here",
    "listResponse": "/Your/Topic/Here"
   }
  },
  {
    ...
  }
]
```

Each object contained in the array will contain the following properties:

- **name**: The name assigned to the type of device that will be displayed on the screen. (Please note: maximum length supported: 32 characters)
- **icon**: The name of the icon to be associated with the type of device displayed on the screen, chosen from those available in the *icons.cpp* file.
- **topic**: An object containing in turn the following properties:
  - **listRequest**: The topic that will be called when a device type is selected from the home screen. You must program your server so that upon receiving a publication on this topic it generates a list of all devices of that type (e.g. a list of all the sockets connected to the broker and their status).
  - **listResponse**: The topic in which the list of devices generated in the previous point will be published and to which this controller will have to subscribe to stay listening.

## Known issues

While saving calibration, on Wemos D1 Mini, the board will restart with a "Cannot write to calibration file" error. Despite this, writing will be successful and all data will be saved in the calibration file.
