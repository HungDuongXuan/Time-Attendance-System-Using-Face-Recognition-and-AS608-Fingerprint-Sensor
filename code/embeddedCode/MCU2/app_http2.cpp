#include <WiFi.h>
#include <esp_http_server.h>
#include <esp_timer.h>
#include <esp_camera.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "ff.h" // FATFS library
#include "fingerprint_auth.h"
// In app_http2.cpp
// #include <Face_recognition_inferencing.h>
// #include "edge-impulse-sdk/dsp/image/image.hpp"
// #include "edge_impulse_camera.h"

// Định nghĩa cho OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_SDA 42  // Chân SDA của OLED
#define OLED_SCL 41  // Chân SCL của OLED
Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// Định nghĩa cho streaming
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// Định nghĩa cho AS608
#define FINGERPRINT_RX 36
#define FINGERPRINT_TX 37
#define MAX_RETRY 5

// Camera constants
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           200
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           200
#define EI_CAMERA_FRAME_BYTE_SIZE                 3

extern fingerprint_auth_result_t auth_result;

HardwareSerial fingerprintSerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerprintSerial);
bool isSensorConnected = false;

// Hàm hiển thị thông báo lên màn hình OLED
void showScreen(const char* line1, const char* line2 = "", const char* line3 = "", const char* line4 = "") {
    display1.clearDisplay();
    display1.setTextSize(1);
    display1.setTextColor(SSD1306_WHITE);
    
    display1.setCursor(0,0);
    display1.println(line1);
    
    if (strlen(line2) > 0) {
        display1.setCursor(0,16);
        display1.println(line2);
    }
    
    if (strlen(line3) > 0) {
        display1.setCursor(0,32);
        display1.println(line3);
    }
    
    if (strlen(line4) > 0) {
        display1.setCursor(0,48);
        display1.println(line4);
    }
    
    display1.display();
    delay(3000);
}

// Hàm khởi tạo AS608
bool initAS608() {
    Serial.println("\nInitializing AS608 Fingerprint Sensor...");
    
    // Reset Serial trước khi khởi tạo
    fingerprintSerial.end();
    delay(100);
    
    // Khởi tạo lại với tốc độ baud chuẩn cho AS608
    fingerprintSerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
    delay(100);
    
    // Thiết lập password mặc định (0x0)
    finger.begin(57600);
    
    int retry = 0;
    while (retry < MAX_RETRY) {
        if (finger.verifyPassword()) {
            Serial.println("AS608 Found!");
            // Đọc và hiển thị thông số
            finger.getParameters();
            Serial.print("Status: 0x"); Serial.println(finger.status_reg, HEX);
            Serial.print("Sys ID: 0x"); Serial.println(finger.system_id, HEX);
            Serial.print("Capacity: "); Serial.println(finger.capacity);
            Serial.print("Security level: "); Serial.println(finger.security_level);
            return true;
        }
        
        Serial.print("AS608 not found, attempt "); 
        Serial.print(retry + 1); 
        Serial.print(" of "); 
        Serial.println(MAX_RETRY);
        
        retry++;
        delay(1000);
    }
    
    return false;
}


// // Biến global cho camera buffer
// static uint8_t *snapshot_buf = nullptr;

// extern "C" {
//     // Hàm chụp và xử lý ảnh từ camera
//     bool capture_camera_image(uint32_t width, uint32_t height, uint8_t *buf) {
//         camera_fb_t *fb = esp_camera_fb_get();
//         if (!fb) {
//             return false;
//         }

//         bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, buf);
//         esp_camera_fb_return(fb);

//         return converted;
//     }

//     // Hàm lấy dữ liệu ảnh cho Edge Impulse
//     int get_camera_data(size_t offset, size_t length, float *out_ptr) {
//         size_t pixel_ix = offset * 3;
//         size_t pixels_left = length;
//         size_t out_ptr_ix = 0;

//         while (pixels_left != 0) {
//             out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix + 2] << 16) + 
//                                  (snapshot_buf[pixel_ix + 1] << 8) + 
//                                  snapshot_buf[pixel_ix];
//             out_ptr_ix++;
//             pixel_ix += 3;
//             pixels_left--;
//         }
//         return 0;
//     }

