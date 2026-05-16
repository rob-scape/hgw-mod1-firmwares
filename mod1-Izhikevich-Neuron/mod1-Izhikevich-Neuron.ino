/*
Izhikevich Neuron Model - Neuron Spiking LFO
A model that reproduces 20+ different neuron firing patterns.

The model has two variables:
- v (membrane potential): Fast spiking variable
- u (recovery variable): Slow adaptation variable

Four parameters (a, b, c, d) control the neuron's "personality":
- a: recovery time scale (how fast u recovers)
- b: sensitivity of u to v (coupling strength)
- c: after-spike reset value for v
- d: after-spike reset increment for u

Pots:
  A0 → a (recovery speed / 0.01-0.2: faster recovery = different spike patterns)
  A1 → b (sensitivity / 0.1-0.3: changes spike clustering behavior)
  A2 → I (input current / 0-15: drives activity from rest to chaos)

Input:
  F1 (A3 / D17) → CV input (modulates input current I, -5 to +10 offset to Pot C)

Outputs:
  D9 (F2)  → v (membrane potential - main spiky chaotic output)
  D10 (F3) → u (recovery variable - slower complementary wave)
  D11 (F4) → spike gate (HIGH during spike, LOW otherwise). Biological timing rather than metronomic timing. Irregular but not random.

Button (D4) → Cycle through neuron types (Regular/Chattering/Bursting/Fast Spiking)
LED (D3)    → Shows neuron type: 1 blink=Regular, 2=Chattering, 3=Bursting, 4=Fast
*/

#include <EEPROM.h>
#define EEPROM_ADDR 0

const int potAPin = A0;  // a parameter
const int potBPin = A1;  // b parameter  
const int potIPin = A2;  // I (input current)

const int cvInputPin = A3;  // F1 CV input
const int pinF2 = 9;   // OC1A - v output
const int pinF3 = 10;  // OC1B - u output  
const int pinF4 = 11;  // OC2A - spike gate output

const int ledPin = 3;
const int buttonPin = 4;

// Neuron type selection (0-3)
int neuronType = 0;  // 0=Regular, 1=Chattering, 2=Bursting, 3=Fast Spiking
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Izhikevich variables
float v = -65.0;  // membrane potential (starts at resting)
float u = 0.0;    // recovery variable
float dt = 0.5;   // time step (milliseconds)

// Parameters c and d based on neuron type
float c = -65.0;  // after-spike reset for v
float d = 2.0;    // after-spike reset increment for u

// Spike detection
bool inSpike = false;
unsigned long lastSpikeTime = 0;

// LED blink pattern for showing neuron type
unsigned long ledBlinkStart = 0;
int ledBlinkCount = 0;
bool showingType = false;

void setup() {
  Serial.begin(9600);
  
  // Read saved neuron type from EEPROM
  neuronType = EEPROM.read(EEPROM_ADDR);
  if (neuronType > 3) neuronType = 0;  // Validate
  
  pinMode(potAPin, INPUT);
  pinMode(potBPin, INPUT);
  pinMode(potIPin, INPUT);
  
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(cvInputPin, INPUT);
  
  pinMode(pinF2, OUTPUT);
  pinMode(pinF3, OUTPUT);
  pinMode(pinF4, OUTPUT);
  
  // Timer1 (16-bit) PWM Setup for F2 (D9) & F3 (D10)
  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM10);
  TCCR1B = _BV(WGM12) | _BV(CS10); // ~31.4kHz PWM
  
  // Timer2 (8-bit) PWM Setup for F4 (D11)
  TCCR2A = _BV(COM2A1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS21);
  
  OCR1A = 128;
  OCR1B = 128;
  OCR2A = 0;  // Start with gate low
  
  updateNeuronType();
  
  Serial.print("Izhikevich Neuron initialized. Type: ");
  printNeuronType();
}

void updateNeuronType() {
  // Set c and d parameters based on neuron type
  // These create different "personalities"
  switch(neuronType) {
    case 0:  // Regular Spiking
      c = -65.0;
      d = 8.0;
      break;
    case 1:  // Chattering (fast bursts)
      c = -50.0;
      d = 2.0;
      break;
    case 2:  // Intrinsically Bursting
      c = -55.0;
      d = 4.0;
      break;
    case 3:  // Fast Spiking
      c = -65.0;
      d = 2.0;
      break;
  }
}

