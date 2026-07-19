#include <stdio.h>
#include "esp_timer.h"
#include <ctype.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <Adafruit_MCP23X17.h>
#include <driver/dac.h>
#include <math.h>
#include <MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <QMC5883LCompass.h>
#include <TinyGPS++.h>
#include <Adafruit_INA219.h>
#include <SPI.h>
#include <LoRa.h>

// ============================================
// MCP23017
// ============================================
Adafruit_MCP23X17 mcp;

#define MCP_CTRL_APRS  3
#define MCP_PTT_VHF    4
#define MCP_CTRL_SSTV  2
#define MCP_PTT_UHF    5

// ============================================
// ESP32 PINS
// ============================================
#define DAC_APRS     DAC_CHANNEL_2
#define TX_VHF       13
#define RX_VHF       14
#define GPS_RX       17
#define GPS_TX       16
#define TRIGGER_PIN  25
#define NANO_RX      32
#define NANO_TX      27

// ============================================
// LORA PINS
// ============================================
#define LORA_SS    5
#define LORA_RST   33
#define LORA_DIO0  35
#define LORA_SCK   18
#define LORA_MOSI  23
#define LORA_MISO  34

// ============================================
// CALLSIGN
// ============================================
#define MY_CALLSIGN  "ABHI"
#define MY_SSID      10

// ============================================
// SERIAL
// ============================================
HardwareSerial Serial2Dev(2);
HardwareSerial SerialGPS(1);

// ============================================
// SENSORS
// ============================================
TinyGPSPlus             gps;
MPU6050                 mpu;
Adafruit_BMP085_Unified bmp(10085);
QMC5883LCompass         compass;
Adafruit_INA219         ina219;

const int MAG_OFFSET_X =  849;
const int MAG_OFFSET_Y =  217;
const int MAG_OFFSET_Z = -180;

float groundPressure   = 1013.25;
bool  groundCalibrated = false;

char  gps_lat_str[12] = "0000.00N";
char  gps_lon_str[12] = "00000.00E";
float gps_alt_m       = 0;
int   gps_sats        = 0;
bool  gps_valid       = false;

float bmp_temp = 0, bmp_pres = 0, bmp_alt = 0;
float imu_roll = 0, imu_pitch = 0, imu_yaw = 0;
float ax_g = 0, ay_g = 0, az_g = 0;
float gx_d = 0, gy_d = 0, gz_d = 0;
float mag_x = 0, mag_y = 0, mag_z = 0;
float ina_voltage = 0, ina_current = 0, ina_power = 0;

char info[120];

// ============================================
// LORA TIMING
// ============================================
unsigned long lastLoRaTX = 0;
#define LORA_INTERVAL  1000

// ============================================
// MODE
// ============================================
enum Mode { IDLE_MODE, APRS_MODE, SSTV_MODE };
Mode currentMode = IDLE_MODE;

// ============================================
// UART2 MODE
// ============================================
enum UARTMode { UART_NANO, UART_DRA818 };
UARTMode uart2Mode = UART_NANO;

void switchToNano()
{
  if (uart2Mode == UART_NANO) return;
  Serial2Dev.end();
  delay(50);
  Serial2Dev.begin(9600, SERIAL_8N1, NANO_RX, NANO_TX);
  uart2Mode = UART_NANO;
}

void switchToDRA818()
{
  if (uart2Mode == UART_DRA818) return;
  Serial2Dev.end();
  delay(50);
  Serial2Dev.begin(9600, SERIAL_8N1, RX_VHF, TX_VHF);
  uart2Mode = UART_DRA818;
}

// ============================================
// AFSK
// ============================================
const int   SAMPLE_RATE     = 9600;
const int   SAMPLES_PER_BIT = SAMPLE_RATE / 1200;
const float INC_MARK  = (2.0f * PI * 1200.0f) / SAMPLE_RATE;
const float INC_SPACE = (2.0f * PI * 2200.0f) / SAMPLE_RATE;

