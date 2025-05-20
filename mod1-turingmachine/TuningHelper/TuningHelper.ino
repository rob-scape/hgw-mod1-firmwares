// Tuning Helper - Outputs fast PWM values to Pin F3 (D10)
// Adjust and verify each note one by one using a tuner

const int pwmOutPin = 10;  // F3 / D10
const int tuningValues[] = {
  0,   // C         0
  11,  // C#        1
  15,  // D         2
  19,  // D#        3
  23,  // E         4
  27,  // F         5
  31,  // F#        6
  35,  // G         7
  40,  // G#        8
  44,  // A         9
  48,  // A#        10
  53,  // B         11

  57,  // C         12
  61,  // C#        13
  65,  // D         14
  69,  // D#        15
  73,  // E         16
  77,  // F         17
  82,  // F#        18
  86,  // G         19
  90,  // G#        20
  94,  // A         21
  99,  // A#        22
  103,  // B        23

  107,  // C        24 
  112,  // C#       25
  116,  // D        26
  120,  // D#       27
  124,  // E        28
  128,  // F        29
  132,  // F#       30
  137,  // G        31
  141,  // G#       32
  145,  // A        33
  149,  // A#       34
  153,  // B        35

  157  // C         36 

};

void setup() {
  Serial.begin(9600);
  setupFastPWM();

  // === COMMENT/UNCOMMENT ONE AT A TIME FOR TUNING ===
  analogWriteQuantized(tuningValues[0]);   // C
  //analogWriteQuantized(tuningValues[1]);   // C#
  //analogWriteQuantized(tuningValues[2]);   // D
  //analogWriteQuantized(tuningValues[3]);   // D#
  //analogWriteQuantized(tuningValues[4]);   // E
  // analogWriteQuantized(tuningValues[5]);   // F
  //analogWriteQuantized(tuningValues[6]);   // F#
  // analogWriteQuantized(tuningValues[7]);   // G
  //analogWriteQuantized(tuningValues[8]);   // G#
  // analogWriteQuantized(tuningValues[9]);   // A
  // analogWriteQuantized(tuningValues[10]);  // A#
  // analogWriteQuantized(tuningValues[11]);  // B
  
  //analogWriteQuantized(tuningValues[12]);  // C
  //analogWriteQuantized(tuningValues[13]);  //  C#
  //analogWriteQuantized(tuningValues[14]);  // D
  //analogWriteQuantized(tuningValues[15]);   // D#
  //analogWriteQuantized(tuningValues[16]);   // E
  //analogWriteQuantized(tuningValues[17]);   // F
  //analogWriteQuantized(tuningValues[18]);   // F#
  //analogWriteQuantized(tuningValues[19]);   // G
  //analogWriteQuantized(tuningValues[20]);   // G#
  //analogWriteQuantized(tuningValues[21]);   // A
  //analogWriteQuantized(tuningValues[22]);  // A#
  //analogWriteQuantized(tuningValues[23]);  // B

   //analogWriteQuantized(tuningValues[24]);  // C
  //analogWriteQuantized(tuningValues[25]);   //  C#
  //analogWriteQuantized(tuningValues[26]);   // D
  //analogWriteQuantized(tuningValues[27]);   // D#
  //analogWriteQuantized(tuningValues[28]);   // E
  //analogWriteQuantized(tuningValues[29]);   // F
  //analogWriteQuantized(tuningValues[30]);   // F#
  //analogWriteQuantized(tuningValues[31]);   // G
  //analogWriteQuantized(tuningValues[32]);   // G#
  //analogWriteQuantized(tuningValues[33]);   // A
  //analogWriteQuantized(tuningValues[34]);   // A#
  //analogWriteQuantized(tuningValues[35]);   // B

  //analogWriteQuantized(tuningValues[36]);   // C
}

void loop() {
  // Nothing here, static output for tuning
}

void setupFastPWM() {
  // --- F3 on D10 via Timer1 ---
  pinMode(pwmOutPin, OUTPUT);
  TCCR1A = _BV(COM1B1) | _BV(WGM10);  // Fast PWM 8-bit, non-inverting on OCR1B
  TCCR1B = _BV(WGM12) | _BV(CS10);    // No prescaler
  OCR1B = 0;
}

void analogWriteQuantized(uint8_t val) {
  OCR1B = val;  // Writes directly to PWM on F3 (D10)
}