void printNeuronType() {
  switch(neuronType) {
    case 0: Serial.println("Regular Spiking"); break;
    case 1: Serial.println("Chattering"); break;
    case 2: Serial.println("Intrinsically Bursting"); break;
    case 3: Serial.println("Fast Spiking"); break;
  }
}

void startTypeBlink() {
  showingType = true;
  ledBlinkStart = millis();
  ledBlinkCount = 0;
}

void updateTypeBlink() {
  if (!showingType) return;
  
  unsigned long elapsed = millis() - ledBlinkStart;
  int blinkPhase = (elapsed / 200) % 2;  // 200ms on/off cycles
  int currentBlink = elapsed / 400;  // Which blink we're on
  
  if (currentBlink < neuronType + 1) {
    digitalWrite(ledPin, blinkPhase == 0 ? HIGH : LOW);
  } else if (elapsed > (neuronType + 1) * 400 + 1000) {
    // Done showing type after blinking + 1 second pause
    showingType = false;
    digitalWrite(ledPin, LOW);
  } else {
    digitalWrite(ledPin, LOW);  // Pause after blinks
  }
}

void loop() {
  // Button handling for neuron type cycling
  bool buttonState = digitalRead(buttonPin);
  
  if (buttonState != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    static bool lastStableState = HIGH;
    
    if (buttonState != lastStableState) {
      lastStableState = buttonState;
      
      if (buttonState == LOW) {
        neuronType = (neuronType + 1) % 4;
        EEPROM.write(EEPROM_ADDR, neuronType);
        updateNeuronType();
        startTypeBlink();
        Serial.print("Neuron type: ");
        printNeuronType();
      }
    }
  }
  
  lastButtonState = buttonState;
  
  // Update LED blink pattern if showing type
  if (showingType) {
    updateTypeBlink();
  }
  
  // F1 as CV input - modulates input current
  // TUNE HERE: CV offset range (wide range for dramatic modulation)
  float cvOffset = mapFloat(analogRead(cvInputPin), 0, 1023, -5.0, 10.0);
  
  // Read pots and map to Izhikevich parameters
  // TUNE HERE: a parameter (recovery time scale)
  float a = mapFloat(analogRead(potAPin), 0, 1023, 0.02, 0.2);
  
  // TUNE HERE: b parameter (sensitivity of u to v)
  float b = mapFloat(analogRead(potBPin), 0, 1023, 0.1, 0.3);
  
  // TUNE HERE: I parameter (input current - drives spiking)
  float I_base = mapFloat(analogRead(potIPin), 0, 1023, 0.0, 15.0);
  
  float I = I_base + cvOffset;  // Add CV offset from F1
  
  // TUNE HERE: I constraint range
  I = constrain(I, -5.0, 25.0);
  
  // Izhikevich neuron equations
  // dv/dt = 0.04*v² + 5*v + 140 - u + I
  // du/dt = a*(b*v - u)
  
  float dv = 0.04 * v * v + 5.0 * v + 140.0 - u + I;
  float du = a * (b * v - u);
  
  // Euler integration
  v += dv * dt;
  u += du * dt;
  
  // Spike reset mechanism (this is key to Izhikevich!)
  if (v >= 30.0) {  // Spike threshold
    v = c;  // Reset v to post-spike value
    u = u + d;  // Increment u by reset amount
    inSpike = true;
    lastSpikeTime = millis();
    
    if (!showingType) {
      digitalWrite(ledPin, HIGH);
    }
  }
  
  // Clear spike flag after brief period
  if (inSpike && millis() - lastSpikeTime > 2) {
    inSpike = false;
    if (!showingType) {
      digitalWrite(ledPin, LOW);
    }
  }
  
  // Constrain variables to reasonable ranges
  v = constrain(v, -80.0, 30.0);
  u = constrain(u, -20.0, 20.0);
  
  // Map to PWM range
  // TUNE HERE: Output scaling ranges
  // v ranges from -80 (rest) to +30 (spike)
  int pwmV = constrain(mapFloat(v, -80, 30, 0, 255), 0, 255);
  
  // u ranges roughly -20 to +20
  int pwmU = constrain(mapFloat(u, -20, 20, 0, 255), 0, 255);
  
  // Spike gate output (full high during spike, zero otherwise)
  int pwmGate = inSpike ? 255 : 0;
  
  // Output PWM signals
  OCR1A = pwmV;    // F2 - v (membrane potential)
  OCR1B = pwmU;    // F3 - u (recovery variable)
  OCR2A = pwmGate; // F4 - spike gate
  
  lastSpikeTime = millis();
}

// Helper function to map float values
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}