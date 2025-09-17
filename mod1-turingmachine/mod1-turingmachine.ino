/*
  MOD1 Turing/KLEE-style Sequencer + BOOT-HOLD SERIAL TUNING MENU (manual with osc/tuner)
  Target: Arduino Nano — HAGIWO MOD1 general-purpose module

  I/O map (MOD1 jacks & pots):
    F1 (A3 / D17)  : Trigger IN (digital)
    F2 (A4)        : CV IN for Pot1 (analog) 
    F3 (D10 / OCR1B): Quantized CV OUT via Fast PWM (0–5 V after RC/op-amp)
    F4 (D11 / OCR2A): Raw CV OUT via Fast PWM (0–5 V)
    Pot1 (A0)      : Randomness vs. Lock
    Pot2 (A1)      : Loop length selector (2,3,4,5,6,8,12,16)
    Pot3 (A2)      : Gain (0..5 V scaling)
    BUTTON_PIN (D4): Short press = cycle scale; Hold on power-up = enter TUNING MENU
    LED (D3 / OCR2B): Activity / progress

  Manual tuning menu:
    - You connect your VCO 1V/Oct pitch input and read pitch on a tuner.
    - Select a note (C2..C5 → indices 0..36), adjust its PWM up/down until the tuner is correct.
    - Save table to EEPROM. Sequencer quantizers use the calibrated table afterwards.

  Serial: 115200 baud.

  Notes:
    - MOD1 PWM ripple ~25 mV @ 62.5 kHz; per-note PWM table compensates static non-linearity well.
    - For best results, warm up your VCO 10–15 minutes. Set 0 V ≈ C2 before starting.
*/

#include <Arduino.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>
#include <math.h>

// ================== Pins ==================
#define BUTTON_PIN 4
const int triggerPin = 17;  // F1 (A3 / D17) digital in
const int cvOutPin  = 11;   // F4 (D11 / OCR2A) raw CV out
const int ledPin    = 3;    // LED (D3 / OCR2B)
const int pot1Pin   = A0;   // randomness/lock
const int pot2Pin   = A1;   // loop length
const int pot3Pin   = A2;   // gain
const int pot1cvPin = A4;   // F2 analog in (runtime)
const int PWM_Q_PIN = 10;   // F3 (D10 / OCR1B) quantized CV

// ================== Sequencer state ==================
typedef int (*QuantizeFunction)(int);

// Forward decl (quantizers use calTable[])
int qMajor(int val);
int qMinor(int val);
int qPhrygian(int val);

QuantizeFunction quantizers[] = { qMajor, qMinor, qPhrygian };
#define NUM_SCALES (sizeof(quantizers) / sizeof(quantizers[0]))

int currentScaleIndex = 0;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;
bool lastButtonState = HIGH;

bool shiftRegister[16] = { 0 };  // 16-step binary sequence
const int loopOptions[] = { 2, 3, 4, 5, 6, 8, 12, 16 };
const int numLoopOptions = sizeof(loopOptions) / sizeof(loopOptions[0]);
bool prevClockState = LOW;

// ================== Tuning table (factory defaults) ==================
// 37 semitones (C .. B over 3 octaves + C), 8-bit PWM domain (0..255)
const uint8_t factoryTuningValues[] PROGMEM = {
  0, 11, 15, 19, 23, 27, 31, 35, 40, 44, 48, 53,
  57, 61, 65, 69, 73, 77, 82, 86, 90, 94, 99, 103,
  107, 112, 116, 120, 124, 128, 132, 137, 141, 145, 149, 153,
  157
};
const int totalNotes = sizeof(factoryTuningValues) / sizeof(factoryTuningValues[0]);

// Runtime calibration table used by quantizers (filled from EEPROM or factory)
uint8_t calTable[37];

// Modal step sets (C as root)
static const uint8_t majorSteps[7]    = {0, 2, 4, 5, 7, 9, 11}; // C major
static const uint8_t minorSteps[7]    = {0, 2, 3, 5, 7, 8, 10}; // C natural minor
static const uint8_t phrygianSteps[7] = {0, 1, 3, 5, 7, 8, 10}; // Phrygian

