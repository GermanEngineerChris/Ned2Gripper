#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ====== WLAN eintragen ======
const char* WIFI_SSID = "yourwifi";
const char* WIFI_PASS = "yourpw";
// ============================

// ====== Pins anpassen ======
static const int SERVO_PIN = 8;   // Servo Signal
static const int BTN_PIN   = 2;   // Optional: Taster gegen GND
// ===========================

Servo servo;
WebServer server(80);

// Winkel-Liste (Buttons)
const int positions[] = {10, 60, 90, 125};
const int numPositions = sizeof(positions) / sizeof(positions[0]);

int currentIndex = 0;
int currentAngle = positions[0];

// Entprellung (optional Taster)
bool lastReadingPressed = false;
bool lastStablePressed  = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 40;

void printAngle(const char* source, int angle) {
  Serial.print("[");
  Serial.print(source);
  Serial.print("] Winkel: ");
  Serial.print(angle);
  Serial.println("°");
}

void moveToAngle(int angle, const char* source) {
  angle = constrain(angle, 0, 180);
  currentAngle = angle;
  servo.write(angle);
  printAngle(source, angle);
}

bool pressedEdge() {
  // INPUT_PULLUP: gedrückt = LOW
  bool pressedNow = (digitalRead(BTN_PIN) == LOW);

  if (pressedNow != lastReadingPressed) {
    lastDebounceTime = millis();
    lastReadingPressed = pressedNow;
  }

  if (millis() - lastDebounceTime > debounceDelay) {
    if (pressedNow != lastStablePressed) {
      lastStablePressed = pressedNow;
      if (lastStablePressed) return true;
    }
  }
  return false;
}

String pageHtml() {
  String html = R"HTML(
<!doctype html><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Servo Control</title>
  <style>
    body{font-family:system-ui,Arial;margin:24px}
    button{padding:14px 18px;margin:8px 8px 0 0;font-size:16px}
    .box{padding:14px;border:1px solid #ddd;border-radius:12px;max-width:520px}
    input{padding:10px;font-size:16px;width:90px}
    .small{color:#666;margin-top:10px}
  </style>
</head>
<body>
  <div class="box">
    <h2>Servo Winkel anfahren</h2>
)HTML";

  html += "<p>Aktueller Winkel: <b>" + String(currentAngle) + "°</b></p>";

  for (int i = 0; i < numPositions; i++) {
    html += "<a href=\"/set?angle=" + String(positions[i]) + "\"><button>";
    html += String(positions[i]) + "°</button></a>";
  }

  html += R"HTML(
    <hr style="margin:18px 0">
    <form action="/set" method="get">
      <label>Freier Winkel (0..180): </label>
      <input type="number" name="angle" min="0" max="180" value="90">
      <button type="submit">Fahren</button>
    </form>
    <p class="small">Tipp: Seite neu laden, um den aktuellen Winkel zu sehen.</p>
  </div>
</body></html>
)HTML";

  return html;
}

void handleRoot() {
  server.send(200, "text/html; charset=utf-8", pageHtml());
}

void handleSet() {
  if (!server.hasArg("angle")) {
    server.send(400, "text/plain; charset=utf-8", "Fehler: angle fehlt. Beispiel: /set?angle=90");
    return;
  }
  int angle = server.arg("angle").toInt();
  moveToAngle(angle, "WEB");

  server.sendHeader("Location", "/");
  server.send(303);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Verbinde mit WLAN: ");
  Serial.println(WIFI_SSID);

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - t0 > 20000) { // 20s Timeout
      Serial.println("\nWLAN Verbindung fehlgeschlagen. Prüfe SSID/PASS.");
      return;
    }
  }
  Serial.println("\nWLAN verbunden!");
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(BTN_PIN, INPUT_PULLUP); // optional

  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  moveToAngle(currentAngle, "BOOT");

  connectWiFi();

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();
  Serial.println("Webserver gestartet (Port 80).");
}

void loop() {
  server.handleClient();

  // Optional: Taster schaltet die festen Positionen durch
  if (pressedEdge()) {
    currentIndex++;
    if (currentIndex >= numPositions) currentIndex = 0;
    moveToAngle(positions[currentIndex], "BTN");
  }
}
