// ============================================================
//  AS7341 over bit-banged I2C
//  SDA = pin 10, SCL = pin 11
//
//  Changes from the original:
//   - Lines are set LOW before switching to OUTPUT (no HIGH glitch)
//   - SCL release waits for the line to actually go HIGH
//     (handles clock stretching and slow pull-ups)
//   - Startup reports WHY it failed (no ACK, stuck line, wrong ID)
//   - Startup retries every 2 s instead of halting forever
// ============================================================
#include <MicroLab.h>

const uint8_t PIN_SDA = 10;
const uint8_t PIN_SCL = 11;
const uint8_t AS7341_ADDR = 0x39;

// --- Registers ---
const uint8_t REG_ENABLE  = 0x80;
const uint8_t REG_ATIME   = 0x81;
const uint8_t REG_ID      = 0x92;
const uint8_t REG_CH0     = 0x95;
const uint8_t REG_STATUS2 = 0xA3;
const uint8_t REG_CFG1    = 0xAA;
const uint8_t REG_CFG6    = 0xAF;
const uint8_t REG_ASTEP_L = 0xCA;
const uint8_t REG_ASTEP_H = 0xCB;

uint16_t channels[12];

// ============================================================
//  LOW-LEVEL I2C
// ============================================================
inline void i2cDelay() { delayMicroseconds(5); }

inline void sdaHigh() { pinMode(PIN_SDA, INPUT_PULLUP); }
inline void sdaLow()  { digitalWrite(PIN_SDA, LOW); pinMode(PIN_SDA, OUTPUT); }
inline void sclLow()  { digitalWrite(PIN_SCL, LOW); pinMode(PIN_SCL, OUTPUT); }

// Release SCL and wait (up to ~1 ms) for it to really go HIGH
bool sclHigh() {
  pinMode(PIN_SCL, INPUT_PULLUP);
  for (uint16_t i = 0; i < 200; i++) {
    if (digitalRead(PIN_SCL)) return true;
    delayMicroseconds(5);
  }
  return false;   // SCL stuck low
}

void i2cStart() {
  sdaHigh(); sclHigh(); i2cDelay();
  sdaLow();  i2cDelay();
  sclLow();  i2cDelay();
}

void i2cStop() {
  sdaLow();  i2cDelay();
  sclHigh(); i2cDelay();
  sdaHigh(); i2cDelay();
}

bool i2cWriteByte(uint8_t b) {
  for (uint8_t i = 0; i < 8; i++) {
    if (b & 0x80) sdaHigh(); else sdaLow();
    i2cDelay();
    sclHigh(); i2cDelay();
    sclLow();  i2cDelay();
    b <<= 1;
  }
  sdaHigh(); i2cDelay();               // release SDA for ACK
  sclHigh(); i2cDelay();
  bool ack = (digitalRead(PIN_SDA) == LOW);
  sclLow(); i2cDelay();
  return ack;
}

uint8_t i2cReadByte(bool ackIt) {
  uint8_t b = 0;
  sdaHigh();
  for (uint8_t i = 0; i < 8; i++) {
    b <<= 1;
    sclHigh(); i2cDelay();
    if (digitalRead(PIN_SDA)) b |= 1;
    sclLow();  i2cDelay();
  }
  if (ackIt) sdaLow(); else sdaHigh();
  i2cDelay();
  sclHigh(); i2cDelay();
  sclLow();  i2cDelay();
  sdaHigh();
  return b;
}

// Clock out up to 9 pulses to free a slave stuck mid-byte
void i2cBusRecover() {
  sdaHigh();
  for (uint8_t i = 0; i < 9 && !digitalRead(PIN_SDA); i++) {
    sclLow(); i2cDelay();
    sclHigh(); i2cDelay();
  }
  i2cStop();
}

// ============================================================
//  REGISTER ACCESS
// ============================================================
bool regWrite(uint8_t reg, uint8_t val) {
  i2cStart();
  bool ok = i2cWriteByte(AS7341_ADDR << 1) &&
            i2cWriteByte(reg) &&
            i2cWriteByte(val);
  i2cStop();
  return ok;
}

bool regReadN(uint8_t reg, uint8_t *buf, uint8_t n) {
  i2cStart();
  if (!i2cWriteByte(AS7341_ADDR << 1)) { i2cStop(); return false; }
  if (!i2cWriteByte(reg))              { i2cStop(); return false; }
  i2cStart();                                         // repeated start
  if (!i2cWriteByte((AS7341_ADDR << 1) | 1)) { i2cStop(); return false; }
  for (uint8_t i = 0; i < n; i++) {
    buf[i] = i2cReadByte(i < (n - 1));                // NACK the last byte
  }
  i2cStop();
  return true;
}

bool regRead(uint8_t reg, uint8_t &val) {
  return regReadN(reg, &val, 1);
}

// ============================================================
//  SMUX CONFIGURATION
//  11 photodiodes, 6 ADCs -> two passes with remapping between
// ============================================================
bool writeSmux(const uint8_t *m) {
  for (uint8_t i = 0; i < 20; i++)
    if (!regWrite(i, m[i])) return false;
  return true;
}

bool smuxLowChannels() {   // F1-F4, Clear, NIR
  static const uint8_t m[20] = {
    0x30,0x01,0x00,0x00,0x00,0x42,0x00,0x00,0x50,0x00,
    0x00,0x00,0x20,0x04,0x00,0x30,0x01,0x50,0x00,0x06
  };
  return writeSmux(m);
}

