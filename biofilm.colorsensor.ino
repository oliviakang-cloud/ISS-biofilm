#include <Adafruit_AS7341.h>

Adafruit_AS7341 as7341;

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }

  if (!as7341.begin()) {
    Serial.println("AS7341 not found - check wiring");
    while (1) { delay(10); }
  }

  // Integration time = (ATIME+1) * (ASTEP+1) * 2.78us
  // 100 * 999 -> ~278 ms per reading
  as7341.setATIME(100);
  as7341.setASTEP(999);
  as7341.setGain(AS7341_GAIN_256X);

  Serial.println("AS7341 ready");
}

void loop() {
  uint16_t readings[12];

  if (!as7341.readAllChannels(readings)) {
    Serial.println("Read failed");
    delay(1000);
    return;
  }

  Serial.print("415nm "); Serial.print(readings[0]);
  Serial.print(" | 445nm "); Serial.print(readings[1]);
  Serial.print(" | 480nm "); Serial.print(readings[2]);
  Serial.print(" | 515nm "); Serial.print(readings[3]);
  Serial.print(" | 555nm "); Serial.print(readings[6]);
  Serial.print(" | 590nm "); Serial.print(readings[7]);
  Serial.print(" | 630nm "); Serial.print(readings[8]);
  Serial.print(" | 680nm "); Serial.print(readings[9]);
  Serial.print(" | Clear "); Serial.print(readings[10]);
  Serial.print(" | NIR ");   Serial.println(readings[11]);

  delay(1000);
}