/*
  Turing Machine / KLEE-style Sequencer (array version)
  Description:
    - F1 (A3/D17): Trigger input (rising edge detection)
    - F2: CV in offset for pot1
    - F3 : Quantized CV output via fast PWM (0–5V)
    - F4 (D11): CV output via fast PWM (0–5V)
    - Pot1 (A0): Randomness vs lock (sequencing knob)
    - Pot2 (A1): Loop length selector (2,3,4,5,6,8,12,16)
    - Pot3 (A2): Gain control (output scaling 0–5V)
*/

#include "scales.h"
#define BUTTON_PIN 4

typedef int (*QuantizeFunction)(int);
QuantizeFunction quantizers[] = {
  quantizeToMajor,
  quantizeToMinor,
  quantizeToPhrygian
};

#define NUM_SCALES (sizeof(quantizers) / sizeof(quantizers[0]))  // define numbers of scales in scales.h

enum ScaleType {
  MAJOR,
  MINOR,
  PHRYGIAN
};


int currentScaleIndex = 0;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;
bool lastButtonState = HIGH;

const int triggerPin = 17;  // F1 (A3 / D17)
const int cvOutPin = 11;    // F4 (D11)
const int ledPin = 3;       // LED for CV feedback

const int pot1Pin = A0;  // Randomness vs lock
const int pot2Pin = A1;  // Loop length
const int pot3Pin = A2;  // Gain

bool shiftRegister[16] = { 0 };  // 16-step binary sequence

const int loopOptions[] = { 2, 3, 4, 5, 6, 8, 12, 16 };
const int numLoopOptions = sizeof(loopOptions) / sizeof(loopOptions[0]);

bool prevClockState = LOW;

