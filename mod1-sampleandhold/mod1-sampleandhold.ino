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
    TCCR1A = _BV(COM1A1) | _BV(WGM10) | _BV(WGM11); // Fast PWM
    TCCR1B = _BV(CS11); // Prescaler 8
}

void loop() {
    // Read Button State
    buttonState = digitalRead(BUTTON_PIN) == LOW;
    
    // Read External Trigger (F1) - Rising edge detection
    bool currentF1State = analogRead(F1_PIN) > 512;  // Consider above 2.5V as "high"
    bool trigger = (currentF1State && !lastF1State);  // Rising edge detected

    // Check Sample Input
    int sampleInput = analogRead(F2_PIN);
    bool externalSample = sampleInput > 20; // Threshold to detect cable plugged in
    
    // Gain Control
    float gain = analogRead(POT_GAIN) / 1023.0;
    
    // Slew Time Control
    int slewTime = map(analogRead(POT_SLEW), 0, 1023, 1, 50); // Adjust range as needed

    // New Sample if Button Pressed or Rising Edge Trigger
    if ((buttonState && !lastButtonState) || trigger) {
        digitalWrite(LED_PIN, HIGH);
        if (externalSample) {
            heldValue = sampleInput * gain; // Use external input
        } else {
            heldValue = random(1024) * gain; // Use random noise
        }
        analogWrite(F4_PIN, heldValue / 4); // Convert 10-bit to 8-bit PWM
        delay(10);
        digitalWrite(LED_PIN, LOW);
    }

    // Slew Calculation
    int delta = heldValue - slewValue;
    int step = constrain(abs(delta) / 10, 1, 20); // Step size based on difference

    // Adjust slewValue with steps
    if (delta > 0) {
        slewValue += step; // Increase if the target is greater
    } else if (delta < 0) {
        slewValue -= step; // Decrease if the target is smaller
    }

    // Low-pass filter implementation for smoothing
    if (abs(slewValue - heldValue) < 10) { // Slightly increase the threshold
        slewValue = (slewValue + heldValue) / 2; // Apply low-pass filter
    }

    // Output the result to F3
    analogWrite(F3_PIN, slewValue / 4); // Convert 10-bit to 8-bit PWM
    delay(slewTime); // Use delay based on the time constant (for smooth transition)

    // Save current F1 state for next loop
    lastF1State = currentF1State;

    // Save button state for next loop
    lastButtonState = buttonState;
}
