/*
Pot1 recorder firmware for Mod1 eurorack module designed by Hagiwo by Rob Heel. 
Pot1 Recorder with Fast PWM Output (100 Hz S&H style) max recording time 7.5 sec. 
You can record movement of pot1 while holding down the button. Once let go it will loop your recording. 
Pot2 is speed, from half to double speed.  Pot3 is gain control.
Speed CV offset via F1 CV in. Gain CV offset via F2 CV in. CV out on F4. 
*/


#include <math.h> 

#define POT1 A0  // Pot to be recorded
#define POT2 A1  // Playback speed control
#define POT3 A2  // Gain
#define F1 A3    // Speed modifier (Voltage control input)
#define F2 A4    // Gain CV
#define BUTTON 4 // Recording button
#define LED 3    // Output Led
#define F4 11    // CV out

#define TABLE_SIZE 768  // Max buffer (~75% of Nano SRAM)
#define INTERVAL 10     // ms between samples (100 Hz)

int values[TABLE_SIZE];
int index = 0;
int recordedLength = 0;

bool recording = false;
bool playback = false;
bool waitForButtonRelease = false;
bool hasRecordedYet = false;

unsigned long lastMillis = 0;

void setup() {
  pinMode(BUTTON, INPUT_PULLUP);  // Active LOW
  pinMode(LED, OUTPUT);
  pinMode(F4, OUTPUT);

  // Fast PWM on Pin 11 (OC2A)
  TCCR2A = _BV(COM2A1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS21);  // Prescaler 8 = ~7.8kHz PWM
  OCR2A = 0;           // Start with 0% duty
}

void loop() {
  bool buttonPressed = (digitalRead(BUTTON) == LOW);
  unsigned long now = millis();

  // === Handle Preview Mode ===
  if (!recording && !playback) {
    int previewVal = analogRead(POT1) / 4;
    OCR2A = previewVal;            // Route Pot1 to output directly (F4)
    analogWrite(LED, previewVal);  // LED mirrors preview signal
  }

  // === Handle Recording ===
  if (recording && index < TABLE_SIZE) {
    if (now - lastMillis >= INTERVAL) {
      lastMillis = now;

      int value = analogRead(POT1) / 4;  // Scale 0–1023 to 0–255
      values[index] = value;
      OCR2A = value;            // Live output while recording
      analogWrite(LED, value);  // LED mirrors signal
      recordedLength = index + 1;
      index++;
    }
  }

  // === Stop recording if full ===
  if (recording && index >= TABLE_SIZE) {
    recording = false;
    playback = true;
    index = 0;
    waitForButtonRelease = true;
    lastMillis = now;
  }

  // === Start recording (on button press)
  if (buttonPressed && !recording && !waitForButtonRelease) {
    recording = true;
    playback = false;
    index = 0;
    recordedLength = 0;
    lastMillis = now;
  }

  // === If button released, allow new recordings again
  if (!buttonPressed && waitForButtonRelease) {
    waitForButtonRelease = false;
  }

  // === Stop recording if button is released early
  if (!buttonPressed && recording) {
    recording = false;
    playback = true;
    index = 0;
    lastMillis = now;
  }

  // === Playback loop
  if (playback) {
    // Get speed from POT2
    int baseInterval = map(analogRead(POT2), 0, 1023, 20, 5);  // Playback speed range 20ms to 5ms
    // Get speed modifier from F1 (scaled from 0 to 5V input, mapped to 1.0 to 3.0 multiplier)
    int modOffset = map(analogRead(F1), 0, 1023, 0, 15);  // F1: adds speed (shorter interval)

    // Calculate final interval by multiplying base speed with the speed modifier
    int dynamicInterval = baseInterval - modOffset;
    dynamicInterval = max(dynamicInterval, 1);  // clamp to safe minimum (avoid 0 or negative)

    if (now - lastMillis >= dynamicInterval) {
      lastMillis = now;

      // Read Pot3 and F2 to calculate gain
      int gainPot = analogRead(POT3);  // 0 - 1023
      int gainCV = analogRead(F2);     // 0 - 1023
      // Convert both to float multipliers between 0.0 - 1.0
      float potGain = gainPot / 1023.0;
      float cvGain = gainCV / 1023.0;

      float totalGain = potGain + cvGain;          // Final gain multiplier (0.0 - 1.0)
      totalGain = constrain(totalGain, 0.0, 1.0);  // Ensure gain is within 0.0 - 1.0

      // Apply gain during playback output
      int outputValue = values[index];

      float scaledOutput = outputValue * totalGain;
      scaledOutput = constrain(scaledOutput, 0.0, 255.0);  // Constrain to 0-255 range
      OCR2A = (int)round(scaledOutput);                           // Convert back to int for the PWM output

      /*
      int scaledOutput = outputValue * totalGain;
      scaledOutput = constrain(scaledOutput, 0, 255);
      OCR2A = scaledOutput;
      */
      analogWrite(LED, scaledOutput);  // LED follows output

      index = (index + 1) % recordedLength;
    }
  }
}
