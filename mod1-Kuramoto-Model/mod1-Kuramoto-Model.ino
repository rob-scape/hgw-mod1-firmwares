/*
Kuramoto Model - Coupled Oscillator Synchronization LFO
A model of coupled phase oscillators that exhibits spontaneous synchronization.
Three oscillators with different natural frequencies try to sync - sometimes they lock,
sometimes they drift apart, creating organic evolving patterns.

This is the model behind firefly synchronization, heart pacemaker cells, and neural rhythms.

Three oscillators with phases θ1, θ2, θ3:
- Each wants to oscillate at its own natural frequency
- Coupling pulls them toward synchronization
- The battle between individuality and conformity creates emergent behavior

Pots:
  A0 → Base frequency (0.5-5 Hz: overall speed of oscillations)
  A1 → Frequency spread (0-5: how different the natural frequencies are)
  A2 → K (coupling strength / 0-3: LOW=locked sync, MID=partial sync, HIGH=independent/chaotic)

Input:
  F1 (A3 / D17) → CV input (modulates coupling strength K, -1 to +2 offset to Pot C)

Outputs:
  D9 (F2)  → Oscillator 1 (sine wave from phase θ1)
  D10 (F3) → Oscillator 2 (sine wave from phase θ2)
  D11 (F4) → Oscillator 3 (sine wave from phase θ3)

Button (D4) → Toggle between sine and square wave outputs
LED (D3)    → Blinks when oscillators are synchronized (phase coherence indicator)
*/

#include <EEPROM.h>
#define EEPROM_ADDR 0

const int potAPin = A0;  // base frequency (speed)
const int potBPin = A1;  // frequency spread
const int potCPin = A2;  // K (coupling strength)

const int cvInputPin = A3;  // F1 CV input
const int pinF2 = 9;   // OC1A - oscillator 1
const int pinF3 = 10;  // OC1B - oscillator 2
const int pinF4 = 11;  // OC2A - oscillator 3

const int ledPin = 3;
const int buttonPin = 4;

// Wave shape toggle
bool squareWave = false;  // false = sine, true = square
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Kuramoto model variables
float theta1 = 0.0;  // Phase of oscillator 1
float theta2 = 2.0;  // Phase of oscillator 2 (start offset)
float theta3 = 4.0;  // Phase of oscillator 3 (start offset)

float omega1 = 1.0;  // Natural frequency of oscillator 1
float omega2 = 1.2;  // Natural frequency of oscillator 2
float omega3 = 0.8;  // Natural frequency of oscillator 3

unsigned long lastUpdateTime = 0;

// Synchronization detection
float lastCoherence = 0.0;
unsigned long lastSyncTime = 0;

void setup() {
  Serial.begin(9600);
  
  // Read saved wave shape from EEPROM
  squareWave = EEPROM.read(EEPROM_ADDR) == 1;
  
  pinMode(potAPin, INPUT);
  pinMode(potBPin, INPUT);
  pinMode(potCPin, INPUT);
  
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
  
  lastUpdateTime = millis();
  
  Serial.print("Kuramoto Model initialized. Wave: ");
  Serial.println(squareWave ? "Square" : "Sine");
}

