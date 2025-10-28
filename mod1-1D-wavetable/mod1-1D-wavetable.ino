/*
  Procedurally Generated Triple Wavetable LFO

  Triple wavetable / terrain LFO with CV speed modulation input and probabilistic SloMo mode.
  Firmware idea by Rob Heel for Mod1 eurorack module by Hagiwo. 

  On each button press, three independent wavetables (“terrains”) are generated.  
  Pot C controls the number of “knots” (points) in each waveform. Individual outs on F2, F3, F4.  

  Waveform generation is semi-random, following musical constraints:
  - Starts and ends at the same value for seamless looping.
  - Contains at least one zero crossing.
  - Nonlinear knot spacing.
  - After a spike, a longer rest region follows.
  - One curved segment per waveform (Bézier)

  Wavetables reading has a defined detune in speed: 
  speed1 * 0.9 -> F2; speed2 * 1.0 -> F3; speed3 * 1.1 -> F4;

  Random slow-mo events that scale with tempo - probability set via Potb. 
  SloMo randomly and independently slows down the playback speed of each terrain waveform for a short, 
  speed-dependent duration — creating natural, unsynced pauses or “breaths” in their motion.

  CV input (0 to 5 Volts) on F1 (A3) for speed offset (adds 0–1 Hz to base speed).

  Pots:
    A0 -> Base speed (0.01–5 Hz)
    A1 -> SloMo  probability
    A2 -> Knots (3..12)

  Button:
    D4 -> generate new set of waveforms

  LED:
    D3 -> blink during generation

  Outputs:
    F2 (D9)  : Terrain 1  
    F3 (D10) : Terrain 2
    F4 (D11) : Terrain 3

  Inputs:
    F1 (A3) : CV input for speed offset (adds 0–1 Hz to base speed)
*/

#define TABLE_SIZE 256
uint16_t terrain1[TABLE_SIZE];
uint16_t terrain2[TABLE_SIZE];
uint16_t terrain3[TABLE_SIZE];

float phase1 = 0.0, phase2 = 0.0, phase3 = 0.0;
float lastPwm1 = 128.0, lastPwm2 = 128.0, lastPwm3 = 128.0;

// --- Uncomment to enable serial debug ---
// #define DEBUG_SERIAL

// --- Pins ---
const int potA = A0;   // speed
const int potB = A1;   // SloMo probability
const int potC = A2;   // knots
const int cvIn = A3;   // F1 speed modulation CV in
const int buttonPin = 4;
const int ledPin = 3;

// --- Debounce ---
bool lastButtonState = HIGH;
unsigned long lastDebounce = 0;
const unsigned long debounceDelay = 50;
bool blinking = false;
unsigned long blinkStart = 0;
const unsigned long blinkDuration = 200;

// --- Utils ---
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// -----------------------------------------------------------------------------
// setup()
// -----------------------------------------------------------------------------
void setup() {
  pinMode(potA, INPUT);
  pinMode(potB, INPUT);
  pinMode(potC, INPUT);
  pinMode(cvIn, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  // PWM outs
  pinMode(9, OUTPUT);   // F2
  pinMode(10, OUTPUT);  // F3
  pinMode(11, OUTPUT);  // F4

  // --- Timer1 (16-bit) PWM Setup for D10 (F3) ---
  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM10);
  TCCR1B = _BV(WGM12) | _BV(CS10);   // ~31.4 kHz PWM, no prescale

  // --- Timer2 (8-bit) PWM Setup for D11 (F4) ---
  TCCR2A = _BV(COM2A1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS21); // prescale 8 → ~7.8 kHz PWM

  OCR1A = 128;
  OCR1B = 128;
  OCR2A = 128;

  randomSeed(analogRead(A7));

  #ifdef DEBUG_SERIAL
  Serial.begin(115200);
  Serial.println("Dual Terrain LFO Ready");
  #endif

  generateTerrains(); // initial generation
}

