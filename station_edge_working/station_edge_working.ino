#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

// WiFi credentials
const char* ssid = "KaushikPhone";
const char* password = "Wifewifi@1";

// OM2M container URL
const char* om2m_server = "http://172.20.10.3:5089/~/in-cse/in-name/condition";

// Buzzer pin
const int buzzerPin = 17;

// Web server
WebServer server(80);

// Dynamic notify URL (will be filled after WiFi connects)
String esp32_notify_url = "";

// Function to subscribe to OM2M
void subscribeToOM2M() {
  HTTPClient http;
  http.begin(om2m_server);
  http.addHeader("X-M2M-Origin", "admin:admin");
  http.addHeader("Content-Type", "application/vnd.onem2m-res+json; ty=23");

  String payload = 
    "{"
    "\"m2m:sub\": {"
    "\"rn\": \"subESP32\","
    "\"nu\": [\"" + esp32_notify_url + "\"],"
    "\"nct\": 2"
    "}"
    "}";

  int httpResponseCode = http.POST(payload);
  Serial.print("OM2M Subscription Response: ");
  Serial.println(httpResponseCode);
  Serial.println(http.getString());

  http.end();
}

// Function to ring buzzer
void ringBuzzer() {
  Serial.println("🚨 Defect detected! Buzzing...");
  digitalWrite(buzzerPin, HIGH);
  digitalWrite(2, HIGH);
  delay(5000);
  digitalWrite(buzzerPin, LOW);
  digitalWrite(2, LOW);
}

// HTTP handler
void handleDefect() {
  if (server.method() == HTTP_POST) {
    Serial.println("📩 POST /defect received");
    ringBuzzer();
    server.send(200, "text/plain", "Buzzer activated");
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(buzzerPin, OUTPUT);
  pinMode(2,OUTPUT);
  digitalWrite(2, LOW);
  digitalWrite(buzzerPin, LOW);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected!");
  IPAddress ip = WiFi.localIP();
  Serial.print("ESP32 IP: ");
  Serial.println(ip);

  // Build dynamic notification URL
  esp32_notify_url = "http://" + ip.toString() + "/defect";

  
  // Set up HTTP server
  server.on("/defect", HTTP_POST, handleDefect);
  server.begin();
  Serial.println("🚀 HTTP Server started!");
  subscribeToOM2M();

}

void loop() {
  server.handleClient();
}
