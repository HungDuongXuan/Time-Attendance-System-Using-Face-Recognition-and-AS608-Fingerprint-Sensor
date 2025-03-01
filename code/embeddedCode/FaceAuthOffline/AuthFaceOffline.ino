#include <Face_recognition_inferencing.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <HTTPClient.h>
#include <AceButton.h>
#include "SD_MMC.h"
#include <time.h>
#include <ESP32Ping.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
#include "camera_pins.h"
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define OLED_SDA 42
#define OLED_SCL 41
#define MIN(a,b) ((a) < (b) ? (a) : (b))

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SD_MMC_CMD 38
#define SD_MMC_CLK 39
#define SD_MMC_D0  40

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS 1600
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS 1200
#define EI_CAMERA_FRAME_BYTE_SIZE 3
// Camera configuration constants
#define CAM_FRAME_SIZE FRAMESIZE_QQVGA  // 160x120 - closest to 100x100
#define CAM_PIXEL_FORMAT PIXFORMAT_RGB565
#define TARGET_IMG_WIDTH 100
#define TARGET_IMG_HEIGHT 100


#define FACE_BUTTON_PIN 21     // Button nhận diện khuôn mặt
#define FINGER_BUTTON_PIN 20   // Button nhận diện vân tay

static uint8_t *snapshot_buf; // Buffer for the image

ace_button::AceButton faceButton(FACE_BUTTON_PIN);

bool isServerOn = false;

void handleEvent(ace_button::AceButton* button, uint8_t eventType, uint8_t buttonState) {
  if (eventType == ace_button::AceButton::kEventPressed) {
    if (button->getPin() == FACE_BUTTON_PIN) {
        handleFaceAuth();
    }
  }
}

#define SECONDS_IN_A_DAY 86400
#define SECONDS_IN_A_HOUR 3600
#define SECONDS_IN_A_MINUTE 60


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.google.com", 7 * 3600);

int daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

bool isLeapYear(int year) {
    return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

void epochToDate(unsigned long epochTime, int &year, int &month, int &day, int &hour, int &minute, int &second) {
    unsigned long days = epochTime / SECONDS_IN_A_DAY;
    unsigned long hours = (epochTime % SECONDS_IN_A_DAY) / SECONDS_IN_A_HOUR;
    unsigned long minutes = (epochTime % SECONDS_IN_A_HOUR) / SECONDS_IN_A_MINUTE;
    unsigned long seconds = epochTime % SECONDS_IN_A_MINUTE;

    year = 1970;
    while (days >= (isLeapYear(year) ? 366 : 365)) {
        days -= (isLeapYear(year) ? 366 : 365);
        year++;
    }

    month = 0;
    while (days >= daysInMonth[month] + (month == 1 && isLeapYear(year))) {
        days -= (daysInMonth[month] + (month == 1 && isLeapYear(year)));
        month++;
    }

    day = days + 1;
    hour = hours;
    minute = minutes;
    second = seconds;
}

String formatTwoDigits(int number) {
    if (number < 10) {
        return "0" + String(number);
    }
    return String(number);
}

String serverIP = "192.168.1.4:5000";

const char* ssid     = "11111111";
const char* password = "111111111";

bool initSDCard() {
    SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
    if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5)) {
        Serial.println("Card Mount Failed");
        return false;
    }

    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD_MMC card attached");
        return false;
    }

    Serial.print("SD_MMC Card Type: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);
    return true;
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  pinMode(FACE_BUTTON_PIN, INPUT_PULLUP);
  faceButton.setEventHandler(handleEvent);

  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (!initSDCard()) {
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("SD Card Init Failed!");
      display.display();
      delay(2000);
  } else {
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("SD Card Init Success!");
      display.display();
      delay(2000);
  }

  if (!SD_MMC.exists("/auth_log.txt")) {
      File file = SD_MMC.open("/auth_log.txt", FILE_WRITE);
      if (file) {
          file.println("Face Authentication Log");
          file.println("----------------------------");
          file.close();
      }
  }

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;    // Định dạng pixel
  config.frame_size   = FRAMESIZE_UXGA;     // Kích thước khung hình
  config.fb_count     = 1;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s) {
    // Điều chỉnh hình ảnh
    s->set_brightness(s, 0);     // -2 to 2
    s->set_contrast(s, 0);       // -2 to 2
    s->set_saturation(s, 0);     // -2 to 2
    s->set_exposure_ctrl(s, 1);  // Tự động phơi sáng
    s->set_gain_ctrl(s, 1);      // Tự động điều chỉnh gain
    s->set_awb_gain(s, 1);       // Tự động cân bằng trắng
    s->set_vflip(s, 1);          // Lật dọc nếu cần
    s->set_hmirror(s, 0);        // Không lật ngang
    
    // Tối ưu cho nhận diện khuôn mặt
    s->set_whitebal(s, 1);       // Bật cân bằng trắng
    s->set_aec2(s, 1);           // Tăng cường phơi sáng
    s->set_denoise(s, 1);        // Giảm nhiễu
  }
  
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  // ESP_PSRAM_ENABLE();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  while (WiFi.STA.hasIP() != true) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");

  timeClient.begin();

  Serial.print("Timekeeping system ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  display.clearDisplay();
  display.setCursor(0,0);
  display.print("Timekeeping system ready!");
  display.display();
  delay(2000);

  // Initialize PSRAM
  if (psramInit()) {
    Serial.println("PSRAM init OK");
    Serial.printf("PSRAM size: %d bytes\n", ESP.getFreePsram());
  } else {
    Serial.println("PSRAM init failed!");
    display.clearDisplay();
    display.println("PSRAM Error!");
    display.display();
    delay(2000);
    return;
  }
}

