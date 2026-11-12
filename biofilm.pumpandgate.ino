// Pins
const uint8_t PIN_EN  = 4;   // enable
const uint8_t PIN_DIR = 5;   // direction
const uint8_t PIN_STP = 6;   // step
const uint8_t PIN_RST = 7;   // reset
const uint8_t PIN_M2  = 26;  // m1
const uint8_t PIN_M1  = 28;  // m2

// const uint8_t PIN_VALVE = 8; // gate/valve  NOT THERE YET

// Timing
const unsigned int STEP_PULSE_US    = 10;    // pulse width
const unsigned int STEP_INTERVAL_US = 1200;  // speed

// Direction
const uint8_t DIR_DISPENSE = HIGH;
const uint8_t DIR_RETRACT  = LOW;

void setup() {
  pinMode(PIN_EN,  OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_STP, OUTPUT);
  pinMode(PIN_RST, OUTPUT);
  pinMode(PIN_M1,  OUTPUT);
  pinMode(PIN_M2,  OUTPUT);
  // pinMode(PIN_VALVE, OUTPUT);

  digitalWrite(PIN_M1, LOW);    // full step
  digitalWrite(PIN_M2, LOW);
  digitalWrite(PIN_RST, HIGH);  // driver awake
  digitalWrite(PIN_EN, HIGH);   // motor off for now
  // digitalWrite(PIN_VALVE, LOW); // valve closed

  delay(2000);
  //fireGate(200, DIR_DISPENSE);  // open valve, pump 200 steps, close
  moveSteps(200, DIR_DISPENSE);
  
}

void loop() { 
  moveSteps(200, DIR_DISPENSE);
}
/*
void fireGate(long steps, uint8_t direction) {
  digitalWrite(PIN_VALVE, HIGH);   // open valve
  delay(50);

  moveSteps(steps, direction);     // pump

  delay(50);
  digitalWrite(PIN_VALVE, LOW);    // close valve
}
*/

// pump control
void moveSteps(long steps, uint8_t direction) {
  if (steps <= 0) return;

  digitalWrite(PIN_DIR, direction);
  delayMicroseconds(5);

  //digitalWrite(PIN_EN, LOW);   // motor on
  delay(5);

  for (long i = 0; i < steps; i++) {
    digitalWrite(PIN_STP, HIGH);
    delayMicroseconds(STEP_PULSE_US);
    digitalWrite(PIN_STP, LOW);
    delayMicroseconds(STEP_INTERVAL_US);
  }

  digitalWrite(PIN_EN, HIGH);  // motor off
}