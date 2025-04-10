/*
// Sample and Hold with Slew for Hagiwos Mod1 by Rob Heel.
 Classical sample and hold function. Sample and hold output on F4, triggered by button or F1 trigger in. 
 If nothing is patched into sample input F2 an internal sample source is used. 
 Pot1 is a bias for the internal noise. In the middle position truly random, fully clockwise shifts towards higher values, 
 fully counter clockwise shifts towards lower values. Pot3  is gain/level. 
 Slewed output on F3 with Pot 2 controling time constant of the slew.

Potentiometers
    Potentiometer 1 → A0 noise bias
    Potentiometer 2 → A1 time constant slew
    Potentiometer 3 → A2 gain
Inputs/Outputs:
    F1 → A3 trigger in
    F2 → A4 sample input
    F3 → A5 slew output
    F4 → D11 sample and hold output
    LED → Pin 3
    Button → Pin 4 trigger

*/


#include <Arduino.h>

// Pin Definitions
#define BUTTON_PIN 4
#define LED_PIN 3
#define F1_PIN A3
#define F2_PIN A4
#define F3_PIN 10 // Slew Output (Fast PWM)
#define F4_PIN 11 // PWM Output
#define POT_GAIN A2
#define POT_SLEW A1

int heldValue = 0;
bool buttonState = false;
bool lastButtonState = false;
int slewValue = 0;

bool lastF1State = LOW;  // Store previous state of F1 (for rising edge detection)

void setup() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    pinMode(F1_PIN, INPUT);
    pinMode(F2_PIN, INPUT);
    pinMode(F3_PIN, OUTPUT);
    pinMode(F4_PIN, OUTPUT);
    
    // Set up PWM on D11 (F4)
    TCCR2A = _BV(COM2A1) | _BV(WGM20) | _BV(WGM21); // Fast PWM
    TCCR2B = _BV(CS21); // Prescaler 8

    // Set up PWM on D10 (F3) - Fast PWM
    //TCCR1A = _BV(COM1A1) | _BV(WGM10) | _BV(WGM11); // Fast PWM
   // TCCR1B = _BV(CS11); // Prescaler 8

    TCCR1A = _BV(COM1A1) | _BV(WGM10);        // Enable PWM on OC1A (D10), WGM10 for 8-bit
    TCCR1B = _BV(WGM12) | _BV(CS11);          // Fast PWM 8-bit, prescaler 8

}

void loop() {
    // Read Button State
    buttonState = digitalRead(BUTTON_PIN) == LOW;
    
    // Read External Trigger (F1) - Rising edge detection
    //bool currentF1State = analogRead(F1_PIN) > 512;  // Consider above 2.5V as "high" 512
    bool currentF1State = digitalRead(F1_PIN); // digital read as trigger detect
    bool trigger = (currentF1State && !lastF1State);  // Rising edge detected

    // Check Sample Input
    int sampleInput = analogRead(F2_PIN);
    bool externalSample = sampleInput > 20; // Threshold to detect cable plugged in
    
    // Gain Control
    float gain = analogRead(POT_GAIN) / 1023.0;
    
    // Slew Time Control
    int slewTime = map(analogRead(POT_SLEW), 0, 1023, 1, 50); // Adjust range as needed

    // Read Potentiometer 1 (A0) to influence random value distribution
    int pot1Value = analogRead(A0);
    int potBias = map(pot1Value, 0, 1023, -500, 500); // Map pot value to range of -500 to +500

    // New Sample if Button Pressed or Rising Edge Trigger
    if ((buttonState && !lastButtonState) || trigger) {
        digitalWrite(LED_PIN, HIGH);
        if (externalSample) {
            heldValue = sampleInput * gain; // Use external input
        } else {
            // Generate random value influenced by Potentiometer 1
            int randomValue = random(1024);
            int biasedValue = randomValue + potBias;

            // Clamp the biased value to the range 0 to 1023
            biasedValue = constrain(biasedValue, 0, 1023);

            heldValue = biasedValue * gain;
        }
        analogWrite(F4_PIN, heldValue / 4); // Convert 10-bit to 8-bit PWM
        delay(10);
        digitalWrite(LED_PIN, LOW);
    }


// Slew Calculation
static unsigned long slewStartTimeMicros = 0;
static int startValue = 0;
static int targetValue = 0;
static int slewDurationMicros = 0;
static bool slewing = false;

static unsigned long lastStepTime = 0;
static int steps = 1;
static float stepSize = 0;
static float intermediateValue = 0;

// Slew time from Pot2
int rawPot = analogRead(POT_SLEW);
float curved = pow((float)rawPot / 1023.0, 2.5); // Optional: exponential response
int currentSlewTime = constrain(curved * 1000, 10, 1000); // 10–1000ms
int totalTimeMicros = currentSlewTime * 1000;

// Start new slew
if (heldValue != targetValue) {
    startValue = slewValue;
    targetValue = heldValue;
    slewDurationMicros = totalTimeMicros;
    steps = max(1, currentSlewTime * 2); // 2 steps per ms = 0.5ms per step
    stepSize = (float)(targetValue - startValue) / steps;
    intermediateValue = startValue;
    slewStartTimeMicros = micros();
    lastStepTime = micros();
    slewing = true;
}

if (slewing) {
    unsigned long now = micros();
    if (now - lastStepTime >= 500) { // 0.5ms per step
        intermediateValue += stepSize;
        slewValue = round(intermediateValue);
        lastStepTime = now;

        if ((stepSize > 0 && slewValue >= targetValue) ||
            (stepSize < 0 && slewValue <= targetValue)) {
            slewValue = targetValue;
            slewing = false;
        }
    }
}

// Output to F3
analogWrite(F3_PIN, slewValue / 4); // 10-bit to 8-bit PWM


    // Save current F1 state for next loop
    lastF1State = currentF1State;

    // Save button state for next loop
    lastButtonState = buttonState;
}