void loop() {
  timeClient.update();
  faceButton.check();
  delay(200);
}

void save_face_auth_result(String face_id) {
    if (!SD_MMC.begin()) {
        Serial.println("Không thể gắn thẻ SD");
        return;
    }

    File file = SD_MMC.open("/face_auth_log.txt", FILE_APPEND);
    if (!file) {
        Serial.println("Không thể mở file để ghi");
        return;
    }

    unsigned long epochTime = timeClient.getEpochTime();
    Serial.println(epochTime);

    if (epochTime < SECONDS_IN_A_DAY*365) {
        Serial.println("Chưa cập nhật thời gian từ NTP");
        file.close();
        return;
    }

    int year, month, day, hour, minute, second;
    epochToDate(epochTime, year, month, day, hour, minute, second);

    String formattedDate = String(year) + "-" + formatTwoDigits(month + 1) + "-" + formatTwoDigits(day);
    String formattedTime = formatTwoDigits(hour) + ":" + formatTwoDigits(minute) + ":" + formatTwoDigits(second);

    String logEntry = "ID: " + face_id + 
                     ", Time: " + formattedDate + " "+ formattedTime;

    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Auth successful");
    display.println("Face ID: " + face_id);
    display.println("Time: " + formattedDate + " " + formattedTime);
    display.display();

    Serial.println(logEntry);
    
    if (file.println(logEntry)) {
        Serial.println("Ghi log thành công vào file face_auth_log.txt");
    } else {
        Serial.println("Ghi log thất bại");
    }

    file.close();
}

bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    bool do_resize = false;

    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb) {
        ei_printf("Camera capture failed\n");
        return false;
    }

   bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf);

   esp_camera_fb_return(fb);

   if(!converted){
       ei_printf("Conversion failed\n");
       return false;
   }

    if ((img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS)
        || (img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS)) {
        do_resize = true;
    }

    if (do_resize) {
        ei::image::processing::crop_and_interpolate_rgb888(
        out_buf,
        EI_CAMERA_RAW_FRAME_BUFFER_COLS,
        EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
        out_buf,
        img_width,
        img_height);
    }


    return true;
}

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    // we already have a RGB888 buffer, so recalculate offset into pixel index
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        // Swap BGR to RGB here
        // due to https://github.com/espressif/esp32-camera/issues/379
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix + 2] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix];

        // go to the next pixel
        out_ptr_ix++;
        pixel_ix+=3;
        pixels_left--;
    }
    // and done!
    return 0;
}

static bool debug_nn = false;

void handleFaceAuth() {
    // instead of wait_ms, we'll wait on the signal, this allows threads to cancel us...
    if (ei_sleep(5) != EI_IMPULSE_OK) {
        return;
    }

    snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);

    // check if allocation was successful
    if(snapshot_buf == nullptr) {
        ei_printf("ERR: Failed to allocate snapshot buffer!\n");
        return;
    }

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;

    if (ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf) == false) {
        ei_printf("Failed to capture image\r\n");
        free(snapshot_buf);
        return;
    }

    // Run the classifier
    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", err);
        return;
    }

    // print the predictions
    ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
                result.timing.dsp, result.timing.classification, result.timing.anomaly);

#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    ei_printf("Object detection bounding boxes:\r\n");
    String label = "-1";
    float max_score = 0.0f;
    for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.value == 0) {
            continue;
        }
        ei_printf("  %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\r\n",
                bb.label,
                bb.value,
                bb.x,
                bb.y,
                bb.width,
                bb.height);
        if (bb.value > max_score) {
            max_score = bb.value;
            label = bb.label;
        }
    }
    if (label != "-1") {
        ei_printf("Selected: %s (%f)\r\n", result.bounding_boxes[strtoul(label.c_str(), NULL, 10)].label, result.bounding_boxes[strtoul(label.c_str(), NULL, 10)].value);
        save_face_auth_result(label);
    }

    // Print the prediction results (classification)
#else
    ei_printf("Predictions:\r\n");
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        ei_printf("  %s: ", ei_classifier_inferencing_categories[i]);
        ei_printf("%.5f\r\n", result.classification[i].value);
    }
#endif


    free(snapshot_buf);

    // Display results
    // display.clearDisplay();
    // if (max_score > 0.3 && max_ix >= 0) {
    //     Serial.println("Nhan dang OK:");
    //     Serial.println(ei_classifier_inferencing_categories[max_ix]);
    //     Serial.printf("Do tin cay: %.1f%%\n", max_score * 100);
        
    // } else {
    //     Serial.println("Khong nhan dien");
    //     Serial.println("duoc khuon mat!");
    // }
    
    display.display();
    delay(2000);
}