void setup() {
  Serial.begin(9600);
  pinMode(triggerPin, INPUT);
  pinMode(cvOutPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  setupFastPWM();
  analogWriteFast(0);
  analogWrite(ledPin, 0);
  randomSeed(analogRead(A7));  // Safe analog pin
}

void loop() {
  checkScaleButton();
  bool currClockState = digitalRead(triggerPin);
  if (currClockState == HIGH && prevClockState == LOW) {
    stepSequencer();
  }
  prevClockState = currClockState;
}

void stepSequencer() {
  // Read pots
  int pot1Hardware = analogRead(pot1Pin);
     Serial.print("POT1: ");
     Serial.print(pot1Hardware);
  int pot1cvInput = analogRead(A4);
  pot1cvInput = map(pot1cvInput, 0, 900, 0, 1023);  // remap for full range behavior

    Serial.print(" | CV IN: ");
    Serial.print(pot1cvInput);
  int pot1 = pot1Hardware + pot1cvInput;  // sum pot1 and CV in F2
  pot1 = constrain(pot1, 0, 1023);        // Make sure we stay in bounds
    Serial.print(" | : ");
    Serial.print(pot1);

  int pot2 = analogRead(pot2Pin);  // loop length
  int pot3 = analogRead(pot3Pin);  // gain

  // Map pot2 to loop length
  //int loopIndex = constrain(map(pot2, 0, 1023, 0, numLoopOptions - 1), 0, numLoopOptions - 1);
  //int loopLength = loopOptions[loopIndex];
  int loopLength;
  if (pot1 > 400 && pot1 < 630) {  // fullyRandom range
    loopLength = 8;                // fixed loop for free-run randomness
  } else {
    int loopIndex = constrain(map(pot2, 0, 1023, 0, numLoopOptions - 1), 0, numLoopOptions - 1);
    loopLength = loopOptions[loopIndex];
  }


  // Map pot3 to gain factor
  float gain = pot3 / 1023.0;

  // Determine behavior from pot1
  bool fullyRandom = false;
  bool locked = false;
  bool doubleLock = false;
  bool slip = false;

  if (pot1 < 40) {
    doubleLock = true;
  } else if (pot1 < 400) {
    slip = true;
  } else if (pot1 < 630) {
    fullyRandom = true;
  } else if (pot1 < 950) {
    slip = true;
  } else {
    locked = true;
  }

  // Decide next bit
  bool newBit = 0;
  if (fullyRandom) {
    newBit = random(2);
  } else if (locked) {
    newBit = shiftRegister[loopLength - 1];
  } else if (doubleLock) {
    int index = (loopLength * 2) - 1;
    if (index < 16) {
      newBit = shiftRegister[index];
    } else {
      newBit = shiftRegister[15];  // fallback
    }
    /*  
  } else if (slip) {
    newBit = (random(100) < 10) ? random(2) : shiftRegister[loopLength - 1]; // FIXED 10% chance of slipping
  }
*/
  } else if (slip) {
    int slipChance = 10;

    if (pot1 < 400) {
      // Slip range from 40 to 400 → increase chance from 10% to 50%
      slipChance = map(pot1, 40, 400, 10, 50);
    } else if (pot1 >= 630 && pot1 < 970) {
      // Slip range from 630 to 970 → decrease chance from 50% to 10%
      slipChance = map(pot1, 630, 950, 50, 10);
    }

    newBit = (random(100) < slipChance) ? random(2) : shiftRegister[loopLength - 1];
  }



  // Shift values right
  for (int i = 15; i > 0; i--) {
    shiftRegister[i] = shiftRegister[i - 1];
  }
  shiftRegister[0] = newBit;

  // Convert top 8 bits to number
  uint16_t bits = 0;
  for (int i = 0; i < 8; i++) {
    bits |= shiftRegister[i] << (7 - i);  // Always use top 8 bits
  }

  // Map to 0–255 for fast PWM
  int cvValue = bits;  // Already 8-bit range
  int scaledValue = cvValue * gain;

  analogWriteFast(scaledValue);  // CV Out on D11 F4
  analogWrite(3, scaledValue);   // LED brightness on Pin 3
  quantized();                   // Quantized CV Out on D10 (F3)

  // Debugging
  Serial.print("CV: ");
  Serial.print(scaledValue);
  Serial.print(" | Loop: ");
  Serial.print(loopLength);
  Serial.print(" | Mode: ");
  if (fullyRandom) Serial.println("Random");
  else if (locked) Serial.println("Locked");
  else if (doubleLock) Serial.println("Double Lock");
  else if (slip) Serial.println("Slip");
}


void checkScaleButton() {
  bool buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == LOW && lastButtonState == HIGH) {
    unsigned long now = millis();
    if (now - lastDebounceTime > debounceDelay) {
      currentScaleIndex = (currentScaleIndex + 1) % NUM_SCALES;
      Serial.print("Switched to scale: ");
      Serial.println(currentScaleIndex);
      lastDebounceTime = now;
    }
  }
  lastButtonState = buttonState;
}


void quantized() {
  int pot3 = analogRead(pot3Pin);
  float gain = pot3 / 1023.0;

  // Use fixed 8-bit value (same as stepSequencer)
  int value = 0;
  for (int i = 0; i < 8; i++) {
    value |= (shiftRegister[i] << (7 - i));
  }

  // Apply gain and quantize
  int maxNoteIndex = totalNotes - 1;
  int rawNote = map(value, 0, 255, 0, maxNoteIndex);
  int scaledNote = rawNote * gain;
  scaledNote = constrain(scaledNote, 0, maxNoteIndex);

  int quantizedVal = quantizers[currentScaleIndex](scaledNote);
  analogWriteQuantized(quantizedVal);
}


// Fast PWM on Timer1 for Pin D10 (F3)
void setupFastPWM() {
  // --- F4 on D11 via Timer2 ---
  TCCR2A = _BV(COM2A1) | _BV(WGM21) | _BV(WGM20);  // Fast PWM on OC2A
  TCCR2B = _BV(CS20);                              // No prescaler
  OCR2A = 0;

  // --- F3 on D10 via Timer1 ---
  pinMode(10, OUTPUT);
  TCCR1A = _BV(COM1B1) | _BV(WGM10);  // Fast PWM 8-bit, non-inverting on OCR1B
  TCCR1B = _BV(WGM12) | _BV(CS10);    // No prescaler
  OCR1B = 0;
}

void analogWriteFast(uint8_t val) {
  OCR2A = val;
}

void analogWriteQuantized(uint8_t val) {
  OCR1B = val;  // Writes to F3
}