//     // Hàm cấp phát bộ nhớ cho camera buffer
//     bool allocate_camera_buffer() {
//         if (snapshot_buf != nullptr) {
//             free(snapshot_buf);
//             snapshot_buf = nullptr;
//         }

//         #if defined(BOARD_HAS_PSRAM)
//         if (psramFound()) {
//             snapshot_buf = (uint8_t*)ps_malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * 
//                                              EI_CAMERA_RAW_FRAME_BUFFER_ROWS * 
//                                              EI_CAMERA_FRAME_BYTE_SIZE);
//         }
//         #endif

//         if (snapshot_buf == nullptr) {
//             snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * 
//                                           EI_CAMERA_RAW_FRAME_BUFFER_ROWS * 
//                                           EI_CAMERA_FRAME_BYTE_SIZE);
//         }

//         return (snapshot_buf != nullptr);
//     }

//     // Hàm giải phóng camera buffer
//     void free_camera_buffer() {
//         if (snapshot_buf) {
//             free(snapshot_buf);
//             snapshot_buf = nullptr;
//         }
//     }
// }


// Handler cho camera stream (giữ nguyên code cũ của bạn)
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK) {
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    while(true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        } else {
            if(fb->format != PIXFORMAT_JPEG) {
                bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                esp_camera_fb_return(fb);
                fb = NULL;
                if(!jpeg_converted) {
                    Serial.println("JPEG compression failed");
                    res = ESP_FAIL;
                }
            } else {
                _jpg_buf_len = fb->len;
                _jpg_buf = fb->buf;
            }
        }
        if(res == ESP_OK) {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(res == ESP_OK) {
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if(_jpg_buf) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if(res != ESP_OK) {
            break;
        }
    }
    return res;
}

uint8_t getFingerprintEnroll(int id,httpd_req_t *req) {
  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }
  
  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  char response [128];
  snprintf(response, sizeof(response), "%d enroll success", id);

  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
    httpd_resp_sendstr(req, response);
    showScreen(response);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    showScreen("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    showScreen("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
      showScreen("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    showScreen("Unknown error");
    return p;
  }



  return true;
}

static esp_err_t enroll_handler(httpd_req_t *req) {
    if (!isSensorConnected) {
        httpd_resp_sendstr(req, "Fingerprint sensor not connected");
        return ESP_FAIL;
    }

    char *buf;
    size_t buf_len;
    char param[32];
    char response[128];
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len <= 1) {
        httpd_resp_sendstr(req, "Missing ID parameter");
        return ESP_OK;
    }
    
    buf = (char*)malloc(buf_len);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
        if (httpd_query_key_value(buf, "id", param, sizeof(param)) == ESP_OK) {
            int id = atoi(param);
            if (id <= 0 || id > 127) {
                httpd_resp_sendstr(req, "Invalid ID (must be 1-127)");
                showScreen("Invalid ID (must be 1-127)");
                free(buf);
                return ESP_OK;
            }
            while (! getFingerprintEnroll(id, req) );
            // httpd_resp_sendstr(req, response);
        }
    }
    
    free(buf);
    return ESP_OK;
}


