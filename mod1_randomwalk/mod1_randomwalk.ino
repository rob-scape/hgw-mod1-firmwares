/*
Random Walk with Gravity Mode for hagiwos Mod1 module by Rob Heel. 
Button toggles between classic Random Walk mode and Gravity Mode.

Hardware Configuration:
- Potentiometer 1 (Rate)    → A0
- Potentiometer 2 (Bias/Offset)  → A1
- Potentiometer 3 (ChaosDepth) → A2

- LED Indicator          → Pin 3 (OCR2B)
- Push Button  → Pin 4

Inputs/Outputs:
- F1    A3  CV input (adds to Rate)
- F2    A4  in/output
- F3    A5  CV input (adds to ChaosDepth)
- F4    D11 Random Walk Output

Ultra Slow Random walk CV module code
- Locked to ultra-slow frequency range for gentle random walks
- Random Walk output remains independent

Behavior:
- Pot1 controls frequency (how often new random steps occur), F1 CV input affects frequency too.
- Pot2 (Bias/Offset): Controls an offset applied to the walk output, shifting it up or down.
- Pot3 (ChaosDepth): Controls how large each random step can be. F3 CV input affects ChaosDepth.
  Low values make the walk drift subtly, high values allow bigger, wilder jumps between steps.

- Button toggles between classic Random Walk mode and Gravity Mode.
  Gravity Mode pulls the output slowly back to 0 over time.
*/

#include <EEPROM.h>
#include <Arduino.h>

// ---------------- Global Variables and Constants ----------------
static const unsigned int TABLE_SIZE = 1024;
static const unsigned long UPDATE_INTERVAL_US = 400; // ~2500Hz LFO update rate

uint8_t waveTable[TABLE_SIZE];

// Phase and value tracking
float walkPhase = 0.0;

// Chaos parameters
float chaosDepth = 0.0;
float rate = 0.001f; // Default ultra slow rate
float bias = 0.0f; // Bias offset

// Mode toggle
bool gravityMode = false; // Default to classic random walk

void setup() {
  // Set up pins
  pinMode(11, OUTPUT);    // Random Walk Output
  pinMode(3, OUTPUT);     // LED Indicator
  pinMode(4, INPUT_PULLUP); // Button input

  configurePWM();
}

void loop() {
  // Read potentiometer values and CV inputs
  rate = readFrequency(A0) + (analogRead(A3) / 1023.0f * 0.1f); // Rate modulated by F1 CV input
  chaosDepth = (analogRead(A2) / 1023.0f) + (analogRead(A5) / 1023.0f); // ChaosDepth modulated by F3 CV input
  chaosDepth = constrain(chaosDepth, 0.0f, 1.0f); // Ensure chaos stays within 0-1

  bias = (analogRead(A1) / 1023.0f) * 0.8f - 0.4f; // Pot2 as bias control (-0.4 to +0.4 offset)

  // Check for button press to toggle mode
  if (digitalRead(4) == LOW) {
    gravityMode = !gravityMode;
    delay(200); // Basic debounce
  }

  // Random walk update
  if (gravityMode) {
    updateGravityWalk(walkPhase, rate, chaosDepth);
  } else {
    updateRandomWalk(walkPhase, rate, chaosDepth);
  }

  // Apply bias to the output value
  int walkStepVal = (int)((walkPhase + bias) * 255.0f); // Renamed for clarity
  walkStepVal = constrain(walkStepVal, 0, 255); // Ensure output stays within valid range

  analogWrite(11, walkStepVal); // Random Walk output
  OCR2B = walkStepVal; // LED brightness reflects output
}

// Set up PWM registers
void configurePWM() {
  TCCR1A = 0; TCCR1B = 0;
  TCCR1A |= (1 << WGM10) | (1 << COM1A1) | (1 << COM1B1);
  TCCR1B |= (1 << WGM12) | (1 << CS10);

  TCCR2A = 0; TCCR2B = 0;
  TCCR2A |= (1 << WGM20) | (1 << WGM21) | (1 << COM2A1) | (1 << COM2B1);
  TCCR2B |= (1 << CS20);
}

// Classic random walk behavior for F4 output
void updateRandomWalk(float &phase, float rate, float depth) {
  float randomStep = (random(-100, 100) / 100.0f) * depth;
  phase += randomStep * rate;

  if (phase < 0.0f) phase = 0.0f;
  if (phase > 1.0f) phase = 1.0f;
}

// Gravity mode — random walk that slowly falls back to 0
void updateGravityWalk(float &phase, float rate, float depth) {
  float randomStep = (random(-100, 100) / 100.0f) * depth;
  phase += randomStep * rate;

  // Introduce gravity pull towards 0
  phase *= 0.99f; // 0.995f → Gentle pull (slow decay) 0.98f → Medium pull (faster drift to zero) 0.9f → Strong pull (snaps back quickly)

  if (phase < 0.0f) phase = 0.0f;
  if (phase > 1.0f) phase = 1.0f;
}

// Read frequency from Pot1 (A0)
float readFrequency(int analogPin) {
  int rawVal = analogRead(analogPin);
  float fMin = 0.001f;  // Locked to ultra slow mode
  float fMax = 0.1f;
  return fMin * powf(fMax / fMin, rawVal / 1023.0f);  // Exponential scaling
}
