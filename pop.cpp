#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <MPU6050.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ---------- GOOGLE GEOLOCATION ----------
const char* GOOGLE_API_KEY = "AIzaSyCPlNhJxaSMXAEpiVjDYo2LE1Tb9mzRb3o";  // ← Paste your key here

// ---------- WIFI ----------
const char* ssid = "Atal Labs";
const char* password = "Atal@dps";

// ---------- OBJECTS ----------
WebServer server(80);
MPU6050 mpu;

// ---------- CRASH DATA ----------
bool crashDetected = false;
String crashTime = "__";
float crashG = 0.0;

// ---------- LIVE G-FORCE (GLOBAL) ----------
float g = 0.0;  // Current real-time g-force (global for web display)

// ---------- LOCATION DATA ----------
double currentLat = 0.0;
double currentLon = 0.0;
float currentAcc = 0.0;
bool locationValid = false;
unsigned long lastLocationUpdate = 0;
const unsigned long LOCATION_INTERVAL = 300000;  // 5 minutes

// ---------- SETTINGS ----------
const float CRASH_G_THRESHOLD = 1.5; // Adjust after testing
const unsigned long HOLD_TIME = 20; // ms
unsigned long impactStart = 0;

// ---------- TIME ----------
String getTimeNow() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "N/A";
  char buf[20];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  return String(buf);
}

// ---------- GEOLOCATION FUNCTION ----------
void getWifiLocation() {
  Serial.println("Scanning WiFi for geolocation...");
  int n = WiFi.scanNetworks();
  
  if (n == 0) {
    Serial.println("No WiFi networks found");
    locationValid = false;
    return;
  }

  DynamicJsonDocument doc(1024);
  JsonArray wifiArray = doc.createNestedArray("wifiAccessPoints");
  
  for (int i = 0; i < n && i < 10; i++) {  // Up to 10 strongest
    JsonObject ap = wifiArray.createNestedObject();
    ap["macAddress"] = WiFi.BSSIDstr(i);
    ap["signalStrength"] = WiFi.RSSI(i);
  }
  doc["considerIp"] = false;

  String payload;
  serializeJson(doc, payload);

  HTTPClient http;
  String url = "https://www.googleapis.com/geolocation/v1/geolocate?key=" + String(GOOGLE_API_KEY);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(payload);
  if (httpCode == 200) {
    String response = http.getString();
    DynamicJsonDocument resp(512);
    deserializeJson(resp, response);
    
    currentLat = resp["location"]["lat"];
    currentLon = resp["location"]["lng"];
    currentAcc = resp["accuracy"];
    locationValid = true;
    
    Serial.printf("Location updated: %.6f, %.6f (±%.0fm)\n", currentLat, currentLon, currentAcc);
  } else {
    Serial.println("Geolocation failed (code: " + String(httpCode) + ")");
    locationValid = false;
  }
  
  http.end();
  WiFi.scanDelete();
}

// ---------- WEBSITE ----------
void handleRoot() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>AeroVasita Helmet</title>
<meta http-equiv="refresh" content="3">
<style>
body { font-family: Arial; background: #020617; color: white; text-align: center; }
.card { background: #020617; border: 2px solid #1e293b; padding: 20px; margin: 40px auto; width: 320px; border-radius: 12px; }
.safe { color: #22c55e; font-size: 28px; }
.crash { color: #ef4444; font-size: 28px; }
.value { font-size: 18px; margin-top: 12px; }
.mapslink { font-size: 20px; margin-top: 20px; }
.mapslink a { color: #ef4444; text-decoration: underline; }
</style>
</head>
<body>
<div class="card">
<h1>AeroVasita Helmet</h1>
)rawliteral";

  if (crashDetected) {
    page += "<h2 class='crash'>CRASH DETECTED</h2>";
    page += "<div class='value'>Time: " + crashTime + "</div>";
    page += "<div class='value'>Impact: " + String(crashG, 2) + " g</div>";
    
    if (locationValid) {
      String mapsUrl = "https://maps.google.com/?q=" + String(currentLat,6) + "," + String(currentLon,6);
      page += "<div class='mapslink'><a href='" + mapsUrl + "'>🚨 OPEN MAPS FOR HELP</a></div>";
    } else {
      page += "<div class='value'>Location: Updating...</div>";
    }
  } else {
    page += "<h2 class='safe'>STATUS: SAFE</h2>";
    page += "<div class='value'>Time: " + getTimeNow() + "</div>";
    page += "<div class='value'>Impact: " + String(g, 2) + " g</div>";  // Now uses global g
  }

  // Always show current location
  if (locationValid) {
    page += "<div class='value'>Lat: " + String(currentLat, 6) + "°</div>";
    page += "<div class='value'>Lon: " + String(currentLon, 6) + "°</div>";
    page += "<div class='value'>Accuracy: ~" + String(currentAcc, 0) + " m</div>";
  } else {
    page += "<div class='value'>Location: Getting...</div>";
  }

  page += R"rawliteral(
</div>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", page);
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("BOOTING ESP32");

  WiFi.begin(ssid, password);
  Serial.print("CONNECTING WIFI");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWIFI CONNECTED");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  configTime(19800, 0, "pool.ntp.org");
  Serial.println("TIME SYNCED");

  Wire.begin(21, 22);
  Serial.println("I2C STARTED");
  mpu.initialize();
  if (mpu.testConnection()) {
    Serial.println("MPU6050 CONNECTED");
  } else {
    Serial.println("MPU6050 NOT FOUND");
  }

  server.on("/", handleRoot);
  server.begin();
  Serial.println("WEB SERVER STARTED");

  getWifiLocation();  // Initial location
  lastLocationUpdate = millis();
}

// ---------- LOOP ----------
void loop() {
  server.handleClient();

  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  float axg = ax / 16384.0;
  float ayg = ay / 16384.0;
  float azg = az / 16384.0;
  g = sqrt(axg * axg + ayg * ayg + azg * azg);  // Update global g

  if (g > CRASH_G_THRESHOLD) {
    if (impactStart == 0) {
      impactStart = millis();
    }
    if (millis() - impactStart > HOLD_TIME && !crashDetected) {
      crashDetected = true;
      crashTime = getTimeNow();
      crashG = g;
      Serial.println("CRASH DETECTED");
      Serial.print("TIME: "); Serial.println(crashTime);
      Serial.print("G FORCE: "); Serial.println(crashG);
      getWifiLocation();  // Fresh location on crash
    }
  } else {
    impactStart = 0;
  }

  // Periodic location update
  if (millis() - lastLocationUpdate >= LOCATION_INTERVAL) {
    getWifiLocation();
    lastLocationUpdate = millis();
  }

  delay(100);
}
