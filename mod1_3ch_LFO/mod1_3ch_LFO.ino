/*
HAGIWO MOD1 3ch LFO Ver1.0. 3ch output LFO.
Adapted by Rob Heel. Added a fourth waveform: Random slope. 

--Pin assign---
POT1  A0  LFO1 frequency
POT2  A1  LFO2 frequency
POT3  A2  LFO3 frequency
F1    A3  frequency CV in (apply all ch LFO freq)
F2    A4  LFO1 output D9 
F3    A5  LFO2 output D10
F4    D11 LFO3 output D11
BUTTON    change waveform
LED       LFO1 output
EEPROM    Saves select waveform

This program generates independent LFO signals (D9, D10, D11) and an LED indicator (D3) at ~62.5kHz PWM.
Each LFO has a selectable waveform (Triangle, Square, Sine, Random Slope) chosen by a push button on D4 (INPUT_PULLUP).
The frequencies of the LFOs are controlled by three pots on A0, A1, A2, plus an offset on A3.
Now the maximum frequency is changed from 2Hz to 5Hz. (0.02 ~ 5Hz range)
Whenever the waveform changes, its type is saved to EEPROM and upon startup, the saved waveform type is restored from EEPROM.
*/

#include <EEPROM.h> // Include EEPROM library for read/write

// ---------------- Global Variables and Constants ----------------
static const unsigned int TABLE_SIZE = 1024;    // 1024 points in one waveform cycle
static const unsigned long UPDATE_INTERVAL_US = 400; // ~2500Hz LFO update rate
static const unsigned long debounceDelay = 50UL;     // 50ms for switch debounce

// Single wave table (8-bit resolution)
uint8_t waveTable[TABLE_SIZE];

// Current wave type: 0=Triangle, 1=Square, 2=Sine, 3=Random Slope
int waveType = 0; 

// Phase indexes for each LFO
float lfoIndex1 = 0.0; 
float lfoIndex2 = 0.0;
float lfoIndex3 = 0.0;

// LFO frequencies for each channel
float lfoFreq1 = 0.02;
float lfoFreq2 = 0.02;
float lfoFreq3 = 0.02;

// Timing variables for LFO update
unsigned long previousMicros = 0;

// Debounce variables
int lastButtonState = HIGH;               // HIGH means not pressed (pull-up)
unsigned long buttonPreviousMillis = 0;   // Last time we acknowledged a button press

// Random Slope variables
float currentSlopeValue1 = 0;
float targetSlopeValue1 = 0;
float slopeStep1 = 0;
unsigned long previousMillis1 = 0;

float currentSlopeValue2 = 0;
float targetSlopeValue2 = 0;
float slopeStep2 = 0;
unsigned long previousMillis2 = 0;

float currentSlopeValue3 = 0;
float targetSlopeValue3 = 0;
float slopeStep3 = 0;
unsigned long previousMillis3 = 0;

// ---------------- Function Prototypes ----------------
void configurePWM();                       // Set up Timer1 and Timer2 for ~62.5kHz PWM
void createWaveTable(int type);            // Re-generate wave table for the chosen wave type
void createTriangleTable();           
void createSquareTable();
void createSineTable();
void updateRandomSlope(float &currentMillis, float &previousMillis, float &currentSlopeValue, float &targetSlopeValue, float &slopeStep, float frequency);
float readFrequency(int analogPin);        // Maps analog input (0~1023) to 0.02~5.0Hz
float readFrequencyOffset(int analogPin);  // Maps analog input (0~1023) to 0.02~5.0Hz
void updateLFO(float &phaseIndex, float freq);
void handleButtonInput();                  // Debounced button reading using millis()

// ------------------------------------------------------
void setup() {
  // Configure I/O pins
  pinMode(9, OUTPUT);    // LFO1 (OCR1A)
  pinMode(10, OUTPUT);   // LFO2 (OCR1B)
  pinMode(11, OUTPUT);   // LFO3 (OCR2A)
  pinMode(3, OUTPUT);    // LED indicator (OCR2B)
  pinMode(4, INPUT_PULLUP); // Push button (pull-up); press => LOW

  // Configure Timer1 & Timer2 to ~62.5kHz PWM
  configurePWM();

  // Read waveType from EEPROM (address 0)
  int storedWaveType = EEPROM.read(0);  // Range could be 0~255
  // Validate the stored value (must be 0,1,2,3); if invalid, fallback to 0
  if (storedWaveType < 0 || storedWaveType > 3) {
    storedWaveType = 0; 
  }
  waveType = storedWaveType;

  // Create the initial wave table based on EEPROM
  createWaveTable(waveType);
}