static uint8_t frame_bytes[300];
static bool    data_bits[4096];
static bool    bit_tones[4096];

volatile int  total_bits  = 0;
volatile int  cur_bit     = 0;
volatile int  samp_in_bit = 0;
volatile bool tx_active   = false;

float phase = 0;
esp_timer_handle_t audio_timer;

// ============================================
// DRA818
// ============================================
String readResp(uint16_t timeout = 800)
{
  unsigned long s = millis();
  String r;
  while (millis() - s < timeout)
    if (Serial2Dev.available())
      r += char(Serial2Dev.read());
  return r;
}

void bootDRA818()
{
  switchToDRA818();
  delay(1000);
  Serial2Dev.print("AT+DMOCONNECT\r\n");
  readResp();
  Serial2Dev.print("AT+DMOSETGROUP=0,144.8000,144.8000,0000,4,0000\r\n");
  readResp();
  Serial2Dev.print("AT+SETFILTER=0,0,0\r\n");
  readResp();
}

// ============================================
// CHECK DTMF
// ============================================
void checkDTMF()
{
  switchToNano();
  if (Serial2Dev.available()) {
    String cmd = Serial2Dev.readStringUntil('\n');
    cmd.trim();
    if (cmd.startsWith("CMD:")) {
      int val = cmd.substring(4).toInt();
      switch (val) {
        case 1: currentMode = IDLE_MODE; break;
        case 2: currentMode = APRS_MODE; break;
        case 3: currentMode = SSTV_MODE; break;
      }
    }
  }
}

// ============================================
// GPS FORMAT
// ============================================
void formatAPRSLat(float lat, char *out)
{
  char hemi = (lat >= 0) ? 'N' : 'S';
  lat = fabs(lat);
  int deg = (int)lat;
  float min = (lat - deg) * 60.0;
  snprintf(out, 12, "%02d%05.2f%c", deg, min, hemi);
}

void formatAPRSLon(float lon, char *out)
{
  char hemi = (lon >= 0) ? 'E' : 'W';
  lon = fabs(lon);
  int deg = (int)lon;
  float min = (lon - deg) * 60.0;
  snprintf(out, 12, "%03d%05.2f%c", deg, min, hemi);
}

// ============================================
// READ SENSORS
// ============================================
void readSensors()
{
  unsigned long gpsStart = millis();
  while (millis() - gpsStart < 500)
    if (SerialGPS.available())
      gps.encode(SerialGPS.read());

  if (gps.location.isValid()) {
    formatAPRSLat(gps.location.lat(), gps_lat_str);
    formatAPRSLon(gps.location.lng(), gps_lon_str);
    gps_alt_m = gps.altitude.meters();
    gps_sats  = gps.satellites.value();
    gps_valid = true;
  } else {
    gps_valid = false;
  }

  sensors_event_t event;
  bmp.getEvent(&event);
  if (event.pressure) {
    bmp_pres = event.pressure;
    bmp.getTemperature(&bmp_temp);
    if (!groundCalibrated) {
      groundPressure = bmp_pres;
      groundCalibrated = true;
    }
    bmp_alt = 44330.0 * (1.0 - pow(bmp_pres / groundPressure, 0.1903));
  }

  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  ax_g = ax / 16384.0;
  ay_g = ay / 16384.0;
  az_g = az / 16384.0;
  gx_d = gx / 131.0;
  gy_d = gy / 131.0;
  gz_d = gz / 131.0;

  compass.read();
  mag_x = (compass.getX() - MAG_OFFSET_X) * 0.0083333;
  mag_y = (compass.getY() - MAG_OFFSET_Y) * 0.0083333;
  mag_z = (compass.getZ() - MAG_OFFSET_Z) * 0.0083333;

  imu_roll  = atan2(ay_g, az_g) * 180.0 / PI;
  imu_pitch = atan2(-ax_g, sqrt(ay_g*ay_g + az_g*az_g)) * 180.0 / PI;

  float roll_r  = imu_roll  * PI / 180.0;
  float pitch_r = imu_pitch * PI / 180.0;
  float mx_comp = mag_x*cos(pitch_r) + mag_z*sin(pitch_r);
  float my_comp = mag_x*sin(roll_r)*sin(pitch_r)
                + mag_y*cos(roll_r)
                - mag_z*sin(roll_r)*cos(pitch_r);

  imu_yaw = atan2(my_comp, mx_comp) * 180.0 / PI;
  if (imu_yaw < 0) imu_yaw += 360.0;

  ina_voltage = ina219.getBusVoltage_V();
  ina_current = ina219.getCurrent_mA();
  ina_power   = ina219.getPower_mW();
}

