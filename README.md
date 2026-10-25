ESP8266 Sensor Web Server

A web-based sensor monitoring system built using the ESP8266 NodeMCU development board. The firmware generates simulated sensor readings, displays live data on a web dashboard, plots the last five minutes of readings, and provides a configuration page for Wi-Fi credentials and sampling interval.

Settings are stored in LittleFS, allowing configuration to persist across device reboots.

Features

Simulated sensor: Generates a simulated sensor value at a configurable interval.
Live dashboard: Displays the current sensor value in a web browser.
Live chart: Plots sensor readings from the last five minutes.
Automatic updates: Refreshes sensor data using periodic HTTP requests without reloading the webpage.
Configuration page: Allows users to change Wi-Fi credentials and sampling interval.
Wi-Fi setup access point: Provides a dedicated network for initial configuration.
Persistent settings: Stores configuration in LittleFS flash storage.
Automatic reconnection: Periodically retries the saved Wi-Fi connection if disconnected.

Hardware Requirements

ESP8266 NodeMCU development board
USB data cable
Computer running Windows, Linux, or macOS
Arduino IDE
Wi-Fi network operating at 2.4 GHz (optional for initial dashboard testing)

No physical sensor is required because this implementation uses simulated sensor readings.

Software Requirements

1. Install Arduino IDE

Download and install Arduino IDE from:

https://www.arduino.cc/en/software

2. Install the ESP8266 board package

1. Open Arduino IDE.
2. Navigate to File → Preferences.
3. Find Additional Boards Manager URLs.
4. Add the following URL:

   `https://arduino.esp8266.com/stable/package_esp8266com_index.json`

5. Click OK.
6. Open Tools → Board → Boards Manager.
7. Search for `esp8266`.
8. Install the ESP8266 board package published by the ESP8266 Community.

3. Required libraries

The firmware uses libraries supplied with the ESP8266 Arduino core:

ESP8266WiFi.h - Wi-Fi access point and router connection 
ESP8266WebServer.h - HTTP web server 
LittleFS.h - Persistent configuration storage 
math.h - Simulated sensor calculations 

No additional third-party libraries are required for the supplied firmware.


The HTML dashboard and settings page are embedded in the Arduino sketch, so separate HTML, CSS, and JavaScript files are not required.

Build and Flash the Firmware

Step 1: Download the repository as a ZIP file from GitHub and extract it.

Step 2: Open the sketch

Open ESP8266_WebServer.ino in Arduino IDE.

Step 3: Select the board

Connect the ESP8266 to the computer using a USB data cable.

In Arduino IDE, select:

Board: Generic ESP8266 board.
Port: The COM port assigned to your device.
Upload Speed:115200 baud.

Step 4: Compile the firmware

Click Verify in Arduino IDE.

The firmware should compile if the ESP8266 board package is installed and the sketch is copied correctly.

Step 5: Upload the firmware

Click Upload and wait for the compilation and flashing process to complete.

If the upload fails, verify the USB cable, selected COM port, USB-to-serial driver, and board selection.

Step 6: Open Serial Monitor

Open Tools → Serial Monitor and set the baud rate to:

115200

Press the RESET button on the ESP8266.

The serial output should indicate that the device has started, the setup access point has been created, and the HTTP server is running.

Example output:


ESP8266 Sensor Web Server

=========================
LittleFS mounted.

Setup Access Point:
SSID: ESP8266-Sensor-Setup
Password: sensor123
AP IP: 192.168.4.1

HTTP server started.
Dashboard: http://192.168.4.1/
Settings:  http://192.168.4.1/settings
Sensor value: 25.123

The displayed sensor values are simulated and will vary.

Access the Web Dashboard

Step 1: Connect to the setup network

From a computer or mobile device, open the Wi-Fi settings and connect to:

SSID - ESP8266-Sensor-Setup
Password - sensor123
ESP8266 access-point IP - 192.168.4.1

The setup network does not provide internet access. Your computer may display a "No Internet" warning; this is expected.

Step 2: Open the dashboard

Open a web browser and navigate to:

http://192.168.4.1/

The dashboard displays the current sensor reading and a chart of the available readings from the last five minutes.

The browser requests updated data approximately every second without reloading the page.

Step 3: Access the settings page

Navigate to:
http://192.168.4.1/settings

The settings page provides the following configuration options:
Wi-Fi network name (SSID)
Wi-Fi password
Sampling interval in milliseconds

To Configure Wi-Fi and Sampling Interval

1. Open the settings page.
2. Enter the SSID of your 2.4 GHz Wi-Fi network.
3. Enter the Wi-Fi password.
4. Enter the desired sampling interval.
5. Click Save Settings.

The allowed sampling interval is between 250 ms and 60,000 ms.

After saving, the firmware writes the configuration to LittleFS and restarts to apply the settings.

When the ESP8266 connects to the router, its assigned IP address is printed in Serial Monitor. Connect your computer to the same router network and open the dashboard using that IP address.

The setup access point remains enabled in this implementation.





Troubleshooting

Setup Wi-Fi network is not visible

-Check that the ESP8266 is powered.

-Open Serial Monitor at 115200 baud.

-Look for the Setup AP startup messages.

-Reset the board and scan for the network again.

Dashboard does not open

-Confirm that the computer is connected to the ESP8266 setup network.

-Enter http://192.168.4.1/ directly in the address bar.

-Check whether the HTTP server started successfully.

-Wi-Fi connection keeps retrying

-The message Retrying saved Wi-Fi connection... indicates that the ESP8266 is not connected to the saved router network.

-Check the SSID and password.

-Ensure the router supports 2.4 GHz Wi-Fi.

-Check signal strength and router access restrictions.

-Review the connection messages in Serial Monitor.

LittleFS mount failed

-Persistent settings depend on LittleFS mounting successfully.

-Check the ESP8266 board configuration and flash settings.

-Review the Serial Monitor output after restarting.

-If necessary, investigate filesystem formatting or initialization.

-Formatting the filesystem erases stored settings and should only be performed when necessary.

