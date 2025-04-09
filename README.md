# **hgw-mod1-firmwares**

 Alternative firmwares for Hagiwos Mod1 Arduino based Module.
 https://note.com/solder_state/n/nc05d8e8fd311
 
 Mod1 is an Arduino Nano based eurorack module designed by Hagiwo, that can be utilized with own code.
 Here you will find some slightly changed or completely new code ideas for this module. 
 
 
 ![mod1](https://assets.st-note.com/production/uploads/images/166671260/rectangle_large_type_2_74d04b7593d4c5aa3a08d021646da297.jpeg) 
 
  # Current firmwares
 - Mod1 LFO
- 3chan LFO
- randomwalk
- bezier curve with spike/ jitter mode
- sample and hold with slew
- Enhanced Bernoulli Gate and Loop Sequencer with glitch bursts
- pot1 recorder 
  
 # Mod1 LFO
 Multi waveform LFO.\
 Instead of SawRevWave a random slope is used as 5th waveform. 
 SawRevWave is still in the code if you want to return to Hagiwos original code.\
 Selectable waveforms: Sine, Triangle, Square, Saw, Random Slope, MaxTable waveforms.  
 
- POT1 →frequency
- POT2 → waveform select
- POT3 → output level
- F1 → frequency CV in
- F2 → waveform CV in
- F3 → output level CV in
- F4 → output
- BUTTON → change frequency range
- LED → output
- EEPROM → Saves the frequency range when the button is pressed
 
 # 3chan LFO
 Three channel LFO.\
 Added a fourth waveform: Random slope.\
 Selectable waveforms: Triangle, Square, Sine, Random Slope chosen by a push button on D4.
 
- POT1 → LFO1 frequency
- POT2 → LFO2 frequency
- POT3 → LFO3 frequency
- F1 → frequency CV in (apply to all ch LFO freq)
- F2 → LFO1 output
- F3 → LFO2 output
- F4 → LFO3 output
- BUTTON → change waveform
- LED → LFO1 output
- EEPROM → Saves select waveform

 # randomwalk
Random Walk with Gravity Mode.\
Button toggles between classic Random Walk mode and Gravity Mode.\
Gravity Mode pulls the output slowly back to 0 over time.\
Rate, Bias/Offset, ChaosDepth can be controlled via pots. CV ins for Rate and ChaosDepth.\
Slow, evolving CV signals, offering both chaotic and stabilized output behaviors. 

Classic Random Walk Mode: Generates smooth, random CV output with adjustable step size.\
Gravity Mode: Adds a gradual pull back to zero, creating a drifting, self-centering effect.\

- Pot1 → Rate / Control the frequency of random steps.    
- Pot2 → Bias/Offset /Shifts the output up or down.
- Pot3 → ChaosDepth (step size of walk) / Adjust the step size from subtle drifts to wild jumps.
- Button → switch mode
- F1 → CV input / adds to Rate
- F3 → CV input / adds to ChaosDepth 
- F4 → Random Walk Output

 # bezier curve with spike/ jitter mode
Bezier curve random CV generator by Hagiwo adapted for Mod1 and added a second Spike/Jitter Bezier mode, accesible via button.\
Spike, jitter probability and spike length can be fine tuned in code.  I think i settled for nicely erratic not too random values in the spike mode.   

- Pot1 → freq
- Pot2 → curve
- Pot3 → dev
- F1 → freq CV in
- F2 → curve CV in
- F3 → dev CV in
- F4 → output
- LED → Pin 3
- Button → switch modes


 # sample and hold with slew
 Classical sample and hold function. Sample and hold output on F4, triggered by button or F1 trigger input.\
 If nothing is patched into sample input F2 an internal sample source is used.\
 Pot1 is a bias for the internal noise.\
 In the middle position truly random, fully clockwise shifts towards higher values, fully counter clockwise shifts towards lower values.\
 Pot3  is gain/level.\
 Slewed output on F3 with Pot 2 controling time constant of the slew, up to 1000 ms slew.
 
 - Pot1 → A0 noise bias
 - Pot2 → A1 time constant slew
 - Pot3 → A2 gain
-  F1 → A3 trigger in
-  F2 → A4 sample input
-  F3 → A5 slew output
-  F4 → D11 sample and hold output
-  LED → Pin 3
-  Button → Pin 4 trigger
 
 # Enhanced Bernoulli Gate and Loop Sequencer with glitch bursts
This firmware combines a probabilistic Bernoulli gate with a step sequencer, providing flexible trigger manipulation and glitchy bursts for
randomness, controlled probabilities, and rhythmic chaos.

Controls:\
Pot 1 Probability\
Sets the probability of switching between the two outputs (F3 and F4).
Fully counterclockwise: Always output F3.
Fully clockwise: Always output F4.
Intermediate positions introduce a random chance of either output based on the pot setting.

Pot 2 Mode/Length\
Selects between Bernoulli Mode and Step Sequence Modes:
0-20% (CCW): Bernoulli Mode (random coin toss per trigger)
20-40%: 4-Step Sequence
40-60%: 8-Step Sequence
60-80%: 16-Step Sequence
80-100% (CW): 32-Step Sequence
In Step Sequence Modes, pressing the button generates a new randomized sequence.

Pot 3 Miss Probability and Glitch Burst\
Sets the probability of skipping triggers:
Fully counterclockwise: No triggers are missed.
Turn clockwise: Up to 35% of triggers are missed. Linear 0-35%.
80-100% (CW): Introduces Glitch Bursts, where rapid chaotic trigger sequences are generated occasionally.

Inputs and Outputs:\
F1 Trigger Input\
Incoming trigger signal to be processed through the Bernoulli gate or sequencer.\
F2 CV Input\
Modulates the output probability in conjunction with Pot 1, allowing CV control.\
F3 Output 1\
One of the two output trigger channels, chosen based on the current probability setting.\
F4 Output 2\
The second output trigger channel, acting as a counterpart to F3.\

Button\
Pressing the button in Sequence Mode regenerates the step sequence with a new randomized pattern.
In Bernoulli Mode, the button has no effect.

LED Indicator\
Flashes on every valid trigger output (either F3 or F4).

This combination of probability control, step sequencing, trigger skipping, and glitch bursts makes the module versatile for generative rhythms, random variations, and chaotic textures. 
 
 
# pot1 recorder 

Pot1 Recorder with Fast PWM Output (100 Hz S&H style) max recording time 7.5 sec.\
S&H style recording and playback in order to squeeze out a useful recording time of the nano’s small SRAM.\
You can record movement of pot1 while holding down the button. Once let go it will loop the recording.\
Pot2 is speed, from half to double speed.\
Pot3 is gain control.\
Speed CV offset via F1 CV in.  Gain CV offset via F2 CV in.\
CV out on F4. 

- Pot1 → Pot to record
- Pot2 → Speed control of recording
- Pot3 → Output gain
- F1 → Speed offset CV in 0-5 V
- F2 → Gain offset CV in 0-5 V
- F3 → /
- F4 → Output
- LED → Visualize Output
- Button → Recording button

 
 
 
 
# mod1 general Hardware Configuration
Potentiometers
- Pot1  → A0
- Pot2  → A1
- Pot3  → A2

Inputs/Outputs:
- F1    → A3/ D17  in/output
- F2    → A4/ D9   in/output 
- F3    → A5/ D10  in/output 
- F4    → D11 in/output 

- LED    → Pin 3
- Button → Pin 4


 
 
 
 
 
 
 
