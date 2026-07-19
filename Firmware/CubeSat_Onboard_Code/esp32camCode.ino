#include <Arduino.h>
#include "esp_camera.h"
#include "esp_timer.h"
#include <math.h>

// ===== PINS =====
#define AUDIO_PIN    2    // GPIO2  → LPF → CD4066 → SA868 MIC
#define TRIGGER_PIN  15   // GPIO14 → from ESP32 GPIO25
#define LED_FLASH    4    // GPIO4  → Flash LED

// ===== PWM =====
#define AUDIO_CH   0
#define PWM_FREQ   62500
#define PWM_BITS   8

// ===== SSTV =====
#define SAMPLE_RATE  11025
#define W  320
#define H  256

#define SYNC_US    4862
#define BLANK_US    572
#define SCAN_US  146432

#define SYNC_SAMPLES  ((uint32_t)((uint64_t)SAMPLE_RATE * SYNC_US  / 1000000ULL))
#define BLANK_SAMPLES ((uint32_t)((uint64_t)SAMPLE_RATE * BLANK_US / 1000000ULL))
#define SCAN_SAMPLES  ((uint32_t)((uint64_t)SAMPLE_RATE * SCAN_US  / 1000000ULL))

// ===== DDS =====
static int16_t sineLUT[256];
static volatile uint32_t phaseAcc  = 0;
static volatile uint32_t phaseStep = 0;

// ===== IMAGE BUFFER =====
uint8_t *rgbImage = nullptr;

// ===== AUDIO ISR =====
void IRAM_ATTR onAudioTimer(void *) {
  phaseAcc += phaseStep;
  uint8_t idx = phaseAcc >> 24;
  uint32_t pwm = ((int32_t)sineLUT[idx] + 32768) >> (16 - PWM_BITS);
  ledcWrite(AUDIO_CH, pwm);
}

inline void setTone(uint32_t f) {
  phaseStep = ((uint64_t)f << 32) / SAMPLE_RATE;
}

void toneSamples(uint32_t freq, uint32_t samples) {
  setTone(freq);
  delayMicroseconds((uint64_t)samples * 1000000ULL / SAMPLE_RATE);
}

inline int pixToFreq(uint8_t v) {
  return 1500 + (v * 800) / 255;
}

// ===== VIS CODE — Martin-1 =====
void sendVIS() {
  toneSamples(1900, SAMPLE_RATE * 300 / 1000);
  toneSamples(1200, SAMPLE_RATE * 10  / 1000);
  toneSamples(1900, SAMPLE_RATE * 300 / 1000);
  toneSamples(1200, SAMPLE_RATE * 30  / 1000);

  uint8_t vis = 44;  // Martin-1
  int ones = 0;
  for (int i = 0; i < 7; i++) {
    if (vis & (1 << i)) { toneSamples(1100, SAMPLE_RATE * 30 / 1000); ones++; }
    else                  toneSamples(1300, SAMPLE_RATE * 30 / 1000);
  }
  toneSamples((ones & 1) ? 1100 : 1300, SAMPLE_RATE * 30 / 1000);
  toneSamples(1200, SAMPLE_RATE * 30 / 1000);
}

// ===== CAMERA INIT =====
bool initCamera() {
  camera_config_t cfg = {
    .pin_pwdn     = 32,
    .pin_reset    = -1,
    .pin_xclk     = 0,
    .pin_sscb_sda = 26,
    .pin_sscb_scl = 27,
    .pin_d7       = 35,
    .pin_d6       = 34,
    .pin_d5       = 39,
    .pin_d4       = 36,
    .pin_d3       = 21,
    .pin_d2       = 19,
    .pin_d1       = 18,
    .pin_d0       = 5,
    .pin_vsync    = 25,
    .pin_href     = 23,
    .pin_pclk     = 22,
    .xclk_freq_hz = 20000000,
    .ledc_timer   = LEDC_TIMER_1,
    .ledc_channel = LEDC_CHANNEL_1,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size   = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count     = 1,
    .fb_location  = CAMERA_FB_IN_PSRAM
  };
  return esp_camera_init(&cfg) == ESP_OK;
}

// ===== CAPTURE =====
bool captureImage() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return false;

  rgbImage = (uint8_t *)ps_malloc(W * H * 3);
  if (!rgbImage) { esp_camera_fb_return(fb); return false; }

  fmt2rgb888(fb->buf, fb->len, fb->format, rgbImage);
  esp_camera_fb_return(fb);
  return true;
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial.println("\n=============================");
  Serial.println("   ESP32-CAM SSTV SYSTEM");
  Serial.println("=============================\n");

  // GPIO15 safe — must be LOW during boot
  // ESP32 GPIO25 idles LOW so this is safe
  pinMode(TRIGGER_PIN, INPUT_PULLDOWN);
  pinMode(LED_FLASH, OUTPUT);
  digitalWrite(LED_FLASH, LOW);

  // PWM — core 3.x API
  ledcAttachChannel(AUDIO_PIN, PWM_FREQ, PWM_BITS, AUDIO_CH);

  // Sine LUT
  for (int i = 0; i < 256; i++)
    sineLUT[i] = (int16_t)(sin(2.0 * PI * i / 256.0) * 16000);

  // Audio timer
  esp_timer_handle_t audioTimer;
  const esp_timer_create_args_t timerArgs = {
    .callback = &onAudioTimer,
    .name     = "sstv_audio"
  };
  esp_timer_create(&timerArgs, &audioTimer);
  esp_timer_start_periodic(audioTimer, 1000000 / SAMPLE_RATE);  // 90us

  // Camera init
  if (!initCamera()) {
    Serial.println("Camera FAIL — check module");
    while (1);
  }
  Serial.println("Camera OK");

  Serial.println("Waiting for trigger on GPIO15...");

  // Wait for trigger pulse from ESP32 GPIO25
  while (!digitalRead(TRIGGER_PIN)) delay(10);

  Serial.println("Triggered! Capturing image...");
  digitalWrite(LED_FLASH, HIGH);
  delay(100);
  digitalWrite(LED_FLASH, LOW);

  if (!captureImage()) {
    Serial.println("Capture FAIL");
    while (1);
  }
  Serial.println("Image captured OK");

  Serial.println("TX START — Martin-1 ~114 seconds");
  sendVIS();

  for (int y = 0; y < H; y++) {
    toneSamples(1200, SYNC_SAMPLES);

    auto scan = [&](int ch) {
      uint32_t prev = 0;
      for (int x = 0; x < W; x++) {
        uint32_t pos = ((uint64_t)(x + 1) * SCAN_SAMPLES) / W;
        uint32_t n   = pos - prev;
        prev = pos;
        uint8_t pix = rgbImage[(y * W + x) * 3 + ch];
        toneSamples(pixToFreq(pix), n);
      }
    };

    scan(1); toneSamples(1500, BLANK_SAMPLES);  // Green
    scan(0); toneSamples(1500, BLANK_SAMPLES);  // Blue
    scan(2);                                     // Red

    if (y % 32 == 0) {
      Serial.print("Line ");
      Serial.print(y);
      Serial.print(" / ");
      Serial.println(H);
    }
  }

  Serial.println("TX DONE");
}

void loop() {}
