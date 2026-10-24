#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <math.h>

ESP8266WebServer server(80);

// ---------------- CONFIGURATION ----------------

const char* AP_SSID = "ESP8266-Sensor-Setup";
const char* AP_PASSWORD = "sensor123";

const char* SETTINGS_FILE = "/settings.txt";

const unsigned long HISTORY_DURATION = 300000UL; // 5 minutes
const unsigned long MIN_INTERVAL = 250UL;
const unsigned long MAX_INTERVAL = 60000UL;

const int MAX_READINGS = 650;

// ---------------- SETTINGS ----------------

String wifiSSID = "";
String wifiPassword = "";

unsigned long sampleInterval = 1000;
unsigned long lastSampleTime = 0;
unsigned long lastWiFiAttempt = 0;

bool filesystemReady = false;

// ---------------- SENSOR HISTORY ----------------

struct Reading {
  unsigned long timestamp;
  float value;
};

Reading readings[MAX_READINGS];

int readingCount = 0;
int nextReadingIndex = 0;

float currentValue = 0.0;

// ---------------- HTML DASHBOARD ----------------

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP8266 Sensor Monitor</title>
<style>
body {
  margin: 0;
  padding: 20px;
  font-family: Arial, sans-serif;
  background: #f2f5f9;
  color: #18212f;
}
.container {
  max-width: 900px;
  margin: auto;
}
.card {
  background: white;
  border-radius: 12px;
  padding: 20px;
  margin-bottom: 18px;
  box-shadow: 0 2px 12px #00000010;
}
h1 { font-size: 25px; }
.value {
  font-size: 48px;
  font-weight: bold;
  color: #087f8c;
}
.small { color: #667085; }
canvas {
  width: 100%;
  height: 300px;
  display: block;
}
a, button {
  color: white;
  background: #087f8c;
  padding: 10px 15px;
  border-radius: 6px;
  text-decoration: none;
  border: 0;
  cursor: pointer;
}
.status { font-weight: bold; }
</style>
</head>

<body>
<div class="container">

  <h1>ESP8266 Sensor Monitor</h1>
  <p class="small">Live sensor dashboard</p>

  <div class="card">
    <h3>Current Sensor Value</h3>
    <div class="value" id="value">--</div>
    <p class="small" id="updated">Waiting for first reading...</p>
    <p>Sampling interval: <strong id="interval">--</strong></p>
  </div>

  <div class="card">
    <h3>Sensor History — Last 5 Minutes</h3>
    <canvas id="chart"></canvas>
    <p class="small">
      The chart updates automatically without reloading this page.
    </p>
    <p class="status" id="connection">Connecting...</p>
  </div>

  <div class="card">
    <h3>Device Settings</h3>
    <p>Change the sampling interval or Wi-Fi configuration.</p>
    <a href="/settings">Open Settings</a>
  </div>

</div>

<script>
const canvas = document.getElementById("chart");
const ctx = canvas.getContext("2d");

let lastPoints = [];

function drawChart(points) {
  lastPoints = points;

  const rect = canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;

  canvas.width = Math.max(1, rect.width * dpr);
  canvas.height = Math.max(1, rect.height * dpr);

  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

  const w = rect.width;
  const h = rect.height;

  ctx.clearRect(0, 0, w, h);

  const left = 48;
  const right = 12;
  const top = 18;
  const bottom = 32;

  const cw = w - left - right;
  const ch = h - top - bottom;

  if (!points || points.length === 0) {
    ctx.fillStyle = "#667085";
    ctx.fillText("Waiting for sensor readings...", 20, 35);
    return;
  }

  let minV = Math.min(...points.map(p => p.value));
  let maxV = Math.max(...points.map(p => p.value));

  if (maxV - minV < 0.01) {
    minV -= 0.5;
    maxV += 0.5;
  }

  const padding = (maxV - minV) * 0.12;
  minV -= padding;
  maxV += padding;

  ctx.font = "11px Arial";
  ctx.lineWidth = 1;
  ctx.strokeStyle = "#e4e7ec";
  ctx.fillStyle = "#667085";

  for (let i = 0; i <= 4; i++) {
    const y = top + ch * i / 4;
    const v = maxV - (maxV - minV) * i / 4;

    ctx.beginPath();
    ctx.moveTo(left, y);
    ctx.lineTo(w - right, y);
    ctx.stroke();

    ctx.fillText(v.toFixed(2), 2, y + 4);
  }

  for (let i = 0; i <= 5; i++) {
    const x = left + cw * i / 5;

    ctx.beginPath();
    ctx.moveTo(x, top);
    ctx.lineTo(x, top + ch);
    ctx.stroke();

    ctx.fillText((-5 + i) + "m", x - 10, h - 8);
  }

  ctx.strokeStyle = "#087f8c";
  ctx.lineWidth = 2;
  ctx.beginPath();

  points.forEach((p, i) => {
    const x = left + cw * (300000 - p.age) / 300000;
    const y = top + ch * (maxV - p.value) / (maxV - minV);

    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });

  ctx.stroke();
}

async function updateDashboard() {
  try {
    const response = await fetch("/api/data", {cache: "no-store"});
    if (!response.ok) throw new Error("HTTP " + response.status);

    const data = await response.json();

    document.getElementById("value").textContent =
      Number(data.current).toFixed(3);

    document.getElementById("interval").textContent =
      data.interval + " ms";

    document.getElementById("updated").textContent =
      "Last updated: " + new Date().toLocaleTimeString();

    document.getElementById("connection").textContent =
      "Device connected";

    drawChart(data.points);
  } catch (e) {
    document.getElementById("connection").textContent =
      "Connection lost — retrying...";
  }
}

window.addEventListener("resize", () => drawChart(lastPoints));

updateDashboard();
setInterval(updateDashboard, 1000);
</script>
</body>
</html>
)rawliteral";