// ------------------------------------------------------
void loop() {
  // Handle push button input (debounce) to change the waveform
  handleButtonInput();

  // Update LFO signals at ~2500Hz
  unsigned long currentMicros = micros();
  if (currentMicros - previousMicros >= UPDATE_INTERVAL_US) {
    previousMicros = currentMicros;

    // Read pot values for each LFO base frequency
    float baseFreq1 = readFrequency(A0);
    float baseFreq2 = readFrequency(A1);
    float baseFreq3 = readFrequency(A2);

    // Read offset from A3 (0~5V => 0.02~5.0Hz)
    float freqOffset = readFrequencyOffset(A3);

    // Combine them: baseFreq + offset
    lfoFreq1 = baseFreq1 + freqOffset;
    lfoFreq2 = baseFreq2 + freqOffset;
    lfoFreq3 = baseFreq3 + freqOffset;

  if (waveType == 3) { // Random Slope
    unsigned long currentMillis = millis();
    updateRandomSlope(currentMillis, previousMillis1, currentSlopeValue1, targetSlopeValue1, slopeStep1, lfoFreq1);
    updateRandomSlope(currentMillis, previousMillis2, currentSlopeValue2, targetSlopeValue2, slopeStep2, lfoFreq2);
    updateRandomSlope(currentMillis, previousMillis3, currentSlopeValue3, targetSlopeValue3, slopeStep3, lfoFreq3);

    // Set duty cycles directly via OCR registers
    OCR1A = currentSlopeValue1;  
    OCR1B = currentSlopeValue2;  
    OCR2A = currentSlopeValue3;  
    OCR2B = currentSlopeValue1;  // LED shows LFO1's output
  } else {
      // Update phase indexes
      updateLFO(lfoIndex1, lfoFreq1);
      updateLFO(lfoIndex2, lfoFreq2);
      updateLFO(lfoIndex3, lfoFreq3);

      // Get table positions (integer) from phase indexes
      int tablePos1 = (int)lfoIndex1 % TABLE_SIZE;
      int tablePos2 = (int)lfoIndex2 % TABLE_SIZE;
      int tablePos3 = (int)lfoIndex3 % TABLE_SIZE;

      // Read waveTable for each LFO
      uint8_t outputVal1 = waveTable[tablePos1];
      uint8_t outputVal2 = waveTable[tablePos2];
      uint8_t outputVal3 = waveTable[tablePos3];

      // Set duty cycles directly via OCR registers
      // LFO1 -> OCR1A (Pin 9)
      // LFO2 -> OCR1B (Pin 10)
      // LFO3 -> OCR2A (Pin 11)
      // LED indicator (same as LFO1) -> OCR2B (Pin 3)

      OCR1A = outputVal1;  
      OCR1B = outputVal2;  
      OCR2A = outputVal3;  
      OCR2B = outputVal1;  // LED shows LFO1's output
    }
  }
}

// ------------------------------------------------------
// Configure Timer1 (16-bit) and Timer2 (8-bit) for ~62.5kHz PWM
void configurePWM() {
  // ---- Timer1 setup (Pins 9=OCR1A, 10=OCR1B) ----
  // Fast PWM 8-bit mode: WGM10=1, WGM11=0, WGM12=1, WGM13=0
  // Non-inverting for OCR1A, OCR1B: COM1A1=1, COM1B1=1
  // No prescaler: CS10=1
  TCCR1A = 0; 
  TCCR1B = 0;
  TCCR1A |= (1 << WGM10) | (1 << COM1A1) | (1 << COM1B1);  
  TCCR1B |= (1 << WGM12) | (1 << CS10);                   

  // ---- Timer2 setup (Pins 3=OCR2B, 11=OCR2A) ----
  // Fast PWM mode (0xFF): WGM20=1, WGM21=1, WGM22=0
  // Non-inverting for OCR2A, OCR2B: COM2A1=1, COM2B1=1
  // No prescaler: CS20=1
  TCCR2A = 0; 
  TCCR2B = 0; 
  TCCR2A |= (1 << WGM20) | (1 << WGM21) | (1 << COM2A1) | (1 << COM2B1); 
  TCCR2B |= (1 << CS20); 
}