// -----------------------------------------------------------------------------
// generateTerrain() helper for one table
// -----------------------------------------------------------------------------
void generateSingleTerrain(uint16_t* table, int knots, float heightScale) {
  float knotVals[24];
  for (int k = 0; k < knots; ++k) {
    float val = (random(0, 1000) / 1000.0f) * heightScale;
    knotVals[k] = val;
  }
  knotVals[random(0, knots)] = 0.0f;
  knotVals[random(0, knots)] = heightScale;

  float knotPos[24];
  float total = 0.0f;
  for (int k = 0; k < knots; ++k) {
    float step = random(50, 200) / 1000.0f;
    total += step;
    knotPos[k] = total;
  }
  for (int k = 0; k < knots; ++k) knotPos[k] /= total;

  knotVals[knots - 1] = knotVals[0];
  knotPos[knots - 1] = 1.0f;

  int curvedSegment = random(0, knots - 1);
  float curvature = random(-80, 80) / 100.0f;

  for (int i = 0; i < TABLE_SIZE; ++i) {
    float t = (float)i / (TABLE_SIZE - 1);
    int k0 = 0;
    while (k0 < knots - 1 && t > knotPos[k0 + 1]) k0++;
    int k1 = min(k0 + 1, knots - 1);
    float frac = (t - knotPos[k0]) / (knotPos[k1] - knotPos[k0]);

    float v;
    if (k0 == curvedSegment) {
      float v0 = knotVals[k0];
      float v1 = knotVals[k1];
      float vMid = (v0 + v1) / 2.0f + curvature;
      vMid = constrain(vMid, 0.0f, 1.0f);
      float oneMinusT = 1.0f - frac;
      v = oneMinusT * oneMinusT * v0
        + 2.0f * oneMinusT * frac * vMid
        + frac * frac * v1;
    } else {
      v = knotVals[k0] * (1.0f - frac) + knotVals[k1] * frac;
    }

    table[i] = (uint16_t)(constrain(v, 0.0f, 1.0f) * 65535.0f);
  }
}

// -----------------------------------------------------------------------------
// generateTerrains() –  at once
// -----------------------------------------------------------------------------
void generateTerrains() {
  digitalWrite(ledPin, HIGH);
  blinking = true;
  blinkStart = millis();

  int rawC = analogRead(potC);
  int knots = mapFloat(rawC, 0, 1023, 3, 12);
  float heightScale = 1.0;

/*
  randomSeed(analogRead(A7));
  generateSingleTerrain(terrain1, knots, heightScale);

  randomSeed(micros());
  generateSingleTerrain(terrain2, knots, heightScale);

  generateSingleTerrain(terrain3, knots, heightScale);
  */

  randomSeed(analogRead(A7));
  generateSingleTerrain(terrain1, knots, heightScale);

  randomSeed(analogRead(A6));
  generateSingleTerrain(terrain2, knots, heightScale);
  
  randomSeed(micros());
  generateSingleTerrain(terrain3, knots, heightScale);

  phase1 = phase2 = phase3 = 0.0;

  #ifdef DEBUG_SERIAL
  Serial.println("New terrains generated");
  #endif
}