// ---------------- SETTINGS PAGE ----------------

const char SETTINGS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP8266 Settings</title>
<style>
body {
  margin: 0;
  padding: 20px;
  font-family: Arial, sans-serif;
  background: #f2f5f9;
  color: #18212f;
}
.container { max-width: 550px; margin: auto; }
.card {
  background: white;
  padding: 22px;
  border-radius: 12px;
  box-shadow: 0 2px 12px #00000010;
}
label { display: block; margin-top: 17px; font-weight: bold; }
input {
  width: 100%;
  box-sizing: border-box;
  padding: 12px;
  margin-top: 7px;
  border: 1px solid #cbd5e1;
  border-radius: 6px;
}
button, a {
  display: inline-block;
  margin-top: 20px;
  padding: 12px 16px;
  background: #087f8c;
  color: white;
  border: 0;
  border-radius: 6px;
  text-decoration: none;
  cursor: pointer;
}
.note { color: #667085; font-size: 13px; }
</style>
</head>
<body>
<div class="container">
<div class="card">
<h2>Device Settings</h2>

<form method="POST" action="/save">

<label for="ssid">Wi-Fi Network Name (SSID)</label>
<input id="ssid" name="ssid" type="text"
       maxlength="32" required autocomplete="off">

<label for="password">Wi-Fi Password</label>
<input id="password" name="password" type="password"
       maxlength="63" autocomplete="new-password"
       placeholder="Enter new password, or leave blank to keep current">

<p class="note">
Leave the password blank to keep the saved password.
</p>

<label for="interval">Sampling Interval (milliseconds)</label>
<input id="interval" name="interval" type="number"
       min="250" max="60000" required>

<p class="note">
Minimum 250 ms; maximum 60000 ms (60 seconds).
</p>

<button type="submit">Save Settings</button>
</form>

<a href="/">Back to Dashboard</a>
</div>
</div>

<script>
fetch("/api/settings", {cache: "no-store"})
.then(r => r.json())
.then(s => {
  document.getElementById("ssid").value = s.ssid;
  document.getElementById("interval").value = s.interval;
})
.catch(() => {});
</script>
</body>
</html>
)rawliteral";

// ---------------- LOAD SETTINGS ----------------

void loadSettings() {
  if (!filesystemReady) return;

  if (!LittleFS.exists(SETTINGS_FILE)) {
    Serial.println("No saved settings; using defaults.");
    return;
  }

  File file = LittleFS.open(SETTINGS_FILE, "r");

  if (!file) {
    Serial.println("Could not open settings file.");
    return;
  }

  String savedSSID = file.readStringUntil('\n');
  String savedPassword = file.readStringUntil('\n');
  String savedInterval = file.readStringUntil('\n');

  file.close();

  savedSSID.trim();
  savedPassword.trim();
  savedInterval.trim();

  if (savedSSID.length() <= 32) {
    wifiSSID = savedSSID;
  }

  if (savedPassword.length() <= 63) {
    wifiPassword = savedPassword;
  }

  if (savedInterval.length() > 0) {
    unsigned long n = strtoul(savedInterval.c_str(), nullptr, 10);

    if (n >= MIN_INTERVAL && n <= MAX_INTERVAL) {
      sampleInterval = n;
    }
  }

  Serial.println("Settings loaded from LittleFS.");
}

