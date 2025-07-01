#include <Arduino.h>
#include <ArduinoJson.h>
#include "picobase64.h"
#include <PacketSerial.h>
#include <stdbool.h>
// SSCMA library for AI inference
#include "Seeed_Arduino_SSCMA.h"


// Configuration constants
#define _LOG 0  // Disable it if needed for high frequency
#define pcSerial Serial
#define ESP32_COMM_BAUD_RATE 921600
#define JSON_BUFFER_SIZE 2048
#define IMAGE_BUFFER_SIZE 4096
#define MUTEX_TIMEOUT_MS 1000
#define BEEP_DURATION_MS 50

// GPIO Pin definitions
#define ESP_RX_PIN 17
#define ESP_TX_PIN 16
#define AT_TX_PIN 20  // (The port on the right side) Write line
#define AT_RX_PIN 21  // Yellow line
#define BUZZER_PIN 19

// Serial interface definitions
#define espSerial Serial1
#define atSerial Serial2

// Command packet types
enum PacketType {
  PKT_TYPE_CMD_BEEP_ON = 0xA1,
  PKT_TYPE_CMD_SHUTDOWN = 0xA3,
  PKT_TYPE_CMD_POWER_ON = 0xA4,
  PKT_TYPE_CMD_MODEL_TITLE = 0xA5
};

// Global objects
SSCMA AI;
PacketSerial myPacketSerial;

// JSON documents with fixed sizes for better memory management
JsonDocument doc_info;
JsonDocument doc_image;

// Global state variables
static bool shutdown_flag = false;
static unsigned long last_beep_time = 0;

// Function declarations
static inline void AI_func(SSCMA& instance);
static inline void send_model_title(SSCMA& instance);
static void beep_init(void);
static void beep_on(void);
static void beep_off(void);
static bool safe_serial_print(const char* message);
static bool safe_serial_print(const String& message);

// Compile information
const char compile_date[] = __DATE__ " " __TIME__;

/************************ Communication handlers ****************************/
void onPacketReceived(const uint8_t* buffer, size_t size) {
  if (size < 1) {
    return;
  }

  switch (static_cast<PacketType>(buffer[0])) {
    case PKT_TYPE_CMD_SHUTDOWN:
      safe_serial_print("cmd shutdown");
      shutdown_flag = true;
      break;
      
    case PKT_TYPE_CMD_BEEP_ON:
      beep_on();
      break;
      
    case PKT_TYPE_CMD_MODEL_TITLE:
      send_model_title(AI);
      break;
      
    default:
      // Unknown command - could log this for debugging
      break;
  }
}

