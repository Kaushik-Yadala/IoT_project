void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, 16, 17); // RX=16, TX=17 (connect ESP32-CAM TX to RX)
  
    Serial.println("Waiting for image...");
  }
  
  void loop() {
    static enum { WAITING, READING_LENGTH, READING_IMAGE } state = WAITING;
    static String lengthStr = "";
    static int imageLength = 0;
    static int bytesRead = 0;
    static uint8_t* imageBuffer = nullptr;
  
    while (Serial2.available()) {
      char c = Serial2.read();
  
      switch (state) {
        case WAITING:
          if (c == 'I' && Serial2.peek() == 'M') {
            String header = "";
            header += c;
            for (int i = 0; i < 9 && Serial2.available(); i++) {
              header += (char)Serial2.read();
            }
            if (header.startsWith("IMG_START")) {
              lengthStr = "";
              state = READING_LENGTH;
            }
          }
          break;
  
        case READING_LENGTH:
          if (c == ':') {
            imageLength = lengthStr.toInt();
            Serial.print("Receiving image of size: ");
            Serial.println(imageLength);
  
            if (imageLength > 0 && imageLength < 100000) {
              imageBuffer = (uint8_t*)malloc(imageLength);
              bytesRead = 0;
              state = READING_IMAGE;
            } else {
              Serial.println("Invalid length");
              state = WAITING;
            }
          } else {
            lengthStr += c;
          }
          break;
  
        case READING_IMAGE:
          imageBuffer[bytesRead++] = (uint8_t)c;
          if (bytesRead >= imageLength) {
            Serial.println("✅ Image received successfully");
  
            // Do something with imageBuffer...
            // For now just free it
            free(imageBuffer);
            imageBuffer = nullptr;
            state = WAITING;
          }
          break;
      }
    }
  }
  