// ---------------- SAVE SETTINGS ----------------

bool saveSettings() {
  if (!filesystemReady) return false;

  File file = LittleFS.open(SETTINGS_FILE, "w");

  if (!file) {
    Serial.println("Could not write settings file.");
    return false;
  }

  file.println(wifiSSID);
  file.println(wifiPassword);
  file.println(sampleInterval);

  file.close();

  Serial.println("Settings saved to LittleFS.");
  return true;
}

// ---------------- SENSOR SIMULATION ----------------

float readSimulatedSensor() {
  // Simulates a slowly varying sensor with a small random component.
  static float phase = 0.0;

  phase += 0.12;

  float value = 25.0
              + 3.0 * sin(phase)
              + random(-100, 101) / 100.0;

  return value;
}

// ---------------- HISTORY MANAGEMENT ----------------

void addReading(float value) {
  unsigned long now = millis();

  readings[nextReadingIndex].timestamp = now;
  readings[nextReadingIndex].value = value;

  nextReadingIndex = (nextReadingIndex + 1) % MAX_READINGS;

  if (readingCount < MAX_READINGS) {
    readingCount++;
  }
}

void removeOldReadings() {
  unsigned long now = millis();

  while (readingCount > 0) {
    int oldestIndex =
      (nextReadingIndex - readingCount + MAX_READINGS) % MAX_READINGS;

    unsigned long age = now - readings[oldestIndex].timestamp;

    if (age <= HISTORY_DURATION) break;

    readingCount--;
  }
}

// ---------------- JSON API ----------------

