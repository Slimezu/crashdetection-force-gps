#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <MPU6050.h>
#include <time.h>

MPU6050 mpu;
WebServer server(80);

const char* ssid = "Atal Lab";
const char* password = "Atal@dps";

bool crashDetected = false;
String crashTime = "";

const float CRASH_THRESHOLD = 220.0;
const unsigned long IMPACT_DURATION = 30;

unsigned long impactStartTime = 0;

void handleRoot() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>AeroVasita Ultra</title>
  <link href="icon.png">
  <style>
    body {
      font-family: 'Segoe UI', Arial, sans-serif;
      background: linear-gradient(135deg, #667eea, #764ba2);
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
      margin: 0;
    }

    .card {
      background: rgba(255, 255, 255, 0.95);
      padding: 28px;
      border-radius: 18px;
      width: 340px;
      text-align: center;
      box-shadow: 0 20px 45px rgba(0,0,0,0.25);
      backdrop-filter: blur(10px);
      animation: fadeIn 0.8s ease-in-out;
    }

    h1 {
      margin-bottom: 18px;
      color: #222;
      font-size: 22px;
      letter-spacing: 0.5px;
    }

    .safe {
      color: #1b5e20;
      font-size: 21px;
      font-weight: 700;
      background: #e8f5e9;
      padding: 14px;
      border-radius: 12px;
      box-shadow: inset 0 0 0 1px rgba(46,125,50,0.2);
    }

    .danger {
      color: #b71c1c;
      font-size: 21px;
      font-weight: 700;
      background: #ffebee;
      padding: 14px;
      border-radius: 12px;
      box-shadow: inset 0 0 0 1px rgba(198,40,40,0.25);
      animation: pulse 1.5s infinite;
    }

    .time {
      margin-top: 12px;
      color: #444;
      font-size: 15px;
      background: #f4f6fb;
      padding: 8px;
      border-radius: 8px;
    }

    .footer {
      margin-top: 22px;
      font-size: 12px;
      color: #777;
      letter-spacing: 0.3px;
    }

    @keyframes fadeIn {
      from {
        opacity: 0;
        transform: translateY(15px);
      }
      to {
        opacity: 1;
        transform: translateY(0);
      }
    }

    @keyframes pulse {
      0% {
        box-shadow: 0 0 0 0 rgba(183, 28, 28, 0.4);
      }
      70% {
        box-shadow: 0 0 0 14px rgba(183, 28, 28, 0);
      }
      100% {
        box-shadow: 0 0 0 0 rgba(183, 28, 28, 0);
      }
    }
  </style>
</head>

<body>
  <div class="card">
    <h1>AeroVasita Ultra Helmet</h1>
)rawliteral";

  if (crashDetected) {
    page += "<div class='danger'>🚨 CRASH DETECTED<br>SEND HELP</div>";
    page += "<div class='time'>⏱ Time: " + crashTime + "</div>";
  } else {
    page += "<div class='safe'>✅ Helmet Currently Safe</div>";
  }

  page += R"rawliteral(
    <div class="footer">Smart Helmet Monitoring System (Crash Detection)</div>
  </div>
</body>
</html>

)rawliteral";

  server.send(200, "text/html", page);
}

String getTimeNow() {
  struct tm t;
  if (!getLocalTime(&t)) return "N/A";
  char buf[20];
  strftime(buf, sizeof(buf), "%H:%M:%S", &t);
  return String(buf);
}

void setup() {
  Serial.begin(115200);

  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected YIPEEE!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.begin();
  Serial.println("Web server started LES GOOOO :) ");

  configTime(19800, 0, "pool.ntp.org");

  Wire.begin(21, 22);
  mpu.initialize();
}

void loop() {
  server.handleClient();

  int16_t x, y, z;
  mpu.getRotation(&x, &y, &z);

  float a = x / 131.0;
  float b = y / 131.0;
  float c = z / 131.0;

  float impact = sqrt(a * a + b * b + c * c);

  if (impact > CRASH_THRESHOLD) {
    if (impactStartTime == 0) {
      impactStartTime = millis();
    }

    if ((millis() - impactStartTime) >= IMPACT_DURATION && !crashDetected) {
      crashDetected = true;
      crashTime = getTimeNow();
    }
  } else {
    impactStartTime = 0;
  }

  delay(120);
}


	