// -----------------------------------------------------------------------------
// loop()
// -----------------------------------------------------------------------------
void loop() {
  // --- button ---
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) lastDebounce = millis();
  if ((millis() - lastDebounce) > debounceDelay) {
    static int lastStableState = HIGH;
    if (reading != lastStableState) {
      lastStableState = reading;
      if (reading == LOW) generateTerrains();
    }
  }
  lastButtonState = reading;

  if (blinking && millis() - blinkStart > blinkDuration) {
    digitalWrite(ledPin, LOW);
    blinking = false;
  }

  // --- base speed from pot ---
  float speedCtrl = analogRead(potA) / 1023.0;
  //float baseHz = 0.01 * pow(500.0, speedCtrl);  // 0.01..5 Hz exponential
  float baseHz = 0.01 * pow(300.0, speedCtrl);  // 0.01 .. ~3 Hz

  // --- CV modulation ---
  int rawCV = analogRead(cvIn);
  float cvHz = mapFloat(rawCV, 0, 1023, 0.0, 1.0); // 0–1 Hz
  float tableHz = baseHz + cvHz;                   // combined

  // --- clamp ---
  if (tableHz < 0.0) tableHz = 0.0;    // safety cap
  if (tableHz > 10.0) tableHz = 10.0;  // safety cap

  // --- time step ---
  static unsigned long lastTime = 0;
  unsigned long now = micros();
  float dt = (now - lastTime) / 1e6;
  if (dt <= 0) dt = 0.001;
  lastTime = now;


    // --- PotB controls slowdown probability/intensity ---
    float intensity = analogRead(potB) / 1023.0;  // 0.0 .. 1.0

    // --- fixed detune between the three outputs ---
    float speed1 = tableHz * 0.9;   // F2
    float speed2 = tableHz * 1.0;   // F3
    float speed3 = tableHz * 1.1;   // F4

    // --- SlowMo struct for independent slowdown events ---
    struct SlowMo {
      bool active;
      float slowFactor;      // e.g., 0.2 = 5× slower
      float duration;        // milliseconds
      unsigned long startTime;
    };
    static SlowMo slow1, slow2, slow3;

    // Helper lambda to maybe trigger one slowdown
    auto maybeTriggerSlowmo = [&](SlowMo &s) {
      // PotB controls probability of triggering
      if (!s.active && random(10000) < intensity * 5) {
        s.active = true;                             // intensity * 5 -> 0.05 % chance
        s.slowFactor = random(10, 50) / 100.0;       // 0.1–0.5x speed
        float durBase = 800 + random(200, 3000);     // 0.8–3 s // or base 2000 + random(1000, 6000) 
        s.duration = durBase / (tableHz + 0.1);      // slower base speed → longer event
        s.startTime = millis();
      }
      if (s.active && (millis() - s.startTime > s.duration)) {
        s.active = false;                             // back to normal speed
      }
    };

    // Evaluate possible slowdowns for each waveform
    maybeTriggerSlowmo(slow1);
    maybeTriggerSlowmo(slow2);
    maybeTriggerSlowmo(slow3);

    // --- Apply slowdown factors if active ---
    float actualSpeed1 = slow1.active ? speed1 * slow1.slowFactor : speed1;
    float actualSpeed2 = slow2.active ? speed2 * slow2.slowFactor : speed2;
    float actualSpeed3 = slow3.active ? speed3 * slow3.slowFactor : speed3;

  // --- Advance phases ---
  phase1 += actualSpeed1 * TABLE_SIZE * dt;
  phase2 += actualSpeed2 * TABLE_SIZE * dt;
  phase3 += actualSpeed3 * TABLE_SIZE * dt;

  // Wrap around
  if (phase1 >= TABLE_SIZE) phase1 -= TABLE_SIZE;
  if (phase2 >= TABLE_SIZE) phase2 -= TABLE_SIZE;
  if (phase3 >= TABLE_SIZE) phase3 -= TABLE_SIZE;

  // --- interpolation helper ---
  auto interpTable = [](uint16_t* tbl, float phase) {
    int i0 = (int)phase;
    int i1 = i0 + 1;
    if (i1 >= TABLE_SIZE) i1 = 0;
    float frac = phase - (float)i0;
    int32_t delta = (int32_t)tbl[i1] - (int32_t)tbl[i0];
    int32_t tmp = (int32_t)tbl[i0] + (int32_t)(delta * frac);
    tmp = constrain(tmp, 0, 65535);
    return (uint16_t)tmp;
  };

    // --- interpolate ---
    uint16_t interp1 = interpTable(terrain1, phase1);
    uint16_t interp2 = interpTable(terrain2, phase2);
    uint16_t interp3 = interpTable(terrain3, phase3);

    int pwm1 = (interp1 + 128) >> 8;
    int pwm2 = (interp2 + 128) >> 8;
    int pwm3 = (interp3 + 128) >> 8;

    lastPwm1 = lastPwm1 * 0.9 + pwm1 * 0.1;
    lastPwm2 = lastPwm2 * 0.9 + pwm2 * 0.1;
    lastPwm3 = lastPwm3 * 0.9 + pwm3 * 0.1;

    // --- write to PWM registers ---
    OCR1A = (uint8_t)lastPwm1;    // F2
    OCR1B = (uint8_t)lastPwm2;    // F3
    OCR2A = (uint8_t)lastPwm3;    // F4

  #ifdef DEBUG_SERIAL
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 100) {
    Serial.print("CVHz="); Serial.print(cvHz, 2);
    Serial.print(" BaseHz="); Serial.print(baseHz, 2);
    Serial.print(" TotalHz="); Serial.println(tableHz, 2);
    lastPrint = millis();
  }
  #endif
}
