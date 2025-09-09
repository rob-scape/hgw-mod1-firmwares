// This code was originally written by HAGIWO and released under CC0
/* Adapted for Mod1 and added a second Spike/Jitter Bezier mode, accesible via button by Rob Heel 
Potentiometers
    Potentiometer 1 → A0 freq
    Potentiometer 2 → A1 curve
    Potentiometer 3 → A2 dev
Inputs/Outputs:
    F1 → A3 freq CV in
    F2 → A4 curve CV in
    F3 → A5 dev CV in
    F4 → D11 output
    LED → Pin 3
    Button → Pin 4 switch modes

*/

#include <avr/io.h>  //for fast PWM
int i = 0;
int start_val = 0;  // Bezier Curve Starting Point
int end_val = 255;  // Bezier Curve end Point
float old_wait = 0;
float wait = 0;    // Bezier curve x-axis (time)
float bz_val = 0;  // Bezier curve y-axis (voltage)
int dev, level, curve, freq;
long timer = 0;   // Time tracking for micros
long timer1 = 0;  // Time tracking for analog read interval
float x[256];     // Bezier Curve Calculation Tables

int devpot = 0;
int devCV = 0;

int freq_rnd = 501;
int freq_dev = 40;
int chance[32] = { 5, 12, 21, 33, 48, 67, 90, 118, 151, 189, 232, 279, 331, 386, 443, 501, 559, 616, 671, 723, 770, 813, 851, 884, 912, 935, 954, 969, 981, 990, 997, 1000 };  // normal distribution table
int freq_err[32] = { 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 20, 22, 24, 26, 28, 30, 33, 36, 40, 46, 52, 58, 64, 70, 76, 82, 90, 98, 110, 122, 136, 148 };                       // Frequency Variation

// Add a new mode variable for Bezier and SPIKE  Bezier
enum Mode {
  BEZIER_MODE,
  SPIKE_BEZIER_MODE
};
Mode currentMode = BEZIER_MODE;  // Start with Bezier Mode

// Variables for the spike and jitter effect
int spike_hold_counter = 0;  // Counter to hold the spike for a few cycles
int spike_value = 0;         // The intensity of the spike (random value)

void setup() {
  for (int j = 0; j < 255; j++) {  // Preparation of Bezier Curve Calculation Tables
    x[j] = j * 0.003921;           // j/255
  }

  pinMode(11, OUTPUT);       // CV output
  pinMode(3, OUTPUT);        // LED output
  pinMode(4, INPUT_PULLUP);  // Button input (Pin 4)
  timer = micros();
  timer1 = millis();

  // pin pwm setting
  TCCR1B &= B11111000;
  TCCR1B |= B00000001;
  delay(50);
}

void loop() {
  // Toggle modes with the button
  if (digitalRead(4) == LOW) {
    delay(200);                                   // debounce delay
    currentMode = (Mode)((currentMode + 1) % 2);  // Toggle between 2 modes
  }

  if (timer1 + 50 < millis()) {
    freq = min(511, (analogRead(0) / 2 + analogRead(3) / 2)) * freq_dev;
    curve = min(255, (analogRead(1) / 4 + analogRead(4) / 4));
    level = 1023 / 4;
    timer1 = millis();
  }

  if (timer + (wait - old_wait) <= micros()) {
    old_wait = wait;
    i++;

    if (i >= 255) {  // Recalculation of target voltage values
      i = 0;
      start_val = end_val;
      end_val = random(0, 255);
      change_freq_error();
    }

    switch (currentMode) {
      case BEZIER_MODE:
        // Standard Bezier calculation
        wait = 3 * pow((1 - x[i]), 2) * x[i] * curve + 3 * (1 - x[i]) * pow(x[i], 2) * (255 - curve) + pow(x[i], 3) * 255;
        wait = 1 + wait * freq * 2;
        bz_val = pow((1 - x[i]), 3) * start_val + 3 * pow((1 - x[i]), 2) * x[i] * start_val + 3 * (1 - x[i]) * pow(x[i], 2) * end_val + pow(x[i], 3) * end_val;
        break;

      case SPIKE_BEZIER_MODE:
        // Basic Bezier curve calculations
        wait = 3 * pow((1 - x[i]), 2) * x[i] * curve + 3 * (1 - x[i]) * pow(x[i], 2) * (255 - curve) + pow(x[i], 3) * 255;
        wait = 1 + wait * freq * 2;  // Frequency influence

        // Basic Bezier transition for the value
        bz_val = pow((1 - x[i]), 3) * start_val + 3 * pow((1 - x[i]), 2) * x[i] * start_val + 3 * (1 - x[i]) * pow(x[i], 2) * end_val + pow(x[i], 3) * end_val;

        // Spike trigger: 5% chance to trigger a spike
        if (random(0, 100) < 5 && spike_hold_counter == 0) {
          // Start the spike hold, random spike size
          spike_hold_counter = random(5, 20);  // Hold spike for 5 to 20 cycles
          spike_value = random(-50, 50);       // Randomize spike intensity
        }

        // If spike is active, hold the spike for a few cycles
        if (spike_hold_counter > 0) {
          bz_val += spike_value;  // Apply spike value
          spike_hold_counter--;   // Decrease the counter, spike will fade out
        }

        // Add jitter effect with 15% chance
        if (random(0, 100) < 15) {    // 15% chance for jitter effect
          bz_val += random(-20, 20);  // Small random jitter in the output
        }

        // Smooth the output to avoid very harsh jumps
        bz_val = constrain(bz_val, 0, 255);  // Keep the value within bounds

        // Set the output voltage (scaled to level)
        analogWrite(11, bz_val * level / 255);

        // Change the values when a full cycle is completed
        if (i >= 255) {
          i = 0;
          start_val = end_val;
          end_val = random(0, 255);
          change_freq_error();  // Keep the frequency variation
        }
        break;
    }

    timer = micros();
    PWM_OUT();  // Output the PWM signal
  }
}  // end void loop

void change_freq_error() {  // Frequency variation is obtained from the standard deviation table

  devpot = map(analogRead(2), 0, 1023, 0, 500);
  devCV = map(analogRead(4), 0, 1023, 0, 500);
// Combine both values and map back to range 0-500
  int dev = map(devpot + devCV, 0, 1000, 0, 500);  
  freq_rnd = random(500 - dev, 500 + dev);
  for (int k = 0; k < 32; k++) {
    if (freq_rnd >= chance[k] && freq_rnd < chance[k + 1]) {
      freq_dev = freq_err[k];
    }
  }
}

void PWM_OUT() {  // PWM output
  int output_value = bz_val * level / 255;
  analogWrite(11, bz_val * level / 255);
  analogWrite(3, output_value);   // LED output - mirrors the CV intensity
}
