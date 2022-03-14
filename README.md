# MiniQttSensor: A Humidity and Temperature sensing IoT application

A fully functional IoT application for temperature and humidity sensing and data transfer over SSL-encrypted MQTT. Client certificate is not required.

Deploy it on several devices to collect (and store) data from multiple locations.

The app targets all ESP8266 and ESP32 boards. It has been tested with:
- Wemos WiFi-ESP8266 DevKit
- D1 Mini Pro Based on ESP8266EX
- ESP32 DevKit

Use your private or any public MQTT broker.

Includes an (optional) "accumulator" client, which polls all data into a MySQL database and bins older data from previously disconnected instances.

The app and the client have been built and tested on Linux. 
The app should work on other architectures, but the client might not.
## Hardware requirements

- One, or more ESP8266- or ESP32-based boards,
- One sht3x sensor per board and wires (four wires per sensor),
- Wifi connection (and access to Internet if using a LAN-external broker)
- 5V or 3.3V power source (5V, if applicable for a board, works better with WiFi).

This set-up is sufficient to build the app but additional items might be required depending on the deployment decisions:
- a rig for hosting the MQTT broker (Raspberry Pi, Cloud, ...)
- device(s) to read/display data (a smartphone MQTT app, the included client, ...)

## Software prerequisited
This app has to be built using a board-specific  [Software Development Kit](https://www.espressif.com/en/products/sdks/esp-idf "ESP IDF by Espressif"):
- [*ESP IDF*](https://github.com/espressif/esp-idf "ESP-IDF on Github") for ESP32-based boards,
- [*ESP8266 RTOS SDK*](https://github.com/espressif/ESP8266_RTOS_SDK "ESP8266 RTOS SDK on Github") for ESP8266-based boards.

To build the (optional) polling client the following are required:
- Python (see `client/requirements.txt` for the list of necessary modules)
- MySQL or similar. The client must have full access to one database.
- Systemd (optional) is used as a default deployment method but it is possible to run the script as a standalone.

To deploy to a dedicated MQTT broker, one has to be able to setup and run a server.

A public MQTT broker ([such as this one](https://test.mosquitto.org "public MQTT broker")) requires neigher system administration  competences nor setting up, and could be a viable alternative at least for testing.

To enjoy the outputs of this app, a MQTT client is required. The easiest is to use a MQTT client on a smartphone (just search on the market, I'm sure there are several options available).

## Install and build the project
### Get the SDK
This app has to be built using a board-specific SDK. 
The roadmap for the SDK installation is described [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#installation-step-by-step "install and setup ESP IDF"), for ESP32, and [here](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/get-started/index.html#setup-toolchain "install and setup ESP8266 RTOS SDK"), for ESP8266.

Follow the relevant instructions to
- Install prerequisites for Windows, Linux, or macOS;
- Get the relevant SDK's source code (ESP IDF or ESP8266 RTOS SDK);
- Set up the toolchain;
- Set up the environment variables.

### Get the source code
Clone (or fork) this repository:
```sh
git clone https://github.com/rytis-paskauskas/MiniQttSensor
```

### Build and flash
This projects can be built using the standard workflow for a given SDK; see [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-6-connect-your-device "ESP IDF build workflow") and [here](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/get-started/index.html#connect "ESP8266 RTOS SDK build workflow") for ESP32 and ESP8266 boards, respectively. Although the underlying build tools are different for the two SDKs, the workflows are almost identical and differ only in semantics (using `make` vs. `idf.py`, for example).

Things could potentially get messy in the case of building for both types of boards.
This project adopts several simple idiosynracies, described below, with the purpose of mitigating potential issues.

The first issue is that the `sdkconfig` files are incompatible between the two SDKs. The two possible approaches are 
- start without a `sdkconfig` file and let the build tool generate a default one, or
- use one of the default templates `sdkconfig.esp8266default` or `sdkconfig.esp32default`, for example `cp sdkconfig.esp8266default sdkconfig; make menuconfig`.

For the same reason, the project is configured to use a non-standard build directory,  `build_esp8266` for ESP8266, and `build` (not `build_esp32`, because I can't get it to work cleanly with CMake) for ESP32.

See also `Makefile` and the `CMakeLists.txt` for respective control statements.

## Clients
### Data
The payload is formatted as a JSON string
```text
{measurement:{temperature: xx.xx,humidity: yy.yy}}
```
(and is exactly 57 bytes long).

### Deploying `myqttsense_client`
1. Configure the "Client" section in Kconfig.
2. Install python prerequisites and install the client
```sh
sudo pip install -r client/requirements.txt
# or
python -m pip install -r client/requirements.txt
# and then
install_client.sh
```
3. Start (and enable) service
```sh
systemctl --user start myqttsense_client.service
# optionally also
systemctl --user enable myqttsense_client.service
```
4. Debug/troubleshoot `myqttsense_client`
```sh
systemctl --user status myqttsense_client.service
journalctl --user -f
```
5. Check out the database(s) using command line (`mysql -u USER -p PWD -h HOST`) or `phpmyadmin` or equivalent tools.

The typical output from `journalctl -f --user`
```text
Mar 14 18:18:37 myhost systemd[1124]: Started MyQTTSense Linux client.
Mar 14 18:18:37 myhost /my/home/dir/.local/bin/myqttsense_client.py[38015]: [INFO] Connected to MQTT broker
Mar 14 18:18:37 myhost /my/home/dir/.local/bin/myqttsense_client.py[38015]: [INFO] Connected to SQL database
Mar 14 18:18:37 myhost /my/home/dir/.local/bin/myqttsense_client.py[38015]: [INFO] subscribing
Mar 14 18:18:58 myhost /my/home/dir/.local/bin/myqttsense_client.py[38015]: [INFO] new topic: sense/sht3x/c4:5b:be:62:87:e3 => `topic_sense_sht3x_c45bbe6287e3_20220314181857`
```

### Other clients

#### Mosquitto 
[Eclipse Mosquitto](https://mosquitto.org) has a command-line client that is quite convenient.

Assuming that a broker has been setup on `localhost`, and its public certificate copied to `ca_certificates/broker.crt`, it is possible to subscribe to all **MyQttSense** messages in verbose mode (useful for debugging) like so:
```sh
mosquitto_sub -h localhost -p 8883 --cafile ca_certificates/broker.crt  -t 'sense/sht3x/#' -F %J  --pretty -v
```

## FAQ
- Why this sensor? What is the accuracy of measurement?
  See [this thread](https://forum.arduino.cc/t/compare-different-i2c-temperature-and-humidity-sensors-sht2x-sht3x-sht85/599609 "i2c sensor Arduino forum thread") for inspiration and/or possible alternatives. Don't expect too much in terms of accuracy (my rule of thumb: ±1°C, ±10% RH).

## Authors
* [Rytis Paškauskas](https://github.com/rytis-paskauskas)

## License
See LICENCE.

## Acknowledgments

[@gschorcht](https://github.com/gschorcht) and  [@UncleRus](https://github.com/UncleRus) are acknowledged for providing the sht3x driver.