bool smuxHighChannels() {  // F5-F8, Clear, NIR
  static const uint8_t m[20] = {
    0x00,0x00,0x00,0x40,0x02,0x00,0x10,0x03,0x50,0x10,
    0x03,0x00,0x00,0x00,0x24,0x00,0x00,0x50,0x00,0x06
  };
  return writeSmux(m);
}

// ============================================================
//  CONTROL HELPERS
// ============================================================
bool enableBits(uint8_t mask, bool on) {
  uint8_t e;
  if (!regRead(REG_ENABLE, e)) return false;
  if (on) e |= mask; else e &= ~mask;
  return regWrite(REG_ENABLE, e);
}

bool powerOn(bool on)        { return enableBits(0x01, on); }   // PON
bool spectralEnable(bool on) { return enableBits(0x02, on); }   // SP_EN

bool applySmux() {
  if (!regWrite(REG_CFG6, 0x10)) return false;   // SMUX command = write
  if (!enableBits(0x10, true))   return false;   // SMUXEN: start load

  unsigned long t0 = millis();
  uint8_t e;
  do {                                           // wait for SMUXEN to clear
    if (!regRead(REG_ENABLE, e)) return false;
    if (millis() - t0 > 100)     return false;
  } while (e & 0x10);
  return true;
}

bool waitForData(unsigned long timeoutMs) {
  unsigned long t0 = millis();
  uint8_t s;
  while (true) {
    if (!regRead(REG_STATUS2, s)) return false;
    if (s & 0x40) return true;                   // AVALID
    if (millis() - t0 > timeoutMs) return false;
    delay(1);
  }
}

// ============================================================
//  SETUP + READ
// ============================================================
bool as7341Begin() {
  sdaHigh(); sclHigh();
  delay(10);

  // Check the bus lines are idle HIGH
  int sda = digitalRead(PIN_SDA), scl = digitalRead(PIN_SCL);
  if (!sda || !scl) {
    MicroLab.serial.print("  Bus line stuck LOW (SDA=");
    MicroLab.serial.print(sda); MicroLab.serial.print(" SCL=");
    MicroLab.serial.print(scl); MicroLab.serial.println(") - trying recovery");
    i2cBusRecover();
    if (!digitalRead(PIN_SDA) || !digitalRead(PIN_SCL)) {
      MicroLab.serial.println("  Still stuck: check wiring, pull-ups, shorts");
      return false;
    }
  }

  uint8_t id;
  if (!regRead(REG_ID, id)) {
    MicroLab.serial.println("  No ACK from 0x39: check power, GND, SDA/SCL swap, pull-ups");
    return false;
  }
  MicroLab.serial.print("  ID register = 0x");
  MicroLab.serial.println(id, HEX);
  if ((id & 0xFC) != 0x24) {
    MicroLab.serial.println("  Unexpected ID: not an AS7341");
    return false;
  }

  if (!powerOn(true)) return false;
  delay(10);

  // Integration time = (ATIME+1) * (ASTEP+1) * 2.78 us ~= 281 ms
  bool ok = regWrite(REG_ATIME, 100) &&
            regWrite(REG_ASTEP_L, 999 & 0xFF) &&
            regWrite(REG_ASTEP_H, (999 >> 8) & 0xFF) &&
            regWrite(REG_CFG1, 9);                // gain 256x
  if (!ok) MicroLab.serial.println("  Config write failed");
  return ok;
}

bool readPass(bool high, uint8_t offset) {
  uint8_t buf[12];
  if (!spectralEnable(false)) return false;
  if (!(high ? smuxHighChannels() : smuxLowChannels())) return false;
  if (!applySmux()) return false;
  if (!spectralEnable(true)) return false;
  if (!waitForData(1000)) return false;
  if (!regReadN(REG_CH0, buf, 12)) return false;
  for (uint8_t i = 0; i < 6; i++)
    channels[offset + i] = buf[i * 2] | ((uint16_t)buf[i * 2 + 1] << 8);
  return true;
}

bool readAllChannels() {
  return readPass(false, 0) &&   // F1-F4, Clear, NIR -> channels[0..5]
         readPass(true, 6);      // F5-F8, Clear, NIR -> channels[6..11]
}

// ============================================================
//  TEST SKETCH
// ============================================================
void setup() {
  MicroLab.serial.begin(9600);
  delay(2000);                    // give the USB link / monitor time to connect
  MicroLab.serial.println("Booting...");

  while (true) {
    MicroLab.serial.println("Looking for AS7341...");
    if (as7341Begin()) break;
    MicroLab.serial.println("AS7341 not found - retrying in 2 s");
    delay(2000);
  }
  MicroLab.serial.println("AS7341 ready");
}

void loop() {
  if (!readAllChannels()) {
    MicroLab.serial.println("Read failed");
    i2cBusRecover();
    delay(1000);
    return;
  }

  MicroLab.serial.print("415 ");      MicroLab.serial.print(channels[0]);
  MicroLab.serial.print(" | 445 ");   MicroLab.serial.print(channels[1]);
  MicroLab.serial.print(" | 480 ");   MicroLab.serial.print(channels[2]);
  MicroLab.serial.print(" | 515 ");   MicroLab.serial.print(channels[3]);
  MicroLab.serial.print(" | 555 ");   MicroLab.serial.print(channels[6]);
  MicroLab.serial.print(" | 590 ");   MicroLab.serial.print(channels[7]);
  MicroLab.serial.print(" | 630 ");   MicroLab.serial.print(channels[8]);
  MicroLab.serial.print(" | 680 ");   MicroLab.serial.print(channels[9]);
  MicroLab.serial.print(" | Clear "); MicroLab.serial.print(channels[10]);
  MicroLab.serial.print(" | NIR ");   MicroLab.serial.println(channels[11]);

  delay(1000);
}