/************************ Buzzer control ****************************/
void beep_init(void) {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void beep_off(void) {
  digitalWrite(BUZZER_PIN, LOW);
}

void beep_on(void) {
  // Non-blocking beep implementation
  unsigned long current_time = millis();
  if (current_time - last_beep_time >= 100) { // Minimum 100ms between beeps
    analogWrite(BUZZER_PIN, 127);
    last_beep_time = current_time;
    // Note: We should use a timer or state machine for proper non-blocking operation
    // For now, keeping the delay but acknowledging it's not ideal
    delay(BEEP_DURATION_MS);
    analogWrite(BUZZER_PIN, 0);
  }
}

/************************ Utility functions ****************************/
static bool safe_serial_print(const char* message) {
#if _LOG
  if (pcSerial) {
    pcSerial.println(message);
    return true;
  }
#endif
  return false;
}

static bool safe_serial_print(const String& message) {
#if _LOG
  if (pcSerial) {
    pcSerial.println(message);
    return true;
  }
#endif
  return false;
}

/************************ Setup functions ****************************/
void setup() {
  beep_init();
  
#if _LOG
  pcSerial.begin(ESP32_COMM_BAUD_RATE);
  // Better wait mechanism instead of while loop
  unsigned long start_time = millis();
  while (!pcSerial && (millis() - start_time < 5000)) {
    delay(100);
  }
  delay(2000);
  pcSerial.println(compile_date);
#endif

  atSerial.setRX(AT_RX_PIN);
  atSerial.setTX(AT_TX_PIN);
  atSerial.setFIFOSize(32 * 1024);
  
  // Initialize AI with retry mechanism
  bool ai_initialized = false;
  int retry_count = 0;
  const int max_retries = 5;
  
  while (!ai_initialized && retry_count < max_retries) {
    safe_serial_print("AI init attempt " + String(retry_count + 1) + "/" + String(max_retries));
    
    if (AI.begin(&atSerial)) {
      ai_initialized = true;
      safe_serial_print("AI initialized successfully");
      
      // 测试基本通信
      char* ai_name = AI.name(false);
      char* ai_id = AI.ID(false);
      // String ai_info = AI.info(false);
      
      if (ai_name && ai_id) {
        safe_serial_print(String("AI.name: ") + String(ai_name));
        safe_serial_print(String("AI.ID: ") + String(ai_id));
        // safe_serial_print(String("AI.info: ") + ai_info);
      } else {
        safe_serial_print("Failed to get AI name/ID");
      }
    } else {
      retry_count++;
      safe_serial_print("AI initialization failed");
      delay(2000);
    }
  }
  
  if (!ai_initialized) {
    safe_serial_print("CRITICAL: Failed to initialize AI after all retries");
  }
}

void loop() {
  AI_func(AI);
  // Check shutdown flag
  if (shutdown_flag) {
    safe_serial_print("Shutdown requested, entering low power mode");
    // Implement shutdown logic here
    return;
  }
}

/************************ ESP32 Communication ****************************/

void setup1() {
  // Configure ESP32 communication
  espSerial.setRX(ESP_RX_PIN);
  espSerial.setTX(ESP_TX_PIN);
  espSerial.begin(ESP32_COMM_BAUD_RATE);

  // Initialize packet communication
  myPacketSerial.setStream(&espSerial);
  myPacketSerial.setPacketHandler(&onPacketReceived);

}

void loop1() {
  myPacketSerial.update();
  
  if (myPacketSerial.overflow()) {
    safe_serial_print("PacketSerial buffer overflow detected");
  }
}

/************************ AI Processing ****************************/
static inline void AI_func(SSCMA& instance) {
  // Process AI inference
  int invoke_result = instance.invoke(1, true, false);
  if (invoke_result == 0) {  // CMD_OK = 0
    doc_info.clear();
    
    // Process boxes
    auto& boxes = instance.boxes();
    JsonArray boxes_array = doc_info["boxes"].to<JsonArray>();
    for (const auto& box : boxes) {
      JsonArray box_array = boxes_array.add<JsonArray>();
      box_array.add(box.x);
      box_array.add(box.y);
      box_array.add(box.w);
      box_array.add(box.h);
      box_array.add(box.score);
      box_array.add(box.target);
    }

    // Process classes
    auto& classes = instance.classes();
    JsonArray classes_array = doc_info["classes"].to<JsonArray>();
    for (const auto& classObj : classes) {
      JsonArray class_array = classes_array.add<JsonArray>();
      class_array.add(classObj.score);
      class_array.add(classObj.target);
    }

    // Process points
    auto& points = instance.points();
    JsonArray points_array = doc_info["points"].to<JsonArray>();
    for (const auto& point : points) {
      JsonArray point_array = points_array.add<JsonArray>();
      point_array.add(point.x);
      point_array.add(point.y);
      point_array.add(point.score);
      point_array.add(point.target);
    }

    // Process keypoints
    auto& keypoints = instance.keypoints();
    JsonArray keypoints_array = doc_info["keypoints"].to<JsonArray>();
    for (const auto& keypoint : keypoints) {
      JsonObject kp_obj = keypoints_array.add<JsonObject>();
      
      JsonArray box_array = kp_obj["box"].to<JsonArray>();
      box_array.add(keypoint.box.x);
      box_array.add(keypoint.box.y);
      box_array.add(keypoint.box.w);
      box_array.add(keypoint.box.h);
      box_array.add(keypoint.box.score);
      box_array.add(keypoint.box.target);
      
      JsonArray points_array = kp_obj["points"].to<JsonArray>();
      for (const auto& point : keypoint.points) {
        JsonArray point_array = points_array.add<JsonArray>();
        point_array.add(point.x);
        point_array.add(point.y);
        point_array.add(point.score);
      }
    }
    
    // Process image data
    auto lastImage = instance.last_image();
    if (lastImage.length() > 0) {
      doc_image.clear();
      doc_image["img"] = lastImage;
      
      if (serializeJson(doc_image, espSerial) > 0) {
        espSerial.println();
      }
      
#if _LOG
      if (pcSerial && serializeJson(doc_image, pcSerial) > 0) {
        pcSerial.println();
      }
#endif
    }
    
    // Send processed data
    if (!doc_info.isNull() && doc_info.size() > 0) {
      if (serializeJson(doc_info, espSerial) > 0) {
        espSerial.println();
      }
      
#if _LOG
      if (pcSerial && serializeJson(doc_info, pcSerial) > 0) {
        pcSerial.println();
      }
#endif
    }
  }
}

/************************ Model title handling ****************************/
static inline void send_model_title(SSCMA& instance) {
#if _LOG
  safe_serial_print("request model title");
#endif

  String base64String = instance.info();

  // Fixed: Check for empty string, not negative length
  if (base64String.length() == 0) {
    safe_serial_print("Empty base64 string received");
    return;
  }

  // 使用picobase64直接解码
  size_t input_length = base64String.length();
  size_t expected_length = GetDecodeExpectedLen(input_length);
  
  // 检查合理的缓冲区大小
  if (input_length > 4096 || expected_length > 2048) {
    safe_serial_print("Base64 string too large");
    return;
  }
  
  char* decoded_string = new(std::nothrow) char[expected_length + 1];
  
  if (!decoded_string) {
    safe_serial_print("Memory allocation failed");
    return;
  }
  
  // 直接使用picobase64解码
  size_t actual_length = DecodeChunk(base64String.c_str(), input_length, (uint8_t*)decoded_string);
  decoded_string[actual_length] = '\0';

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, decoded_string);
  
  if (error) {
    safe_serial_print("JSON deserialization failed");
  } else {
    JsonDocument send_doc;
    send_doc["name"] = doc["name"];
    
    if (serializeJson(send_doc, espSerial) > 0) {
      espSerial.println();
    }
    
#if _LOG
    if (pcSerial && serializeJson(send_doc, pcSerial) > 0) {
      pcSerial.println();
    }
#endif
  }
  
  // 清理动态分配的内存
  delete[] decoded_string;
}