// ------------------------------------------------------
// Debounced button input using millis()
void handleButtonInput() {
  // Read current button state (LOW when pressed)
  int reading = digitalRead(4);
  unsigned long currentMillis = millis();

  // Check if enough time has passed to confirm a new button event
  if (currentMillis - buttonPreviousMillis > debounceDelay) {
    // Detect transition from HIGH to LOW (button press)
    if (reading == LOW && lastButtonState == HIGH) {
      // Cycle waveType: 0->1->2->3->0->...
      waveType = (waveType + 1) % 4;

      // Regenerate the wave table for the new wave type
      createWaveTable(waveType);

      // Save the new waveType to EEPROM
      EEPROM.write(0, waveType);

      // Update the timestamp of this confirmed press
      buttonPreviousMillis = currentMillis;
    }
  }
  // Update for next iteration
  lastButtonState = reading;
}

// ------------------------------------------------------
// Create wave table based on waveType
void createWaveTable(int type) {
  switch (type) {
    case 0: // Triangle
      createTriangleTable();
      break;
    case 1: // Square
      createSquareTable();
      break;
    case 2: // Sine
      createSineTable();
      break;
    default: // Random Slope (no wave table needed)
      break;
  }
}

// ------------------------------------------------------
// Create a triangle wave in waveTable (8-bit range 0~255)
void createTriangleTable() {
  // First half rising 0->255, second half falling 255->0
  for (int i = 0; i < TABLE_SIZE; i++) {
    if (i < (TABLE_SIZE / 2)) {
      float val = (255.0f * i) / (TABLE_SIZE / 2);
      waveTable[i] = (uint8_t)val;
    } else {
      int j = i - (TABLE_SIZE / 2);
      float val = 255.0f - (255.0f * j) / (TABLE_SIZE / 2);
      waveTable[i] = (uint8_t)val;
    }
  }
}

// ------------------------------------------------------
// Create a square wave in waveTable (8-bit range 0 or 255)
void createSquareTable() {
  // First half of cycle = 0, second half = 255
  for (int i = 0; i < TABLE_SIZE; i++) {
    if (i < (TABLE_SIZE / 2)) {
      waveTable[i] = 0;
    } else {
      waveTable[i] = 255;
    }
  }
}

// ------------------------------------------------------
// Create a sine wave in waveTable (8-bit range 0~255)
void createSineTable() {
  for (int i = 0; i < TABLE_SIZE; i++) {
    // Angle from 0 to 2*pi
    float angle = 2.0f * 3.14159265359f * ((float)i / (float)TABLE_SIZE);
    // Map sin() from -1~+1 to 0~255
    float sineVal = (sin(angle) + 1.0f) * 127.5f;
    if (sineVal < 0.0f)   sineVal = 0.0f;
    if (sineVal > 255.0f) sineVal = 255.0f;
    waveTable[i] = (uint8_t)sineVal;
  }
}

// ------------------------------------------------------
// Map analog input (0~1023) to 0.02~5.0Hz
float readFrequency(int analogPin) {
  int rawVal = analogRead(analogPin); 
  float fMin = 0.02f;
  float fMax = 5.0f;      // Changed upper limit to 5.0Hz
  float freq = fMin + (fMax - fMin) * (rawVal / 1023.0f);
  return freq;
}

// ------------------------------------------------------
// Map analog input (0~1023) to 0.02~5.0Hz (offset)
float readFrequencyOffset(int analogPin) {
  int rawVal = analogRead(analogPin);
  float fMin = 0.02f;
  float fMax = 5.0f;      // Changed upper limit to 5.0Hz
  float offset = fMin + (fMax - fMin) * (rawVal / 1023.0f);
  return offset;
}

// ------------------------------------------------------
// Update LFO phase index based on frequency
void updateLFO(float &phaseIndex, float freq) {
  // ~2500 updates per second => increment = freq * (TABLE_SIZE / 2500)
  float increment = freq * ((float)TABLE_SIZE / 2500.0f);
  phaseIndex += increment;
  // The table access uses % TABLE_SIZE on int cast, so no clamp needed here
}

// ------------------------------------------------------
// Update Random Slope waveform
void updateRandomSlope(unsigned long currentMillis, unsigned long &previousMillis, float &currentSlopeValue, float &targetSlopeValue, float &slopeStep, float frequency) {
    if (currentMillis - previousMillis >= (1000.0 / frequency)) {
        previousMillis = currentMillis;
        targetSlopeValue = random(0, 256); // Generate a random value between 0 and 255
        slopeStep = (targetSlopeValue - currentSlopeValue) / (1000.0 / frequency);
    }

    currentSlopeValue += slopeStep;

    // Keep within bounds
    if ((slopeStep > 0 && currentSlopeValue >= targetSlopeValue) ||
        (slopeStep < 0 && currentSlopeValue <= targetSlopeValue)) {
        currentSlopeValue = targetSlopeValue;
    }
}