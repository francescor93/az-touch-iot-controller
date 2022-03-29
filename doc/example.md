# Operation example

As stated in the [README](https://github.com/francescor93/az-touch-iot-controller/blob/main/README.md) file, this sketch is designed to be highly customizable and thus adapt to your needs and your network infrastructure.  
Following is an example of my setup, to help you understand how it can be used and configured.

## What do I use

I uploaded this sketch on the bigger of the compatible boards, the [ESP32 Dev Kit C](https://www.az-delivery.de/en/products/esp32-developmentboard), which also supports the use of device colors.
In my network I have several Arduino Nano 33 IoT and ESP8266-01 that manage the different devices (curtains, air conditioners, etc.).  
A Python script listens for communications between all devices making sure the correct messages are exchanged between each.  
A MySQL database is used to save the list of devices present and the current status.

## How I changed the configuration files

Following the instructions in the README file, I modified the json configuration file to best match my needs. In particular, I have included in the `IoT` section the individual types of devices that I want to control (*Relays*, *Curtains*, *Air Conditioners*, *LED Strips*, *Environmental sensors*, *Emergency batteries*) as well as the topics I want to use to obtain the lists of devices or commands.  
For example, for relay-type devices, I set up my infrastructure so that when the controller publishes to the */Relay/0/StatusCommand* topic, an array is published in response by the Python script to the */Relay/0/StatusReply* topic containing the data of all connected relay-type devices, and when the controller publishes to the  */Relay/1/StatusCommand* topic, an array is published in response by the Python script to the */Relay/1/StatusReply* topic containing the available statuses for that relay (in this case *On* or *Off*).

Below is my complete configuration file, for clarity. I just blacked out sensitive information for obvious reasons.

```json
{
  "wifi": {
    "ssid": "**********",
    "password": "**********",
    "ip": "**********",
    "gateway": "**********",
    "subnet": "**********",
    "dns": "8.8.8.8"
  },
  "mqtt": {
    "host": "**********",
    "user": "**********",
    "password": "**********"
  },
  "device": {
    "id": "1"
  },
  "time": {
    "utcOffset": 1,
    "ntpServer": "it.pool.ntp.org"
  },
  "screen": {
    "landscape": false,
    "darkMode": true,
    "maxColsLandscape": 3,
    "maxRowsLandscape": 2,
    "iconsSize": 50,
    "timeout": 15,
    "headerHeight": 15
  },
  "iot": [
    {
      "name": "Relay",
      "icon": "toggle-on",
      "topic": {
        "listRequest": "/Relay/0/StatusCommand",
        "listResponse": "/Relay/0/StatusReply",
        "statusRequest": "/Relay/%s/StatusCommand",
        "statusResponse": "/Relay/%s/StatusReply"
      }
    },
    {
      "name": "Curtain",
      "icon": "curtain-closed",
      "topic": {
        "listRequest": "/Curtain/0/StatusCommand",
        "listResponse": "/Curtain/0/StatusReply",
        "statusRequest": "/Curtain/%s/StatusCommand",
        "statusResponse": "/Curtain/%s/StatusReply"
      }
    },
    {
      "name": "Air conditioner",
      "icon": "ac-cool",
      "topic": {
        "listRequest": "/AirConditioner/0/StatusCommand",
        "listResponse": "/AirConditioner/0/StatusReply",
        "statusRequest": "/AirConditioner/%s/StatusCommand",
        "statusResponse": "/AirConditioner/%s/StatusReply"
      }
    },
    {
      "name": "LED strip",
      "icon": "lamp-on",
      "topic": {
        "listRequest": "/LedStrip/0/StatusCommand",
        "listResponse": "/LedStrip/0/StatusReply",
        "statusRequest": "/LedStrip/%s/StatusCommand",
        "statusResponse": "/LedStrip/%s/StatusReply"
      }
    },
    {
      "name": "Environment",
      "icon": "therm-half",
      "topic": {
        "listRequest": "/EnvironmentalSensor/0/StatusCommand",
        "listResponse": "/EnvironmentalSensor/0/StatusReply",
        "statusRequest": "/EnvironmentalSensor/%s/StatusCommand",
        "statusResponse": "/EnvironmentalSensor/%s/StatusReply"
      }
    },
    {
      "name": "Battery",
      "icon": "batt-half",
      "topic": {
        "listRequest": "/BatteryMeter/0/StatusCommand",
        "listResponse": "/BatteryMeter/0/StatusReply",
        "statusRequest": "/BatteryMeter/%s/StatusCommand",
        "statusResponse": "/BatteryMeter/%s/StatusReply"
      }
    }
  ]
}
```

As for the advanced configuration file, I have not changed any parameters, just leaving the default ones:

```c++
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
static bool debug = false
```

## How does it work

Let's take two examples of real cases.

### Control a curtain

As seen in the configuration above, one of the types of devices that I have decided to control through this sketch is a curtain.

When I tap on the *Curtain* icon, the controller publishes a new message to the

```text
/Curtain/0/StatusCommand
```

topic, containing no data.  
The Python script is subscribed to this topic, so when it receives it, after extracting the message type and the device name, it queries the database to look for all available curtains and their information.
The following code block shows reading from the database and subsequent publication. For the sake of brevity, the previous parts of extracting the message type and curtain number are omitted.

```python
elif messageType == "StatusCommand":
 if curtainNumber == "0":
  result = subDatabase.dbQuery(
   "SELECT id, alias AS name, CASE WHEN status = 0 THEN 'cyan' WHEN status = 1 THEN 'blue' WHEN status = 2 THEN 'red' END AS color, CASE WHEN status = 0 THEN 'curtain-open' WHEN status = 1 THEN 'curtain-closed' WHEN status = 2 THEN 'curtain-open' END AS icon FROM curtains", True)
 [...]
 client.publish("/Curtain/" + curtainNumber +
     "/StatusReply", json.dumps(result))
```

In this case, each device can have three statuses and colors: 0 (open, color: cyan), 1 (closed, color: blue), 2 (error, color: red) and two icons: *curtain-open* (when open or in error), *curtain-closed* (when closed). Pay attention that the colors and icons are chosen from those available in the `icons.cpp` file.  
When the query has been run, a publication on the

```text
/Curtain/0/StatusReply
```

topic takes place, having as payload the result in json format, in my case like this:

```json
[{"id": 1, "name": "Camera Francesco", "color": "cyan", "icon": "curtain-open"}]
```

The controller receives back this json and saves it in memory, showing the second level page containing the list of all the curtains (in my case, only one), which will have the chosen color and icon, if available.  
By touching the cell containing this curtain, another publication will take place: in this case the used topic is the one defined as `statusRequest`, which is */Curtain/%s/StatusCommand*, but the `%s` string is replaced on the fly with the identifier of the touched device, which according to the previous payload is *1*. So the publication is performed on the

```text
/Curtain/1/StatusCommand
```

topic, with no data.  
In this case the listening Python script creates a new json statically containing the available statuses for this device, which are *Open* and *Closed*, and the related topics and payloads to recall them. The code used is the following:

```python
elif messageType == "StatusCommand":
 [...]
 else:
  result = [{'name': 'Aperto', 'topic': '/Curtain/' + curtainNumber + '/OutputCommand', 'data': '0'},
     {'name': 'Chiuso', 'topic': '/Curtain/' + curtainNumber + '/OutputCommand', 'data': '1'}]
 client.publish("/Curtain/" + curtainNumber +
     "/StatusReply", json.dumps(result))
```

So the response will be published on the

```text
/Curtain/1/StatusReply
```

topic, with the following payload:

```json
[{"name": "Aperto", "topic": "/Curtain/1/OutputCommand", "data": "0"}, {"name": "Chiuso", "topic": "/Curtain/1/OutputCommand", "data": "1"}]
```

The controller receives back also this json and saves it in memory, showing the third level page containing the list of all the available statuses. From here, by touching the cell of a status, we can directly send the command to recall that status; so, for example, by tapping the first cell, a new message will be published on the

```text
/Curtain/1/OutputCommand
```

topic, with payload:

```text
0
```

As you can see, *topic* and *payload* are the ones previously received.  
According to how I have set up my infrastructure and devices, this last topic is not received by the Python script, but directly by the final device, the curtain, which will open. Of course you can set up this part of the project according to your needs.  
After publishing to this topic, the controller will show a confirmation screen for a few seconds, before returning to the home screen.

### Control an environmental sensor

Another type of device that I have configured is an environmental sensor: an *Arduino Nano 33 IoT* connected to a *DHT22* sensor and a *BMP280* sensor that allows me to read temperature, humidity and pressure from the room. Of course, this type of device will not have statuses that can be activated, but only the possibility of showing the collected data. So let's see how to do it.

When I tap on the *Environment* icon, the controller publishes a new message to the

```text
/EnvironmentalSensor/0/StatusCommand
```

topic, containing no data.  
The Python script is subscribed to this topic, so when it receives it, after extracting the message type and the device name, it queries the database to look for all available environmental sensors and their information.
The following code block shows reading from the database and subsequent publication. For the sake of brevity, the previous parts of extracting the message type and sensor number are omitted.

```python
elif messageType == "StatusCommand":
 if sensorNumber == "0":
  result = subDatabase.dbQuery(
     "SELECT id, alias AS name, 'temp-half' AS icon FROM environment_sensors", True)
  [...]
 client.publish("/EnvironmentalSensor/" + sensorNumber +
     "/StatusReply", json.dumps(result))
```

In this case, only the device id and name are extracted from the database, and the icon is statically defined. These data are then published on the 

```text
/EnvironmentalSensor/0/StatusReply
```

topic as a json payload like this

```json
[{"id": 1, "name": "Sala", "icon": "temp-half"}, {"id": 2, "name": "Camera Francesco", "icon": "temp-half"}]
```

The controller receives back this json and saves it in memory, showing the second level page containing the list of all the environmental sensors.  
By touching, for example, the first cell, corresponding to the first device in the received array, another publication will take place: in this case the used topic is the one defined as `statusRequest`, which is */EnvironmentalSensor/%s/StatusCommand*, but the `%s` string is replaced on the fly with the identifier of the touched device, which according to the previous payload is *1*. So the publication is performed on the topic

```text
/EnvironmentalSensor/1/StatusCommand
```

with no data.  
In this case, the Python script will query the database to extract the values of the last reading and merge them into a single json of data, which will in turn be inserted into a json containing the sensor information.
```python
elif messageType == "StatusCommand":
 [...]
 else:
  result = subDatabase.dbQuery(
     "SELECT environment_sensors.alias AS name, JSON_OBJECT('Temperatura', CONCAT(CAST(environment_data.temperature AS NCHAR), '°C'), 'Umidità', CONCAT(CAST(environment_data.humidity AS NCHAR), '%'), 'Pressione', CONCAT(CAST(environment_data.pressure AS NCHAR), ' hPa'), 'Aggiornamento', DATE_FORMAT(environment_data.created_at, '%d/%m/%Y, %H:%i')) AS data FROM environment_data JOIN environment_sensors ON environment_data.sensor_id = environment_sensors.id WHERE environment_data.sensor_id = '{}' ORDER BY environment_data.created_at DESC LIMIT 1".format(sensorNumber), True)
 client.publish("/Curtain/" + curtainNumber +
     "/StatusReply", json.dumps(result))
```

So the response published on the

```text
/EnvironmentalSensor/1/StatusReply
```

topic will be like this:

```json
[{"name": "Sala", "data": "{\"Temperatura\": \"24.72\u00b0C\", \"Umidit\u00e0\": \"35.90%\", \"Pressione\": \"1029.53 hPa\", \"Aggiornamento\": \"27/03/2022, 21:23\"}"}]
```

As you can see this payload, unlike that of the previous example, doesn't contain a `topic` object, and the array is composed of a single element: when both these conditions are met, the controller won't show a third level page containing the cells with some commands, but will only show on the screen the value of the `data` object, appropriately decoded.  
This information will remain visible for the time set as `timeout`, after which the backlight will turn off and the controller will return to the home screen.