// ============================================
// LORA SEND
// ============================================
void sendLoRa()
{
  String payload = "{";
  payload += "\"lat\":"  + String(gps.location.lat(), 6) + ",";
  payload += "\"lng\":"  + String(gps.location.lng(), 6) + ",";
  payload += "\"galt\":" + String(gps_alt_m, 1) + ",";
  payload += "\"sat\":"  + String(gps_sats) + ",";
  payload += "\"ax\":"   + String(ax_g, 2) + ",";
  payload += "\"ay\":"   + String(ay_g, 2) + ",";
  payload += "\"az\":"   + String(az_g, 2) + ",";
  payload += "\"gx\":"   + String(gx_d, 1) + ",";
  payload += "\"gy\":"   + String(gy_d, 1) + ",";
  payload += "\"gz\":"   + String(gz_d, 1) + ",";
  payload += "\"mx\":"   + String(mag_x, 1) + ",";
  payload += "\"my\":"   + String(mag_y, 1) + ",";
  payload += "\"mz\":"   + String(mag_z, 1) + ",";
  payload += "\"t\":"    + String(bmp_temp, 1) + ",";
  payload += "\"p\":"    + String(bmp_pres, 1) + ",";
  payload += "\"balt\":" + String(bmp_alt, 1) + ",";
  payload += "\"v\":"    + String(ina_voltage, 2) + ",";
  payload += "\"c\":"    + String(ina_current, 1) + ",";
  payload += "\"pw\":"   + String(ina_power, 1) + ",";
  payload += "\"r\":"    + String(imu_roll, 1) + ",";
  payload += "\"pi\":"   + String(imu_pitch, 1) + ",";
  payload += "\"y\":"    + String(imu_yaw, 1);
  payload += "}";

  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();
}

// ============================================
// AX25
// ============================================
void encode_ax25_address(const char *call, int ssid, bool last, uint8_t *out)
{
  char buf[7] = "      ";
  for (int i = 0; call[i] && call[i] != '-' && i < 6; i++)
    buf[i] = toupper(call[i]);
  for (int i = 0; i < 6; i++) out[i] = buf[i] << 1;
  uint8_t ss = 0x60 | ((ssid & 0x0F) << 1);
  if (last) ss |= 1;
  out[6] = ss;
}

uint16_t ax25_crc(const uint8_t *data, int len)
{
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < len; i++) {
    uint8_t b = data[i];
    for (int j = 0; j < 8; j++) {
      bool bit = (crc ^ b) & 1;
      crc >>= 1;
      if (bit) crc ^= 0x8408;
      b >>= 1;
    }
  }
  return ~crc;
}

int build_aprs_frame(const char *msg)
{
  int idx = 0; uint8_t a[7];
  encode_ax25_address("APRS", 0, false, a);
  memcpy(&frame_bytes[idx], a, 7); idx += 7;
  encode_ax25_address(MY_CALLSIGN, MY_SSID, true, a);
  memcpy(&frame_bytes[idx], a, 7); idx += 7;
  frame_bytes[idx++] = 0x03;
  frame_bytes[idx++] = 0xF0;
  while (*msg) frame_bytes[idx++] = *msg++;
  uint16_t crc = ax25_crc(frame_bytes, idx);
  frame_bytes[idx++] = crc & 0xFF;
  frame_bytes[idx++] = crc >> 8;
  return idx;
}