void loop() {
  // Button handling for wave shape toggle
  bool buttonState = digitalRead(buttonPin);
  
  if (buttonState != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    static bool lastStableState = HIGH;
    
    if (buttonState != lastStableState) {
      lastStableState = buttonState;
      
      if (buttonState == LOW) {
        squareWave = !squareWave;
        EEPROM.write(EEPROM_ADDR, squareWave ? 1 : 0);
        Serial.print("Wave shape: ");
        Serial.println(squareWave ? "Square" : "Sine");
      }
    }
  }
  
  lastButtonState = buttonState;
  
  // Calculate time step (dt in seconds)
  unsigned long currentTime = millis();
  float dt = (currentTime - lastUpdateTime) / 1000.0;
  lastUpdateTime = currentTime;
  
  // Clamp dt to prevent huge jumps
  if (dt > 0.1) dt = 0.1;
  if (dt < 0.001) dt = 0.001;
  
  // F1 as CV input - modulates coupling strength
  // TUNE HERE: CV offset range
  float cvOffset = mapFloat(analogRead(cvInputPin), 0, 1023, -1.0, 2.0);
  
  // Read pots and map to Kuramoto parameters
  // TUNE HERE: Base frequency (overall speed) - POT A
  float baseFreq = mapFloat(analogRead(potAPin), 0, 1023, 0.5, 8.0);
  
  // TUNE HERE: Frequency spread (how different the natural frequencies are) - POT B
  float spread = mapFloat(analogRead(potBPin), 0, 1023, 0.0, 9.0);
  
  // TUNE HERE: K (coupling strength) - THE KEY PARAMETER - POT C (INVERTED)
  // K=0: locked together, K>1: partial sync, K>2: independent/chaotic
  float K_base = mapFloat(analogRead(potCPin), 0, 1023, 3.0, 0.0);  // Inverted: CCW=high coupling, CW=low coupling
  float K = K_base + cvOffset;
  K = constrain(K, 0.0, 5.0);
  
  // Set natural frequencies based on spread
  // Center frequency is baseFreq, spread determines variation
  omega1 = baseFreq;
  omega2 = baseFreq + spread * 0.3;
  omega3 = baseFreq - spread * 0.2;
  
  // Kuramoto model equations:
  // dθ1/dt = ω1 + (K/N) * Σ sin(θj - θ1)
  // dθ2/dt = ω2 + (K/N) * Σ sin(θj - θ2)
  // dθ3/dt = ω3 + (K/N) * Σ sin(θj - θ3)
  // where N=3 (number of oscillators)
  
  float coupling1 = (K / 3.0) * (sin(theta2 - theta1) + sin(theta3 - theta1));
  float coupling2 = (K / 3.0) * (sin(theta1 - theta2) + sin(theta3 - theta2));
  float coupling3 = (K / 3.0) * (sin(theta1 - theta3) + sin(theta2 - theta3));
  
  float dtheta1 = omega1 + coupling1;
  float dtheta2 = omega2 + coupling2;
  float dtheta3 = omega3 + coupling3;
  
  // Update phases
  theta1 += dtheta1 * dt;
  theta2 += dtheta2 * dt;
  theta3 += dtheta3 * dt;
  
  // Keep phases in reasonable range (wrap around 2π)
  theta1 = fmod(theta1, TWO_PI);
  theta2 = fmod(theta2, TWO_PI);
  theta3 = fmod(theta3, TWO_PI);
  
  if (theta1 < 0) theta1 += TWO_PI;
  if (theta2 < 0) theta2 += TWO_PI;
  if (theta3 < 0) theta3 += TWO_PI;
  
  // Calculate order parameter (phase coherence) for LED
  // r = |Σ exp(i*θj)| / N  measures how synchronized they are
  float sumCos = cos(theta1) + cos(theta2) + cos(theta3);
  float sumSin = sin(theta1) + sin(theta2) + sin(theta3);
  float coherence = sqrt(sumCos * sumCos + sumSin * sumSin) / 3.0;
  
  // LED indicates synchronization (high coherence)
  // TUNE HERE: Coherence threshold for sync indication
  if (coherence > 0.8) {
    digitalWrite(ledPin, HIGH);
    lastSyncTime = millis();
  } else if (millis() - lastSyncTime > 100) {
    digitalWrite(ledPin, LOW);
  }
  
  // Convert phases to output values
  int pwm1, pwm2, pwm3;
  
  if (squareWave) {
    // Square wave output (comparator on phase)
    pwm1 = (theta1 < PI) ? 255 : 0;
    pwm2 = (theta2 < PI) ? 255 : 0;
    pwm3 = (theta3 < PI) ? 255 : 0;
  } else {
    // Sine wave output
    // TUNE HERE: Output amplitude/offset
    pwm1 = constrain((int)(127.5 + 127.5 * sin(theta1)), 0, 255);
    pwm2 = constrain((int)(127.5 + 127.5 * sin(theta2)), 0, 255);
    pwm3 = constrain((int)(127.5 + 127.5 * sin(theta3)), 0, 255);
  }
  
  // Output PWM signals
  OCR1A = pwm1;  // F2 - Oscillator 1
  OCR1B = pwm2;  // F3 - Oscillator 2
  OCR2A = pwm3;  // F4 - Oscillator 3
  
  lastCoherence = coherence;
}

// Helper function to map float values
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}