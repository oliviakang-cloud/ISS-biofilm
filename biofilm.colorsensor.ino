// ============================================================
//  AS7341 over bit-banged I2C
//  SDA = pin 10, SCL = pin 11
// ============================================================

const uint8_t PIN_SDA = 10;
const uint8_t PIN_SCL = 11;
const uint8_t AS7341_ADDR = 0x39;

// --- Registers ---
const uint8_t REG_ENABLE  = 0x80;
const uint8_t REG_ATIME   = 0x81;
const uint8_t REG_STATUS2 = 0xA3;
const uint8_t REG_CFG1    = 0xAA;
const uint8_t REG_CFG6    = 0xAF;
const uint8_t REG_ASTEP_L = 0xCA;
const uint8_t REG_ASTEP_H = 0xCB;
const uint8_t REG_CH0     = 0x95;
const uint8_t REG_ID      = 0x92;

uint16_t channels[12];

// ============================================================
//  LOW-LEVEL I2C
// ============================================================
inline void sdaHigh() { pinMode(PIN_SDA, INPUT_PULLUP); }
inline void sdaLow()  { pinMode(PIN_SDA, OUTPUT); digitalWrite(PIN_SDA, LOW); }
inline void sclHigh() { pinMode(PIN_SCL, INPUT_PULLUP); }
inline void sclLow()  { pinMode(PIN_SCL, OUTPUT); digitalWrite(PIN_SCL, LOW); }
inline void i2cDelay(){ delayMicroseconds(5); }

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
  sdaHigh(); i2cDelay();
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

// ============================================================
//  REGISTER ACCESS
// ============================================================
bool regWrite(uint8_t reg, uint8_t val) {
  i2cStart();
  if (!i2cWriteByte(AS7341_ADDR << 1)) { i2cStop(); return false; }
  if (!i2cWriteByte(reg))              { i2cStop(); return false; }
  if (!i2cWriteByte(val))              { i2cStop(); return false; }
  i2cStop();
  return true;
}

bool regReadN(uint8_t reg, uint8_t *buf, uint8_t n) {
  i2cStart();
  if (!i2cWriteByte(AS7341_ADDR << 1)) { i2cStop(); return false; }
  if (!i2cWriteByte(reg))              { i2cStop(); return false; }
  i2cStart();                                   // repeated start
  if (!i2cWriteByte((AS7341_ADDR << 1) | 1)) { i2cStop(); return false; }
  for (uint8_t i = 0; i < n; i++) {
    buf[i] = i2cReadByte(i < (n - 1));          // NACK the last byte
  }
  i2cStop();
  return true;
}

uint8_t regRead(uint8_t reg) {
  uint8_t v = 0;
  regReadN(reg, &v, 1);
  return v;
}

// ============================================================
//  SMUX CONFIGURATION
//  The AS7341 has 11 photodiodes but only 6 ADCs, so a reading
//  happens in two passes with the diodes remapped in between.
// ============================================================
void smuxLowChannels() {   // F1-F4, Clear, NIR
  const uint8_t m[20] = {
    0x30,0x01,0x00,0x00,0x00,0x42,0x00,0x00,0x50,0x00,
    0x00,0x00,0x20,0x04,0x00,0x30,0x01,0x50,0x00,0x06
  };
  for (uint8_t i = 0; i < 20; i++) regWrite(i, m[i]);
}

void smuxHighChannels() {  // F5-F8, Clear, NIR
  const uint8_t m[20] = {
    0x00,0x00,0x00,0x40,0x02,0x00,0x10,0x03,0x50,0x10,
    0x03,0x00,0x00,0x00,0x24,0x00,0x00,0x50,0x00,0x06
  };
  for (uint8_t i = 0; i < 20; i++) regWrite(i, m[i]);
}

// ============================================================
//  CONTROL HELPERS
// ============================================================
void powerOn(bool on) {
  uint8_t e = regRead(REG_ENABLE);
  if (on) e |= 0x01; else e &= ~0x01;
  regWrite(REG_ENABLE, e);
}

void spectralEnable(bool on) {
  uint8_t e = regRead(REG_ENABLE);
  if (on) e |= 0x02; else e &= ~0x02;
  regWrite(REG_ENABLE, e);
}

bool applySmux() {
  regWrite(REG_CFG6, 0x10);              // SMUX command = write
  uint8_t e = regRead(REG_ENABLE);
  regWrite(REG_ENABLE, e | 0x10);        // start SMUX load

  unsigned long t0 = millis();
  while (regRead(REG_ENABLE) & 0x10) {   // wait for it to clear
    if (millis() - t0 > 100) return false;
  }
  return true;
}

bool waitForData(unsigned long timeoutMs) {
  unsigned long t0 = millis();
  while (!(regRead(REG_STATUS2) & 0x40)) {   // AVALID
    if (millis() - t0 > timeoutMs) return false;
    delay(1);
  }
  return true;
}

// ============================================================
//  SETUP + READ
// ============================================================
bool as7341Begin() {
  sdaHigh(); sclHigh();
  delay(10);

  uint8_t id = regRead(REG_ID);
  if ((id & 0xFC) != 0x24) return false;   // wrong/absent device

  powerOn(true);
  delay(10);

  regWrite(REG_ATIME, 100);
  regWrite(REG_ASTEP_L, 999 & 0xFF);
  regWrite(REG_ASTEP_H, (999 >> 8) & 0xFF);
  regWrite(REG_CFG1, 9);                   // gain 256x
  return true;
}

bool readAllChannels() {
  uint8_t buf[12];

  // --- Pass 1: F1-F4 ---
  spectralEnable(false);
  smuxLowChannels();
  if (!applySmux()) return false;
  spectralEnable(true);
  if (!waitForData(1000)) return false;
  if (!regReadN(REG_CH0, buf, 12)) return false;
  for (uint8_t i = 0; i < 6; i++)
    channels[i] = buf[i*2] | (buf[i*2+1] << 8);

  // --- Pass 2: F5-F8 ---
  spectralEnable(false);
  smuxHighChannels();
  if (!applySmux()) return false;
  spectralEnable(true);
  if (!waitForData(1000)) return false;
  if (!regReadN(REG_CH0, buf, 12)) return false;
  for (uint8_t i = 0; i < 6; i++)
    channels[i+6] = buf[i*2] | (buf[i*2+1] << 8);

  return true;
}

// ============================================================
//  TEST SKETCH
// ============================================================
void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }

  if (!as7341Begin()) {
    Serial.println("AS7341 not found");
    while (1) delay(10);
  }
  Serial.println("AS7341 ready");
}

void loop() {
  if (!readAllChannels()) {
    Serial.println("Read failed");
    delay(1000);
    return;
  }

  Serial.print("415 "); Serial.print(channels[0]);
  Serial.print(" | 445 "); Serial.print(channels[1]);
  Serial.print(" | 480 "); Serial.print(channels[2]);
  Serial.print(" | 515 "); Serial.print(channels[3]);
  Serial.print(" | 555 "); Serial.print(channels[6]);
  Serial.print(" | 590 "); Serial.print(channels[7]);
  Serial.print(" | 630 "); Serial.print(channels[8]);
  Serial.print(" | 680 "); Serial.print(channels[9]);
  Serial.print(" | Clear "); Serial.print(channels[10]);
  Serial.print(" | NIR "); Serial.println(channels[11]);

  delay(1000);
}