void handleData() {
  removeOldReadings();

  unsigned long now = millis();

  String json;
  json.reserve(22000);

  json = "{\"current\":";
  json += String(currentValue, 3);
  json += ",\"interval\":";
  json += String(sampleInterval);
  json += ",\"points\":[";

  bool first = true;

  for (int i = 0; i < readingCount; i++) {
    int index =
      (nextReadingIndex - readingCount + i + MAX_READINGS)
      % MAX_READINGS;

    unsigned long age = now - readings[index].timestamp;

    if (age > HISTORY_DURATION) continue;

    if (!first) json += ",";

    json += "{\"age\":";
    json += String(age);
    json += ",\"value\":";
    json += String(readings[index].value, 3);
    json += "}";

    first = false;
  }

  json += "]}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void handleSettingsAPI() {
  // Never send the saved Wi-Fi password to the browser.
  String json = "{\"ssid\":\"";

  String safeSSID = wifiSSID;
  safeSSID.replace("\\", "\\\\");
  safeSSID.replace("\"", "\\\"");
  safeSSID.replace("\n", "");
  safeSSID.replace("\r", "");

  json += safeSSID;
  json += "\",\"interval\":";
  json += String(sampleInterval);
  json += "}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

// ---------------- WEB PAGE HANDLERS ----------------

void handleRoot() {
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

void handleSettingsPage() {
  server.send_P(200, "text/html", SETTINGS_HTML);
}

void handleSaveSettings() {
  if (!server.hasArg("ssid") || !server.hasArg("interval")) {
    server.send(400, "text/plain",
                "Missing SSID or sampling interval.");
    return;
  }

  String newSSID = server.arg("ssid");
  String newPassword = server.arg("password");
  String intervalText = server.arg("interval");

  newSSID.trim();
  intervalText.trim();

  if (newSSID.length() < 1 || newSSID.length() > 32 ||
      newPassword.length() > 63) {
    server.send(400, "text/plain", "Invalid Wi-Fi credentials.");
    return;
  }

  if (intervalText.length() == 0) {
    server.send(400, "text/plain", "Sampling interval is required.");
    return;
  }

  for (unsigned int i = 0; i < intervalText.length(); i++) {
    if (!isDigit(intervalText[i])) {
      server.send(400, "text/plain",
                  "Sampling interval must be a number.");
      return;
    }
  }

  unsigned long newInterval =
    strtoul(intervalText.c_str(), nullptr, 10);

  if (newInterval < MIN_INTERVAL ||
      newInterval > MAX_INTERVAL) {
    server.send(400, "text/plain",
                "Interval must be between 250 and 60000 ms.");
    return;
  }

  String oldSSID = wifiSSID;
  String oldPassword = wifiPassword;
  unsigned long oldInterval = sampleInterval;

  wifiSSID = newSSID;
  sampleInterval = newInterval;

  // Blank password means keep the current password when
  // updating the same network.
  if (newPassword.length() > 0) {
    wifiPassword = newPassword;
  } else if (newSSID != oldSSID) {
    wifiPassword = "";
  }

  if (!saveSettings()) {
    wifiSSID = oldSSID;
    wifiPassword = oldPassword;
    sampleInterval = oldInterval;

    server.send(500, "text/plain",
                "Could not save settings. Check LittleFS.");
    return;
  }

  lastSampleTime = millis();

  server.send(200, "text/html",
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Settings Saved</title></head><body>"
    "<h2>Settings saved successfully.</h2>"
    "<p>The sampling interval has been updated.</p>"
    "<p>New Wi-Fi credentials are saved. The device will now restart "
    "to apply the Wi-Fi configuration.</p>"
    "<p>Reconnect to ESP8266-Sensor-Setup if necessary.</p>"
    "</body></html>");

  // Give the browser time to receive the response.
  delay(800);
  ESP.restart();
}

void handleNotFound() {
  server.send(404, "text/plain",
              "Page not found. Open / or /settings");
}

// ---------------- WIFI ----------------

void startAccessPoint() {
  WiFi.mode(WIFI_AP_STA);

  bool ok = WiFi.softAP(AP_SSID, AP_PASSWORD);

  Serial.println();
  Serial.println("Setup Access Point:");

  if (ok) {
    Serial.print("SSID: ");
    Serial.println(AP_SSID);

    Serial.print("Password: ");
    Serial.println(AP_PASSWORD);

    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("WARNING: Access Point startup failed.");
  }
}

void connectToSavedWiFi() {
  if (wifiSSID.length() == 0) {
    Serial.println("No saved Wi-Fi network.");
    Serial.println("Use the setup AP to configure Wi-Fi.");
    return;
  }

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(wifiSSID);

  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 12000UL) {
    delay(250);
    yield();
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected.");
    Serial.print("Station IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi connection unsuccessful.");
    Serial.println("Setup AP remains available.");
  }

  lastWiFiAttempt = millis();
}

// ---------------- SETUP ----------------

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("ESP8266 Sensor Web Server");
  Serial.println("=========================");

  randomSeed(ESP.getChipId() ^ micros());

  filesystemReady = LittleFS.begin();

  if (!filesystemReady) {
    Serial.println("LittleFS mount failed.");
    Serial.println("Persistent settings unavailable.");
  } else {
    Serial.println("LittleFS mounted.");
    loadSettings();
  }

  startAccessPoint();
  connectToSavedWiFi();

  // Register web routes.
  server.on("/", HTTP_GET, handleRoot);
  server.on("/settings", HTTP_GET, handleSettingsPage);
  server.on("/api/data", HTTP_GET, handleData);
  server.on("/api/settings", HTTP_GET, handleSettingsAPI);
  server.on("/save", HTTP_POST, handleSaveSettings);
  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println("HTTP server started.");
  Serial.println("Dashboard: http://192.168.4.1/");
  Serial.println("Settings:  http://192.168.4.1/settings");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Dashboard on your Wi-Fi: http://");
    Serial.println(WiFi.localIP());
  }

  // Start with a reading immediately.
  currentValue = readSimulatedSensor();
  addReading(currentValue);
  lastSampleTime = millis();
}

// ---------------- MAIN LOOP ----------------

void loop() {
  server.handleClient();

  unsigned long now = millis();

  // Sample sensor at the configured interval.
  if (now - lastSampleTime >= sampleInterval) {
    lastSampleTime = now;

    currentValue = readSimulatedSensor();
    addReading(currentValue);

    Serial.print("Sensor value: ");
    Serial.println(currentValue, 3);
  }

  removeOldReadings();

  // Reconnect to the configured Wi-Fi network if disconnected.
  // The setup AP remains available while this is attempted.
  if (wifiSSID.length() > 0 &&
      WiFi.status() != WL_CONNECTED &&
      now - lastWiFiAttempt >= 15000UL) {

    lastWiFiAttempt = now;

    Serial.println("Retrying saved Wi-Fi connection...");
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  }

  yield();
}
