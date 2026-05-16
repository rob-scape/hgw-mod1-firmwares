/*
Hindmarsh-Rose Model - Neuron Bursting LFO
A 3D neuron model that produces bursting patterns.

The model has three variables:
- x (membrane potential): Fast spiking variable
- y (recovery): Fast inactivation
- z (adaptation): Slow variable that modulates bursting

Can be tricky because small changes of variables (pots) can lead to unexpected results, while in other settings nothing much happens.
If nothing is happening, nudge pot 3 (I) upward slowly — transitions between regimes can be abrupt.

Pots:
  A0 → r (adaptation speed / 0.001-0.01: how quickly the slow variable adapts)
  A1 → s (adaptation strength / 1-6: how strongly adaptation affects spiking)
  A2 → I (input current / 1-4: drives activity from calm to bursting)

Input:
  F1 (A3 / D17) → CV input - modulates input current

Outputs:
  D9 (F2)  → x (membrane potential - main chaotic spiky output)
  D10 (F3) → y (fast recovery - complementary rhythmic output)
  D11 (F4) → z (slow adaptation - long evolving wave)

Button (D4) → toggle normal and slow mode
LED (D3)    → Blinks on spike peaks
*/

#include <EEPROM.h>
#define EEPROM_ADDR 0

const int potAPin = A0;  // r (adaptation rate)
const int potBPin = A1;  // s (adaptation strength)
const int potIPin = A2;  // I (input current)

const int cvInputPin = A3;  // F1 CV input
const int pinF2 = 9;   // OC1A - x output
const int pinF3 = 10;  // OC1B - y output  
const int pinF4 = 11;  // OC2A - z output

const int ledPin = 3;
const int buttonPin = 4;

// Trigger state
bool lastTriggerState = LOW;

// Mode switching
bool slowMode = false;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Hindmarsh-Rose variables
float x = 0.0;   // membrane potential (fast variable)
float y = 0.0;   // recovery variable (fast variable)
float z = 0.0;   // adaptation current (slow variable)
float dt = 0.01; // time step

// Fixed parameters
const float a = 1.0;
const float b = 3.0;
const float c = 1.0;
const float d = 5.0;
const float xR = -1.6;

// LED spike detection
float lastX = 0.0;
unsigned long lastSpikeTime = 0;

void setup() {
  Serial.begin(9600);
  
  // Read saved mode from EEPROM
  slowMode = EEPROM.read(EEPROM_ADDR) == 1;
  
  pinMode(potAPin, INPUT);
  pinMode(potBPin, INPUT);
  pinMode(potIPin, INPUT);
  
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(cvInputPin, INPUT);
  
  pinMode(pinF2, OUTPUT);
  pinMode(pinF3, OUTPUT);
  pinMode(pinF4, OUTPUT);
  
  // Timer1 (16-bit) PWM Setup for F2 (D9) & F3 (D10)
  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM10);
  TCCR1B = _BV(WGM12) | _BV(CS10); // ~31.4kHz PWM
  
  // Timer2 (8-bit) PWM Setup for F4 (D11)
  TCCR2A = _BV(COM2A1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS21);
  
  OCR1A = 128;
  OCR1B = 128;
  OCR2A = 128;
  
  // Initialize to a typical starting point
  x = 0.0;
  y = 0.0;
  z = 2.0;
  
  Serial.print("Hindmarsh-Rose initialized. Slow mode: ");
  Serial.println(slowMode ? "ON" : "OFF");
}

void loop() {
  // Button handling for mode toggle
  bool buttonState = digitalRead(buttonPin);
  
  if (buttonState != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    static bool lastStableState = HIGH;
    
    if (buttonState != lastStableState) {
      lastStableState = buttonState;
      
      if (buttonState == LOW) {
        slowMode = !slowMode;
        EEPROM.write(EEPROM_ADDR, slowMode ? 1 : 0);
        Serial.print("Slow mode: ");
        Serial.println(slowMode ? "ON" : "OFF");
      }
    }
  }
  
  lastButtonState = buttonState;
  
  // F1 as CV input - modulates input current
  // Read analog value from F1 and map to offset range
  float cvOffset = mapFloat(analogRead(cvInputPin), 0, 1023, -1.0, 3.0);  // ±1 offset, -1,3 offset
  
  // Read pots and map to Hindmarsh-Rose parameters
  float r = mapFloat(analogRead(potAPin), 0, 1023, 0.006, 0.001);  // adaptation rate (inverted: CCW=slow, CW=fast)
  float s = mapFloat(analogRead(potBPin), 0, 1023, 1.0, 20.0);      // adaptation strength
  float I_base = mapFloat(analogRead(potIPin), 0, 1023, 1.0, 4.0); // input current from pot
  float I = I_base + cvOffset;  // Add CV offset from F1
  I = constrain(I, 0.5, 7.0);   // Keep within bounds
  
  // Map pot A also to time step for speed control
  int potVal = analogRead(potAPin);
  if (slowMode) {
    if (potVal < 341) dt = 0.005;
    else if (potVal < 682) dt = 0.01;
    else dt = 0.02;
  } else {
    if (potVal < 341) dt = 0.02;
    else if (potVal < 682) dt = 0.05;
    else dt = 0.1;
  }
  
  // Hindmarsh-Rose equations
  // dx/dt = y - a*x³ + b*x² - z + I
  // dy/dt = c - d*x² - y
  // dz/dt = r * (s * (x - xR) - z)
  
  float dx = y - a * x * x * x + b * x * x - z + I;
  float dy = c - d * x * x - y;
  float dz = r * (s * (x - xR) - z);
  
  // Euler integration
  x += dx * dt;
  y += dy * dt;
  z += dz * dt;
  
  // Prevent runaway (though HR is usually well-behaved)
  x = constrain(x, -3.0, 3.0);
  y = constrain(y, -20.0, 5.0);
  z = constrain(z, 0.0, 5.0);
  
  // Map to PWM range
  // x typically ranges from -2 to +2 with spikes
  int pwmX = constrain(mapFloat(x, -2.5, 2.5, 0, 255), 0, 255);
  
  // y ranges roughly -15 to +3
  int pwmY = constrain(mapFloat(y, -15, 3, 0, 255), 0, 255);
  
  // z is slow adaptation, ranges 0 to 4
  int pwmZ = constrain(mapFloat(z, 0, 4, 0, 255), 0, 255);
  
  // Output PWM signals
  OCR1A = pwmX;  // F2 - x (main chaotic spiky output)
  OCR1B = pwmY;  // F3 - y (fast recovery)
  OCR2A = pwmZ;  // F4 - z (slow adaptation wave)
  
  // LED blinks on spike peaks (when x crosses above threshold)
  if (x > 1.0 && lastX <= 1.0) {
    digitalWrite(ledPin, HIGH);
    lastSpikeTime = millis();
  }
  
  // Turn off LED after brief flash
  if (millis() - lastSpikeTime > 50) {
    digitalWrite(ledPin, LOW);
  }
  
  lastX = x;
}

// Helper function to map float values
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}