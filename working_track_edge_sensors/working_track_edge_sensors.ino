#include <WiFi.h>
#include <HTTPClient.h>

#include "esp_camera.h"
#include "base64.h"

#define CAMERA_MODEL_AI_THINKER
const float referenceDistance = 2.9;
const float threshold = 0.5;
// real value should be ideally 1.8 cm but we are using 0.5 cm to scale it down for our model's purposes
// values in cm

// CAMERA PINS
#if defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
#endif

// ULTRASONIC PINS
int trigger = 14;
int echo = 13;

// SPEED OF SOUND
float speed = 0.0343;

// the wifi
const char* ssid = "KaushikPhone";
const char* password = "Wifewifi@1";

// uri
const char* om2m_server = "http://172.20.10.3:5089/~/in-cse/in-name/UltrasoundData";

void setup() {
  Serial.begin(115200);

  // Camera config
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // INITIALIZING ULTRASOUND PINS
  pinMode(trigger, OUTPUT);
  pinMode(echo, INPUT);

  // wifi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
}

float getDistance() {
  digitalWrite(trigger, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigger, LOW);
  float duration = pulseIn(echo, HIGH);
  float distance = .5 * speed * duration;
  return distance;
}

String checkAlignment(float distance) {
  int res = 1;
  if (abs(referenceDistance - distance) > threshold) {
    res = 0;
  }
  if (!res) {
    // write code to send track_condition: Defective to a container in om2m called condition
    HTTPClient http;
    String condition_url = "http://172.20.10.3:5089/~/in-cse/in-name/condition";

    http.begin(condition_url);
    http.addHeader("X-M2M-Origin", "admin:admin");
    http.addHeader("X-M2M-RI", "condReq001");
    http.addHeader("Content-Type", "application/vnd.onem2m-res+json; ty=4");

    // Use triple escaping for OM2M string in "con"
    String innerJSON = "{\\\"track_condition\\\":\\\"Defective\\\"}";
    String payload = "{\"m2m:cin\": {\"con\": \"" + innerJSON + "\"}}";

    Serial.println("Payload to condition:");
    Serial.println(payload);

    int code = http.POST(payload);
    Serial.print("Condition response code: ");
    Serial.println(code);
    Serial.println("Condition response body:");
    Serial.println(http.getString());

    http.end();
  }

  if (res) {
    return "Good";
  } else {
    return "Bad";
  }
}

void sendDataToOM2M(float distance, String encoding) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(om2m_server);
    http.addHeader("X-M2M-Origin", "admin:admin");
    http.addHeader("X-M2M-RI", "esp32Req001");
    http.addHeader("Content-Type", "application/vnd.onem2m-res+json; ty=4");

    String track_id = "T" + String(1000);

    // Get time since ESP started (uptime)
    unsigned long seconds = millis() / 1000;
    int hrs = seconds / 3600;
    int mins = (seconds % 3600) / 60;
    int secs = seconds % 60;

    char timeBuffer[9];
    sprintf(timeBuffer, "%02d:%02d:%02d", hrs, mins, secs);
    String uptime = String(timeBuffer);

    // Hardcoded date (replace with RTC if needed)
    String currentDate = "2025-04-18";

    float ultrasound_reading_1 = distance;
    String image_encoding = encoding;
    String alignment = checkAlignment(ultrasound_reading_1);

    // Build JSON string with escaped quotes
    String json = "\\\"track_id\\\":\\\"" + track_id + "\\\",";
    json += "\\\"date\\\":\\\"" + currentDate + "\\\",";
    json += "\\\"time\\\":\\\"" + uptime + "\\\",";
    json += "\\\"ultrasound_reading_1\\\":\\\"" + String(ultrasound_reading_1, 2) + "\\\",";
    json += "\\\"image_encoding\\\":\\\"" + image_encoding + "\\\",";
    json += "\\\"alignment\\\":\\\"" + alignment + "\\\"";

    json = "{" + json + "}";
    String payload = "{\"m2m:cin\": {\"con\": \"" + json + "\"}}";

    int httpResponseCode = http.POST(payload);
    Serial.print("HTTP Response Code: ");
    Serial.println(httpResponseCode);
    Serial.println("Response:");
    Serial.println(http.getString());

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}


void loop() {

  // Throw away the first few frames
  for (int i = 0; i < 3; i++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) {
      esp_camera_fb_return(fb);  // Return immediately
      delay(100);                // Give sensor time to adjust
    }
  }


  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Failed to capture image");
    delay(5000);
    return;
  }

  // Encode the image in base64
  String image_base64 = base64::encode(fb->buf, fb->len);
  Serial.println("-----BEGIN IMAGE BASE64-----");
  //
  Serial.println(image_base64);
  Serial.println("-----END IMAGE BASE64-----");

  esp_camera_fb_return(fb);

  float distance = getDistance();
  Serial.print("Distance (cm): ");
  sendDataToOM2M(distance, image_base64);

  delay(60000);  // Wait 20 seconds before next image
}