// Hàm xác thực vân tay
esp_err_t verify() {
    if (!isSensorConnected) {
        return ESP_FAIL;
    }

    uint8_t p = FINGERPRINT_NOFINGER;
    int timeout = 0;
    
    while (p != FINGERPRINT_OK && timeout < 100) {
        p = finger.getImage();
        if (p == FINGERPRINT_OK) {
            p = finger.image2Tz();
            if (p != FINGERPRINT_OK) {
                return ESP_FAIL;
            }
            
            p = finger.fingerFastSearch();
            if (p == FINGERPRINT_OK) {
                auth_result.fingerID = finger.fingerID;
                auth_result.confidence = finger.confidence;
                // auth_result.timestamp = time(NULL);
                return ESP_OK;
            } else if (p == FINGERPRINT_NOTFOUND) {
                return ESP_FAIL;
            }
        }
        delay(100);
        timeout++;
    }
    
    if (timeout >= 100) {
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// Hàm xử lý xác thực vân tay
esp_err_t verify_handler(httpd_req_t *req) {
    esp_err_t result = verify();

    if (result == ESP_OK) {
        // Gửi thông báo lên server
        // sendMessageToServer("Authentication by fingerprint");
        
        // Trả về thông tin xác thực
        char response[128];
        snprintf(response, sizeof(response), 
                "Found ID #%d with confidence %d", 
                auth_result.fingerID, 
                auth_result.confidence);
        httpd_resp_sendstr(req, response);
    } else {
        httpd_resp_sendstr(req, "Authentication failed");
    }

    return result;
}


// Handler xóa vân tay
static esp_err_t delete_handler(httpd_req_t *req) {
    if (!isSensorConnected) {
        httpd_resp_sendstr(req, "Fingerprint sensor not connected");
        showScreen("Fingerprint sensor not connected");
        return ESP_FAIL;
    }

    char *buf;
    size_t buf_len;
    char param[32];
    char response[128];
    
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len <= 1) {
        httpd_resp_sendstr(req, "Missing ID parameter");
        showScreen("Missing ID parameter");
        return ESP_OK;
    }
    
    buf = (char*)malloc(buf_len);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
        if (httpd_query_key_value(buf, "id", param, sizeof(param)) == ESP_OK) {
            int id = atoi(param);
            if (id <= 0 || id > 127) {
                httpd_resp_sendstr(req, "Invalid ID (must be 1-127)");
                showScreen("Invalid ID (must be 1-127)");
                // delay(1000);
                // display1.clearDisplay();
                free(buf);
                return ESP_OK;
            }
            
            uint8_t p = finger.deleteModel(id);
            if (p == FINGERPRINT_OK) {
                snprintf(response, sizeof(response), "Deleted ID %d", id);
            } else {
                snprintf(response, sizeof(response), "Error deleting ID %d: %d", id, p);
            }
            
            httpd_resp_sendstr(req, response);
            showScreen(response);
        }
    }
    
    free(buf);
    return ESP_OK;
}

void startServer() {
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display1.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.println("SSD1306 allocation failed");
      return;
  }
  display1.clearDisplay();
  showScreen("Starting", "System...");

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  
  Serial.begin(115200);
  while (!Serial) {
      delay(100);
  }
  
  config.server_port = 80;
  config.ctrl_port = 32768;
  
  isSensorConnected = initAS608();
  if (!isSensorConnected) {
      showScreen("AS608 Error", "Check:", "1. Power 3.3V", "2. Connections");
      Serial.println("Failed to initialize AS608!");
      Serial.println("1. Check power (3.3V for AS608)");
      Serial.println("2. Check TX/RX connections");
      Serial.println("3. Check if sensor is damaged");
  } else {
    showScreen("fingerprint success");
  }

  httpd_handle_t server = NULL;
  
  httpd_uri_t stream_uri = {
      .uri       = "/stream",
      .method    = HTTP_GET,
      .handler   = stream_handler,
      .user_ctx  = NULL
  };
  
  httpd_uri_t enroll_uri = {
      .uri       = "/enroll",
      .method    = HTTP_GET,
      .handler   = enroll_handler,
      .user_ctx  = NULL
  };
  
  httpd_uri_t verify_uri = {
      .uri       = "/verify",
      .method    = HTTP_GET,
      .handler   = verify_handler,
      .user_ctx  = NULL
  };
  httpd_uri_t delete_uri = {
      .uri       = "/delete",
      .method    = HTTP_GET,
      .handler   = delete_handler,
      .user_ctx  = NULL
  };
  
  if (httpd_start(&server, &config) == ESP_OK) {
      httpd_register_uri_handler(server, &stream_uri);
      httpd_register_uri_handler(server, &enroll_uri);
      httpd_register_uri_handler(server, &verify_uri);
      httpd_register_uri_handler(server, &delete_uri);
      Serial.printf("Server started on port: '%d'\n", config.server_port);
      showScreen(("Server started on port: " + String(config.server_port)).c_str());
  } else {
      Serial.println("Failed to start server");
      showScreen("Failed to start server");
  }
}