#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "KaushikPhone";
const char* password = "Wifewifi@1";

// Replace with your PC's local IP address where OM2M is running
const char* om2m_server = "http://172.20.10.3:5089/~/in-cse/in-name/UltrasoundData";

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  
  sendDataToOM2M();
}

// random data for now
String getUltrasoundData(){
   String track_id = "T" + String(random(100, 999));

  char timeBuffer[20];
  sprintf(timeBuffer, "2025-04-07T%02d:%02d:%02d", random(0, 24), random(0, 60), random(0, 60));
  String time = String(timeBuffer);

  float ultrasound = random(100, 500) / 100.0;    // 1.00 to 5.00 cm
  float temperature = random(200, 350) / 10.0;    // 20.0 to 35.0 °C
  float speed = random(20, 100);                 // 20 to 100 km/h

  const char* conditions[] = {"Defective", "Not-Defective"};
  String condition = conditions[random(0, 2)];

  // 👉 Carefully escape inner quotes
  String json = "\\\"track_id\\\":\\\"" + track_id + "\\\",";
  json += "\\\"time\\\":\\\"" + time + "\\\",";
  json += "\\\"ultrasound_reading\\\":" + String(ultrasound, 2) + ",";
  json += "\\\"temperature\\\":" + String(temperature, 1) + ",";
  json += "\\\"speed\\\":" + String(speed, 1) + ",";
  json += "\\\"track_condition\\\":\\\"" + condition + "\\\"";

  json = "{" + json + "}";

  return json;
}

void sendDataToOM2M() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(om2m_server);

    // Set OM2M headers
    http.addHeader("X-M2M-Origin", "admin:admin");
    http.addHeader("X-M2M-RI", "esp32Req001");
    http.addHeader("Content-Type", "application/vnd.onem2m-res+json; ty=4");

    // the data retrieval
    String sensorData = getUltrasoundData();

    // the payload
    String payload = "{\"m2m:cin\": {\"con\": \"" + sensorData + "\"}}";

    int httpResponseCode = http.POST(payload);

    Serial.print("HTTP Response Code: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println("Response:");
    Serial.println(response);

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void loop() {
  // No repeating sends in this example
}