void build_bit_tones(const char *payload)
{
  int flen = build_aprs_frame(payload);
  int data_len = 0;
  const uint8_t FLAG = 0x7E;

  for (int f = 0; f < 30; f++)
    for (int i = 0; i < 8; i++)
      data_bits[data_len++] = (FLAG >> i) & 1;

  int ones = 0;
  for (int i = 0; i < flen; i++) {
    for (int j = 0; j < 8; j++) {
      bool b = (frame_bytes[i] >> j) & 1;
      data_bits[data_len++] = b;
      if (b) {
        ones++;
        if (ones == 5) { data_bits[data_len++] = 0; ones = 0; }
      } else { ones = 0; }
    }
  }

  for (int f = 0; f < 5; f++)
    for (int i = 0; i < 8; i++)
      data_bits[data_len++] = (FLAG >> i) & 1;

  bool mark = true;
  for (int i = 0; i < data_len; i++) {
    if (!data_bits[i]) mark = !mark;
    bit_tones[i] = mark;
  }
  total_bits = data_len;
  cur_bit = 0;
  samp_in_bit = 0;
}

// ============================================
// AUDIO ISR
// ============================================
void IRAM_ATTR audioCallback(void *)
{
  if (!tx_active || cur_bit >= total_bits) {
    dac_output_voltage(DAC_APRS, 128);
    return;
  }
  phase += bit_tones[cur_bit] ? INC_MARK : INC_SPACE;
  if (phase > 2*PI) phase -= 2*PI;
  dac_output_voltage(DAC_APRS, 128 + (int)(sinf(phase)*60));
  if (++samp_in_bit >= SAMPLES_PER_BIT) {
    samp_in_bit = 0;
    cur_bit++;
    if (cur_bit >= total_bits) tx_active = false;
  }
}

// ============================================
// SEND ONE APRS PACKET
// ============================================
void send_aprs(const char *msg)
{
  build_bit_tones(msg);
  phase = 0; cur_bit = 0; samp_in_bit = 0;

  mcp.digitalWrite(MCP_CTRL_APRS, HIGH);
  mcp.digitalWrite(MCP_PTT_VHF,   LOW);
  delay(1500);

  tx_active = true;
  while (tx_active) delay(1);

  mcp.digitalWrite(MCP_PTT_VHF,   HIGH);
  mcp.digitalWrite(MCP_CTRL_APRS, LOW);
}

// ============================================
// APRS — LoRa stopped during transmission
// ============================================
void runAPRS()
{
  switchToDRA818();
  readSensors();

  int alt_feet = (int)(gps_alt_m * 3.28084);

  snprintf(info, sizeof(info), "!%s/%s-/A=%06d",
    gps_lat_str, gps_lon_str, alt_feet);
  send_aprs(info); delay(3000);

  snprintf(info, sizeof(info), ">T:%.1fC P:%.1fhPa",
    bmp_temp, bmp_pres);
  send_aprs(info); delay(3000);

  snprintf(info, sizeof(info), ">BALT:%.1fm SATS:%d",
    bmp_alt, gps_sats);
  send_aprs(info); delay(3000);

  snprintf(info, sizeof(info), ">ROLL:%.1f PITCH:%.1f",
    imu_roll, imu_pitch);
  send_aprs(info); delay(3000);

  snprintf(info, sizeof(info), ">YAW:%.1f", imu_yaw);
  send_aprs(info); delay(3000);

  snprintf(info, sizeof(info), ">V:%.2f I:%.1fmA W:%.2f",
    ina_voltage, ina_current, ina_power);
  send_aprs(info); delay(3000);

  switchToNano();

  // Return to IDLE — LoRa will resume in main loop
  currentMode = IDLE_MODE;
}

