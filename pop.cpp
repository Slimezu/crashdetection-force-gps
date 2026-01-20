#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <MPU6050.h>
#include <time.h>

// ---------- WIFI ----------
const char* ssid = "RacerAbrar";
const char* password = "zafar123";

// ---------- OBJECTS ----------
WebServer server(80);
MPU6050 mpu;

// ---------- CRASH DATA ----------
bool crashDetected = false;
String crashTime = "__";
float crashG = 0.0;

// ---------- SETTINGS ----------
const float CRASH_G_THRESHOLD = 1.5;   // small jerk / ~5cm drop
const unsigned long HOLD_TIME = 20;    // ms

unsigned long impactStart = 0;

// ---------- TIME ----------
String getTimeNow() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "N/A";
  char buf[20];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  return String(buf);
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
body {
  font-family: Arial;
  background: #020617;
  color: white;
  text-align: center;
}
.card {
  background: #020617;
  border: 2px solid #1e293b;
  padding: 20px;
  margin: 40px auto;
  width: 320px;
  border-radius: 12px;
}
.safe { color: #22c55e; }
.crash { color: #ef4444; }
.value { font-size: 20px; margin-top: 10px; }
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
  } else {
    page += "<h2 class='safe'>STATUS: SAFE</h2>";
    page += "<div class='value'>Time: __</div>";
    page += "<div class='value'>Impact: __ g</div>";
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
}

// ---------- LOOP ----------
void loop() {
  server.handleClient();

  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  float axg = ax / 16384.0;
  float ayg = ay / 16384.0;
  float azg = az / 16384.0;

  float g = sqrt(axg * axg + ayg * ayg + azg * azg);

  if (g > CRASH_G_THRESHOLD) {
    if (impactStart == 0) {
      impactStart = millis();
    }

    if (millis() - impactStart > HOLD_TIME && !crashDetected) {
      crashDetected = true;
      crashTime = getTimeNow();
      crashG = g;

      Serial.println("CRASH DETECTED");
      Serial.print("TIME: ");
      Serial.println(crashTime);
      Serial.print("G FORCE: ");
      Serial.println(crashG);
    }
  } else {
    impactStart = 0;
  }

  delay(100);
}
