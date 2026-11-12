#include <Wire.h>
#include <MicroLab.h>

const uint8_t SHT30_ADDR = 0x44;

float tempC = NAN;
float humidity = NAN;

void setup() {
MicroLab.beginDebugSerial(9600);

delay(1000);

MicroLab.serial.println("SHT30 test starting");

// GPIO 2 and 3 belong to I2C1
Wire1.setSDA(2);
Wire1.setSCL(3);
Wire1.begin();

MicroLab.serial.println("I2C1 initialized");
}

void loop() {
if (sht30Read()) {
MicroLab.serial.print("Temp: ");
MicroLab.serial.print(tempC, 2);
MicroLab.serial.print(" C | RH: ");
MicroLab.serial.print(humidity, 1);
MicroLab.serial.println(" %");
} else {
MicroLab.serial.println("SHT30 read failed");
}

delay(2000);
}

bool sht30Read() {
Wire1.beginTransmission(SHT30_ADDR);

Wire1.write(0x24);
Wire1.write(0x00);

uint8_t error = Wire1.endTransmission();

if (error != 0) {
MicroLab.serial.print("I2C transmission error: ");
MicroLab.serial.println(error);
return false;
}

delay(20);

uint8_t received = Wire1.requestFrom(
(uint8_t)SHT30_ADDR,
(uint8_t)6
);

if (received != 6) {
MicroLab.serial.print("Bytes received: ");
MicroLab.serial.println(received);
return false;
}

uint8_t data[6];

for (uint8_t i = 0; i < 6; i++) {
data[i] = Wire1.read();
}

uint16_t rawTemp = ((uint16_t)data[0] << 8) | data[1];
uint16_t rawHum = ((uint16_t)data[3] << 8) | data[4];

tempC = -45.0 + 175.0 * ((float)rawTemp / 65535.0);
humidity = 100.0 * ((float)rawHum / 65535.0);

return true;
}