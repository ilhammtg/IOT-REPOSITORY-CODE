// Blynk Configuration
#define BLYNK_TEMPLATE_ID "TMPL64sUdByNZ"
#define BLYNK_TEMPLATE_NAME "ESP32CAM SECURITY"
#define BLYNK_AUTH_TOKEN "I8A0qu3UGwLhwirKGscEZd4KSPf3faiI" // Ganti dengan token Anda

// Tambahkan library untuk HTTP Client
#include <HTTPClient.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "esp_camera.h"
#include "esp_timer.h"


// WiFi credentials
char ssid[] = "ham";     // Ganti dengan SSID WiFi Anda
char pass[] = "12345678"; // Ganti dengan password WiFi Anda

// Pin definitions
#define PIR_PIN 12
#define BUZZER_PIN 13

// Camera pins untuk AI Thinker ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Variables
bool motionDetected = false;
bool buzzerActive = false;
unsigned long buzzerStartTime = 0;
unsigned long lastMotionTime = 0;
const unsigned long motionCooldown = 10000; // 10 detik cooldown antar deteksi

// Blynk Timer
BlynkTimer timer;

// Konfigurasi server PHP
const char* phpServerURL = "https://data.smkn1gandapura.sch.id/upload.php";
const char* imageViewURL = "https://data.smkn1gandapura.sch.id/uploads/image.jpg";

// Variabel untuk tracking upload
unsigned long lastUploadTime = 0;
int uploadCounter = 0;
bool uploadInProgress = false;

// Fungsi untuk upload gambar ke server PHP
bool uploadImageToServer() {
  if (uploadInProgress) return false;
  
  uploadInProgress = true;
  sendToTerminal("📤 Starting image upload...");
  
  // Capture gambar dari kamera
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed for upload");
    sendToTerminal("❌ Camera capture failed");
    uploadInProgress = false;
    return false;
  }
  
  HTTPClient http;
  http.begin(phpServerURL);
  
  // Siapkan multipart form data
  String boundary = "ESP32CAMBoundary";
  String contentType = "multipart/form-data; boundary=" + boundary;
  http.addHeader("Content-Type", contentType);
  
  // Buat body multipart
  String body = "--" + boundary + "\r\n";
  body += "Content-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\n";
  body += "Content-Type: image/jpeg\r\n\r\n";
  
  String bodyEnd = "\r\n--" + boundary + "--\r\n";
  
  // Hitung total size
  int totalSize = body.length() + fb->len + bodyEnd.length();
  
  // Siapkan payload
  uint8_t* payload = (uint8_t*)malloc(totalSize);
  if (!payload) {
    sendToTerminal("❌ Memory allocation failed");
    esp_camera_fb_return(fb);
    uploadInProgress = false;
    return false;
  }
  
  // Gabungkan semua data
  memcpy(payload, body.c_str(), body.length());
  memcpy(payload + body.length(), fb->buf, fb->len);
  memcpy(payload + body.length() + fb->len, bodyEnd.c_str(), bodyEnd.length());
  
  // Kirim POST request
  sendToTerminal("🌐 Uploading to server...");
  int httpResponseCode = http.POST(payload, totalSize);
  
  // Cleanup
  free(payload);
  esp_camera_fb_return(fb);
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    Serial.printf("Upload success: %s\n", response.c_str());
    sendToTerminal("✅ Upload successful!");
    
    // Increment counter dan update Blynk
    uploadCounter++;
    updateBlynkImageGallery();
    
    http.end();
    uploadInProgress = false;
    return true;
  } else {
    Serial.printf("Upload failed: HTTP %d\n", httpResponseCode);
    sendToTerminal("❌ Upload failed: HTTP " + String(httpResponseCode));
    http.end();
    uploadInProgress = false;
    return false;
  }
}

// Fungsi untuk update Image Gallery di Blynk
void updateBlynkImageGallery() {
  // Image Gallery Widget butuh integer untuk trigger update
  // Kita kirim timestamp sebagai trigger
  unsigned long timestamp = millis() / 1000;
  
  sendToTerminal("🖼️ Updating image gallery...");
  sendToTerminal("📷 Image URL: " + String(imageViewURL));
  sendToTerminal("🔢 Trigger: " + String(timestamp));
  
  // Kirim ke V0 (Image Gallery)
  Blynk.virtualWrite(V0, timestamp);
  
  // Kirim URL ke terminal untuk referensi
  sendToTerminal("🌐 View at: " + String(imageViewURL));
}

// Update fungsi startVideoCapture untuk upload gambar
void startVideoCapture() {
  sendToTerminal("📸 Motion detected - capturing image...");
  
  // Upload gambar ke server
  if (uploadImageToServer()) {
    sendToTerminal("🎯 Image uploaded successfully!");
    sendToTerminal("📊 Upload count: " + String(uploadCounter));
  } else {
    sendToTerminal("⚠️ Upload failed, retrying...");
    // Retry setelah 2 detik
    delay(2000);
    uploadImageToServer();
  }
  
  sendToTerminal("---");
}

// Manual upload via button V4
BLYNK_WRITE(V4) {
  int captureCmd = param.asInt();
  if (captureCmd == 1) {
    Serial.println("Manual image capture requested");
    sendToTerminal("📹 Manual capture requested");
    
    if (uploadImageToServer()) {
      sendToTerminal("✅ Manual upload successful!");
    } else {
      sendToTerminal("❌ Manual upload failed!");
    }
  }
}