// For printing note names
const char * const noteNames[12] PROGMEM = {
  "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// ================== EEPROM format ==================
struct CalBlob {
  char    sig[4];       // "CVT1"
  uint8_t table[37];    // 37 semitones
};
const int EEPROM_ADDR = 0;

// ================== PWM setup ==================
void setupFastPWM() {
  // --- F4 on D11 via Timer2 (8-bit Fast PWM, non-inverting, no prescaler) ---
  TCCR2A = _BV(COM2A1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS20);
  OCR2A = 0;

  // --- F3 on D10 via Timer1 (8-bit Fast PWM on OCR1B, non-inverting, no prescaler) ---
  pinMode(PWM_Q_PIN, OUTPUT);
  TCCR1A = _BV(COM1B1) | _BV(WGM10);
  TCCR1B = _BV(WGM12) | _BV(CS10);
  OCR1B = 0;
}

inline void analogWriteFast(uint8_t val)   { OCR2A = val; }  // D11 (raw CV)
inline void analogWriteQuantized(uint8_t v){ OCR1B = v;   }  // D10 (quantized CV)

// ================== Quantizers (use calTable[]) ==================
int qFromSteps(int val, const uint8_t* steps, uint8_t nSteps) {
  int octave  = val / 12;
  int semitone= val % 12;
  for (uint8_t i = 0; i < nSteps; i++) {
    if (semitone <= steps[i]) {
      int idx = octave * 12 + steps[i];
      if (idx >= totalNotes) idx = totalNotes - 1;
      return calTable[idx]; // returns 0..255 (PWM)
    }
  }
  int idx = octave * 12 + steps[nSteps - 1];
  if (idx >= totalNotes) idx = totalNotes - 1;
  return calTable[idx];
}

int qMajor(int val)    { return qFromSteps(val, majorSteps, 7); }
int qMinor(int val)    { return qFromSteps(val, minorSteps, 7); }
int qPhrygian(int val) { return qFromSteps(val, phrygianSteps, 7); }

// ================== EEPROM helpers ==================
void loadCalibration() {
  CalBlob blob;
  EEPROM.get(EEPROM_ADDR, blob);
  if (blob.sig[0]=='C' && blob.sig[1]=='V' && blob.sig[2]=='T' && blob.sig[3]=='1') {
    memcpy(calTable, blob.table, totalNotes);
    Serial.println(F("Calibration loaded from EEPROM."));
  } else {
    for (int i=0;i<totalNotes;i++) {
      calTable[i] = pgm_read_byte_near(factoryTuningValues + i);
    }
    Serial.println(F("No calibration found; using factory table."));
  }
}

void saveCalibration() {
  CalBlob blob;
  blob.sig[0]='C'; blob.sig[1]='V'; blob.sig[2]='T'; blob.sig[3]='1';
  memcpy(blob.table, calTable, totalNotes);
  EEPROM.put(EEPROM_ADDR, blob);
  Serial.println(F("Calibration saved to EEPROM."));
}

void resetToFactory() {
  for (int i=0;i<totalNotes;i++) {
    calTable[i] = pgm_read_byte_near(factoryTuningValues + i);
  }
  Serial.println(F("Reverted to factory table (not saved). Use 'w' to persist."));
}

// ================== Targets / Note helpers ==================
// Target frequency for index 0..36, mapping 0→C2, 12→C3, 24→C4, 36→C5
double targetFreqForIndex(int idx) {
  int octave = 2 + (idx / 12);     // 2..5
  int semit  = idx % 12;
  // C0 = 16.3516 Hz
  int absSemisFromC0 = octave * 12 + semit;
  return 16.3516 * pow(2.0, absSemisFromC0 / 12.0);
}

void noteNameForIndex(int idx, char *buf, size_t bufsz) {
  int octave = 2 + (idx / 12);
  int semit  = idx % 12;
  char nn[4];
  strncpy_P(nn, (PGM_P)pgm_read_word(&(noteNames[semit])), sizeof(nn));
  nn[sizeof(nn)-1] = '\0';
  snprintf(buf, bufsz, "%s%d", nn, octave);
}

// ================== SERIAL TUNING MENU ==================
void printTuningHelp() {
  Serial.println(F("\n=== CV TUNING MENU (manual, D10 outputs selected note) ==="));
  Serial.println(F("Commands:"));
  Serial.println(F("  h           : help"));
  Serial.println(F("  n / p       : next / prev note"));
  Serial.println(F("  i <0..36>   : jump to note index (C2=0 .. C5=36)"));
  Serial.println(F("  + / -       : PWM up/down by step (default step=1)"));
  Serial.println(F("  S <1..16>   : set step size"));
  Serial.println(F("  v <0..255>  : set PWM directly"));
  Serial.println(F("  a           : auto-scan (2 s per note, no changes)"));
  Serial.println(F("  w / r / f   : write to EEPROM / reload / factory defaults"));
  Serial.println(F("  t           : print target info for current note"));
  Serial.println(F("  x           : exit and run sequencer\n"));
}

void printNoteState(int idx) {
  char nameBuf[5];
  noteNameForIndex(idx, nameBuf, sizeof(nameBuf));
  double f = targetFreqForIndex(idx);
  Serial.print(F("[Note ")); Serial.print(idx); Serial.print(F("] "));
  Serial.print(nameBuf);
  Serial.print(F("  target=")); Serial.print(f, 2); Serial.print(F(" Hz"));
  Serial.print(F("  PWM=")); Serial.print(calTable[idx]);
  Serial.println();
}

void runTuningMenu() {
  Serial.setTimeout(1500);
  analogWrite(ledPin, 64);

  int idx = 0;                  // start at C2
  uint8_t step = 1;             // fine step
  bool running = true;

  // Output current note on D10
  analogWriteQuantized(calTable[idx]);

  printTuningHelp();
  printNoteState(idx);

  while (running) {
    // keep outputting current PWM (steady DC) on D10 for your tuner
    // (nothing needed here; OCR1B holds its last value)

    if (Serial.available()) {
      char c = Serial.read();

      switch (c) {
        case 'h': printTuningHelp(); break;

        case 'n':
          if (idx < totalNotes-1) { idx++; analogWriteQuantized(calTable[idx]); }
          printNoteState(idx);
          break;

        case 'p':
          if (idx > 0) { idx--; analogWriteQuantized(calTable[idx]); }
          printNoteState(idx);
          break;

        case '+':
          if (calTable[idx] < 255) calTable[idx] = (uint8_t)min(255, calTable[idx] + step);
          analogWriteQuantized(calTable[idx]);
          printNoteState(idx);
          break;

        case '-':
          if (calTable[idx] > 0) calTable[idx] = (uint8_t)max(0, calTable[idx] - step);
          analogWriteQuantized(calTable[idx]);
          printNoteState(idx);
          break;

        case 'S': {
          long s = Serial.parseInt();
          if (s >= 1 && s <= 16) {
            step = (uint8_t)s;
            Serial.print(F("Step set to ")); Serial.println(step);
          } else {
            Serial.println(F("Invalid step. Use 1..16"));
          }
        } break;

        case 'i': {
          long newIdx = Serial.parseInt();
          if (newIdx >= 0 && newIdx < totalNotes) {
            idx = (int)newIdx;
            analogWriteQuantized(calTable[idx]);
            printNoteState(idx);
          } else {
            Serial.println(F("Invalid index. Use 0..36"));
          }
        } break;

        case 'v': {
          long v = Serial.parseInt();
          if (v >= 0 && v <= 255) {
            calTable[idx] = (uint8_t)v;
            analogWriteQuantized(calTable[idx]);
            printNoteState(idx);
          } else {
            Serial.println(F("Invalid PWM. Use 0..255"));
          }
        } break;

        case 'a': {
          Serial.println(F("Auto-scan start (no changes made)..."));
          for (int k=0;k<totalNotes;k++) {
            char nameBuf[5];
            noteNameForIndex(k, nameBuf, sizeof(nameBuf));
            analogWriteQuantized(calTable[k]);
            Serial.print(F("Note ")); Serial.print(k);
            Serial.print(F("  ")); Serial.print(nameBuf);
            Serial.print(F("  target=")); Serial.print(targetFreqForIndex(k),2);
            Serial.print(F(" Hz  PWM=")); Serial.println(calTable[k]);
            delay(2000);
          }
          Serial.println(F("Auto-scan done."));
          // restore current idx output
          analogWriteQuantized(calTable[idx]);
        } break;

        case 'w': saveCalibration(); break;
        case 'r': loadCalibration(); analogWriteQuantized(calTable[idx]); printNoteState(idx); break;
        case 'f': resetToFactory();  analogWriteQuantized(calTable[idx]); printNoteState(idx); break;

        case 't': printNoteState(idx); break;

        case 'x':
          running = false;
          break;

        case '\n':
        case '\r':
        case ' ':
          // ignore whitespace
          break;

        default:
          Serial.print(F("Unknown cmd '")); Serial.print(c); Serial.println(F("'. Press 'h' for help."));
          break;
      }
    }
  }

  analogWrite(ledPin, 0);
  Serial.println(F("Exiting tuning menu. Running sequencer..."));
}

// ================== Setup / Loop ==================
void setup() {
  Serial.begin(115200);
  pinMode(triggerPin, INPUT);
  pinMode(cvOutPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  setupFastPWM();
  analogWriteFast(0);
  analogWrite(ledPin, 0);

  // Seed table from factory, then try to load calibration
  for (int i=0;i<totalNotes;i++) calTable[i] = pgm_read_byte_near(factoryTuningValues + i);
  loadCalibration();

  // Boot-hold to enter SERIAL TUNING MENU
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(600); // hold
    if (digitalRead(BUTTON_PIN) == LOW) {
      runTuningMenu();
    }
  }

  randomSeed(analogRead(A7));  // free analog pin for entropy
}

void loop() {
  checkScaleButton();
  bool currClockState = digitalRead(triggerPin);
  if (currClockState == HIGH && prevClockState == LOW) {
    stepSequencer();
  }
  prevClockState = currClockState;
}

// ================== Sequencer logic ==================
void stepSequencer() {
  // Read pots
  int pot1Hardware = analogRead(pot1Pin);
  int pot1cvInput  = analogRead(pot1cvPin); // F2 analog (runtime)
  pot1cvInput = map(pot1cvInput, 0, 900, 0, 1023); // remap for full-range feel
  int pot1 = pot1Hardware + pot1cvInput;
  pot1 = constrain(pot1, 0, 1023);

  int pot2 = analogRead(pot2Pin);  // loop length
  int pot3 = analogRead(pot3Pin);  // gain
  float gain = pot3 / 1023.0f;

  int loopLength;
  if (pot1 > 400 && pot1 < 630) {
    loopLength = 8; // fixed loop in fullyRandom region
  } else {
    int loopIndex = constrain(map(pot2, 0, 1023, 0, numLoopOptions - 1), 0, numLoopOptions - 1);
    loopLength = loopOptions[loopIndex];
  }

  bool fullyRandom = false, locked = false, doubleLock = false, slip = false;
  if (pot1 < 40)            doubleLock = true;
  else if (pot1 < 400)      slip = true;
  else if (pot1 < 630)      fullyRandom = true;
  else if (pot1 < 950)      slip = true;
  else                      locked = true;

  // Decide next bit
  bool newBit = 0;
  if (fullyRandom) {
    newBit = random(2);
  } else if (locked) {
    newBit = shiftRegister[loopLength - 1];
  } else if (doubleLock) {
    int index = (loopLength * 2) - 1;
    if (index < 16) newBit = shiftRegister[index];
    else            newBit = shiftRegister[15];
  } else if (slip) {
    int slipChance = 10;
    if (pot1 < 400)                       slipChance = map(pot1, 40, 400, 10, 50);
    else if (pot1 >= 630 && pot1 < 970)   slipChance = map(pot1, 630, 950, 50, 10);
    newBit = (random(100) < slipChance) ? random(2) : shiftRegister[loopLength - 1];
  }

  // Shift
  for (int i = 15; i > 0; i--) shiftRegister[i] = shiftRegister[i - 1];
  shiftRegister[0] = newBit;

  // 8-bit value from top 8 bits
  uint16_t bits = 0;
  for (int i = 0; i < 8; i++) bits |= shiftRegister[i] << (7 - i);

  int cvValue = bits;                  // 0..255
  int scaledValue = (int)(cvValue * gain);

  // Outputs
  analogWriteFast(scaledValue);  // D11 raw CV
  analogWrite(ledPin, scaledValue);
  quantized();                   // D10 quantized CV
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
  float gain = pot3 / 1023.0f;

  // Same 8-bit value as stepSequencer()
  int value = 0;
  for (int i = 0; i < 8; i++) value |= (shiftRegister[i] << (7 - i));

  int maxNoteIndex = totalNotes - 1;         // 36
  int rawNote = map(value, 0, 255, 0, maxNoteIndex);
  int scaledNote = (int)(rawNote * gain);
  scaledNote = constrain(scaledNote, 0, maxNoteIndex);

  int quantizedVal = quantizers[currentScaleIndex](scaledNote); // returns 0..255 from calTable
  analogWriteQuantized((uint8_t)quantizedVal);
}