// ============================================
// SSTV — LoRa stopped during transmission
// ============================================
void runSSTV()
{
  // Stop AFSK timer
  esp_timer_stop(audio_timer);
  dac_output_voltage(DAC_APRS, 128);

  pinMode(TRIGGER_PIN, OUTPUT);
  digitalWrite(TRIGGER_PIN, LOW);

  mcp.digitalWrite(MCP_CTRL_SSTV, HIGH);
  mcp.digitalWrite(MCP_PTT_UHF,   LOW);
  delay(600);

  digitalWrite(TRIGGER_PIN, HIGH);
  delay(20);
  digitalWrite(TRIGGER_PIN, LOW);

  unsigned long t0 = millis();
  while (millis() - t0 < 130000) {
    while (SerialGPS.available())
      gps.encode(SerialGPS.read());
    delay(1000);
  }

  mcp.digitalWrite(MCP_PTT_UHF,   HIGH);
  mcp.digitalWrite(MCP_CTRL_SSTV, LOW);

  // Restart AFSK timer
  esp_timer_start_periodic(audio_timer, 1000000 / SAMPLE_RATE);

  // Return to IDLE — LoRa will resume in main loop
  currentMode = IDLE_MODE;
}

// ============================================
// SETUP
// ============================================
void setup()
{
  Serial.begin(115200);

  Wire.begin(21, 22);

  if (!mcp.begin_I2C()) { while(1); }

  mcp.pinMode(MCP_CTRL_APRS, OUTPUT);
  mcp.pinMode(MCP_PTT_VHF,   OUTPUT);
  mcp.pinMode(MCP_CTRL_SSTV, OUTPUT);
  mcp.pinMode(MCP_PTT_UHF,   OUTPUT);
  mcp.digitalWrite(MCP_CTRL_APRS, LOW);
  mcp.digitalWrite(MCP_PTT_VHF,  HIGH);
  mcp.digitalWrite(MCP_CTRL_SSTV, LOW);
  mcp.digitalWrite(MCP_PTT_UHF,  HIGH);

  dac_output_enable(DAC_APRS);
  dac_output_voltage(DAC_APRS, 128);

  pinMode(TRIGGER_PIN, OUTPUT);
  digitalWrite(TRIGGER_PIN, LOW);

  Serial2Dev.begin(9600, SERIAL_8N1, NANO_RX, NANO_TX);
  uart2Mode = UART_NANO;

  SerialGPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  mpu.initialize();
  mpu.setI2CBypassEnabled(true);
  mpu.setSleepEnabled(false);

  if (!bmp.begin())    { while(1); }
  compass.init();
  if (!ina219.begin()) { while(1); }

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) { while(1); }

  bootDRA818();
  switchToNano();

  const esp_timer_create_args_t t = {
    .callback = &audioCallback,
    .name     = "afsk"
  };
  esp_timer_create(&t, &audio_timer);
  esp_timer_start_periodic(audio_timer, 1000000 / SAMPLE_RATE);

  currentMode = IDLE_MODE;
}

// ============================================
// LOOP
// ============================================
void loop()
{
  while (SerialGPS.available())
    gps.encode(SerialGPS.read());

  checkDTMF();

  switch (currentMode) {

    // LoRa runs continuously in IDLE
    case IDLE_MODE:
      if (millis() - lastLoRaTX >= LORA_INTERVAL) {
        readSensors();
        sendLoRa();
        lastLoRaTX = millis();
      }
      break;

    // APRS — LoRa stopped, after done -> IDLE -> LoRa resumes
    case APRS_MODE:
      runAPRS();
      break;

    // SSTV — LoRa stopped, after done -> IDLE -> LoRa resumes
    case SSTV_MODE:
      runSSTV();
      break;
  }
}