// Tambahkan fungsi untuk test koneksi server
void testServerConnection() {
  HTTPClient http;
  http.begin(phpServerURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    sendToTerminal("✅ Server reachable: HTTP " + String(httpResponseCode));
  } else {
    sendToTerminal("❌ Server unreachable: " + String(httpResponseCode));
  }
  
  http.end();
}

// Update fungsi setup untuk test server
void setup() {
  Serial.begin(115200);
  Serial.println("=== ESP32-CAM Security System Starting ===");
  
  // Initialize pins
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.println("1. Pins initialized");
  
  // Initialize camera
  initCamera();
  
  // Connect to WiFi and Blynk
  Serial.println("3. Connecting to WiFi...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\n4. WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Initialize Blynk
  Serial.println("5. Connecting to Blynk...");
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("6. Blynk Connected!");
  
  // Test server connection
  Serial.println("7. Testing server connection...");
  testServerConnection();
  
  // Setup timers
  timer.setInterval(500L, checkPIR);
  timer.setInterval(100L, checkBuzzer);
  // Remove stream timer karena tidak diperlukan lagi
  
  Serial.println("=== System Ready! ===");
  Serial.printf("Upload Server: %s\n", phpServerURL);
  Serial.printf("Image View: %s\n", imageViewURL);
  Serial.println("Waiting for motion detection...");
}

void loop() {
  Blynk.run();
  timer.run();
}

// Tambahkan fungsi initCamera()
void initCamera() {
  Serial.println("2. Initializing camera...");
  
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
  
  // Frame size untuk capture
  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA; // 640x480
    config.jpeg_quality = 12;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA; // 320x240
    config.jpeg_quality = 15;
    config.fb_count = 1;
  }
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init FAILED with error 0x%x\n", err);
    sendToTerminal("❌ Camera init failed!");
    return;
  }
  Serial.println("Camera initialized successfully!");
  sendToTerminal("✅ Camera initialized");
}

// Fungsi sendToTerminal
void sendToTerminal(String message) {
  // Format: [timestamp] message
  unsigned long currentTime = millis() / 1000;
  String timeStamp = "[" + String(currentTime) + "s] ";
  Blynk.virtualWrite(V1, timeStamp + message);
  
  // Also print to Serial for debugging
  Serial.println(timeStamp + message);
}

// Fungsi checkPIR
void checkPIR() {
  int pirState = digitalRead(PIR_PIN);
  
  if (pirState == HIGH && !motionDetected) {
    // Cek cooldown untuk menghindari spam
    if (millis() - lastMotionTime > motionCooldown) {
      motionDetected = true;
      lastMotionTime = millis();
      
      Serial.println(">>> MOTION DETECTED! <<<");
      sendToTerminal("🚨 MOTION DETECTED!");
      
      // Start image capture and upload
      startVideoCapture();
    }
  }
  
  if (pirState == LOW && motionDetected) {
    motionDetected = false;
    Serial.println("Motion ended.");
  }
}

// Tombol Alert - V2 (Button Widget)
BLYNK_WRITE(V2) {
  int buttonState = param.asInt();
  
  if (buttonState == 1) {
    Serial.println(">>> ALERT BUTTON PRESSED! <<<");
    sendToTerminal("🚨 ALERT ACTIVATED!");
    sendToTerminal("🔊 Unknown person detected!");
    
    // Aktifkan buzzer
    buzzerActive = true;
    buzzerStartTime = millis();
    digitalWrite(BUZZER_PIN, HIGH);
    
    sendToTerminal("📢 Buzzer ON - 6 seconds");
    Serial.println("Buzzer ON - 6 seconds countdown started");
  }
}

void checkBuzzer() {
  if (buzzerActive) {
    // Cek apakah sudah 6 detik
    if (millis() - buzzerStartTime >= 6000) {
      // Matikan buzzer
      digitalWrite(BUZZER_PIN, LOW);
      buzzerActive = false;
      
      Serial.println("Buzzer OFF - Alert finished");
      sendToTerminal("✅ Alert finished");
      sendToTerminal("👀 System ready...");
    }
  }
}

// Reset System Button - V3 (Button Widget)
BLYNK_WRITE(V3) {
  int resetCmd = param.asInt();
  if (resetCmd == 1) {
    Serial.println("System reset command received");
    
    // Reset semua status
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
    motionDetected = false;
    uploadInProgress = false;
    
    sendToTerminal("🔄 SYSTEM RESET");
    sendToTerminal("✅ All alerts cleared");
    sendToTerminal("📡 IP: " + WiFi.localIP().toString());
    sendToTerminal("👀 Ready for monitoring");
    sendToTerminal("---");
    
    Serial.println("System reset completed");
  }
}

// Update BLYNK_CONNECTED callback
BLYNK_CONNECTED() {
  Serial.println("Blynk connected successfully!");
  sendToTerminal("🟢 ESP32-CAM Security Online");
  sendToTerminal("📡 WiFi IP: " + WiFi.localIP().toString());
  sendToTerminal("🎥 Camera: " + String(psramFound() ? "VGA" : "QVGA"));
  sendToTerminal("🌐 Server: " + String(phpServerURL));
  sendToTerminal("🖼️ Image Gallery: V0");
  sendToTerminal("⚡ System ready!");
  sendToTerminal("---");
}
