#include <WiFi.h>
#include <HTTPClient.h>
#include "base64.h" // Install "Base64" by Adam Rudd from Library Manager

const char* ssid = "KaushikPhone";
const char* password = "Wifewifi@1";

int trigger = 14;
int echo = 27;

// Replace with your PC's local IP address where OM2M is running
const char* om2m_server = "http://172.20.10.3:5089/~/in-cse/in-name/UltrasoundData";

void setup() {
  Serial.begin(115200);
  delay(1000);

  // setting pins
  pinMode(trigger, OUTPUT);
  pinMode(echo, INPUT);

  // WiFi.begin(ssid, password);
  // Serial.print("Connecting to WiFi...");
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print(".");
  // }
  // Serial.println("\nConnected to WiFi!");
  
  // sendDataToOM2M();

  String json = buildJson();
  print(json);
}

float getUltrasoundReading(){

  delayMicroseconds(10);
  digitalWrite(trigger, LOW);
  float duration = pulseIn(echo, HIGH);
  float distance = .5*speed*duration;
  Serial.println(distance);
  return distance;

}

String getImage(){
    Serial2.begin(115200, SERIAL_8N1, 16, 17);  // RX=16, TX=17 (adjust as needed)
      Serial2.println("CAPTURE");
    
      // Wait for image start
      while (Serial2.available()) {
        String header = Serial2.readStringUntil('\n');
        if (header.indexOf("IMG_START") >= 0) break;
      }
    
      uint8_t imageBuffer[30000];  // max image size ~30KB
      int index = 0;
    
      unsigned long startTime = millis();
      while (millis() - startTime < 3000) { // timeout after 3 seconds
        if (Serial2.available()) {
          uint8_t b = Serial2.read();
          if (b == 'I') {
            // peek to check for IMG_END
            if (Serial2.peek() == 'M') {
              String tail = Serial2.readStringUntil('\n');
              if (tail.indexOf("IMG_END") >= 0) break;
            }
          }
          if (index < sizeof(imageBuffer)) {
            imageBuffer[index++] = b;
          }
        }
      }
    
      // Base64 encode image
      String imageBase64 = base64::encode(imageBuffer, index);
      return "\"" + imageBase64 + "\"";  // return as a JSON string (quoted)
}

String checkAlignment(float reading){
// Shreyas, try to figure out the correct threshold
    float threshold = 5;
    if(reading<threshold){
        notifyImmediately();
        return "Bad";
    }else{
        return "Good";
    }
}

float getTemperature(){
    // if needed will write the code, for now:
    return 0;
}

String buildJson(){

// create a track_id
   String track_id = "T" + String(999);

// get the time of the reading
  char timeBuffer[20];
  sprintf(timeBuffer, "2025-04-07T%02d:%02d:%02d", random(0, 24), random(0, 60), random(0, 60));
  String time = String(timeBuffer);

  // get ultrasound_reading
  float ultrasound_reading_1 = getUltrasoundReading();    // 1.00 to 5.00 cm
  float ultrasound_reading_2 = -1;

  // get image encoding
  String image_encoding = getImage();

  // get the alignment
  String alignment = checkAlignment(ultrasound_reading_1);

// get temperature // will probably remove
  float temperature = getTemperature();    // 20.0 to 35.0 °C

  // 👉 Carefully escape inner quotes
  String json = "\\\"track_id\\\":\\\"" + track_id + "\\\",";
  json += "\\\"time\\\":\\\"" + time + "\\\",";
  json += "\\\"ultrasound_reading_1\\\":" + String(ultrasound_reading_1, 2) + ",";
  json += "\\\"image_encoding\\\":" + image_encoding + ",";
  json += "\\\"alignment\\\":" + alignment + ",";
  json += "\\\"temperature\\\":\\\"" + String(temperature, 2) + "\\\"";

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
    String sensorData = buildJson();

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
