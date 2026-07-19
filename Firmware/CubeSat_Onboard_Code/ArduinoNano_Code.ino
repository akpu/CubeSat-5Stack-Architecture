#include <SoftwareSerial.h>

#define Q1  4
#define Q2  5
#define Q3  6
#define Q4  7
#define EST 8

SoftwareSerial espSerial(9, 10);

int lastDTMF = -1;

void sendAT(const char *cmd, uint16_t waitMs = 1000)
{
  Serial.print(cmd);
  Serial.print("\r\n");
  unsigned long t0 = millis();
  while (millis() - t0 < waitMs)
    if (Serial.available()) Serial.read();
}

void configSA868_SSTV()
{
  sendAT("AT+DMOCONNECT");
  sendAT("AT+DMOSETGROUP=0,433.0000,433.0000,0000,4,0000");
  sendAT("AT+SETFILTER=0,0,0");
}

void setup()
{
  Serial.begin(9600);
  espSerial.begin(9600);

  pinMode(Q1,  INPUT);
  pinMode(Q2,  INPUT);
  pinMode(Q3,  INPUT);
  pinMode(Q4,  INPUT);
  pinMode(EST, INPUT);

  delay(1500);
  espSerial.println("CMD:1");
}

void loop()
{
  if (digitalRead(EST) == HIGH) {

    int dtmf =
      (digitalRead(Q4) << 3) |
      (digitalRead(Q3) << 2) |
      (digitalRead(Q2) << 1) |
       digitalRead(Q1);

    if (dtmf != lastDTMF) {

      if (dtmf == 3) {
        configSA868_SSTV();
        delay(300);
      }

      espSerial.print("CMD:");
      espSerial.println(dtmf);

      lastDTMF = dtmf;
      delay(300);
    }

  } else {
    lastDTMF = -1;
  }
}
