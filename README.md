# **hgw-mod1-firmwares**

 Alternative firmwares for Hagiwos Mod1 Arduino based Module.
 https://note.com/solder_state/n/nc05d8e8fd311
 
 Mod1 is an Arduino Nano based eurorack module designed by Hagiwo, that can be utilized with own code.
 Here you will find some slightly changed or completely new code ideas for this module. 
 
 
 ![mod1](https://assets.st-note.com/production/uploads/images/166671260/rectangle_large_type_2_74d04b7593d4c5aa3a08d021646da297.jpeg) 
 
 # Mod1 LFO
 Insted of SawRevWave a random slope is used as 5th waveform. 
 SawRevWave is still in the code if you want to return to Hagiwos original code. 
 Sine, Triangle, Square, Saw, Random Slope, MaxTable waveforms.  
 
 # 3chan LFO
 Added a fourth waveform: Random slope. 
 Selectable waveforms (Triangle, Square, Sine, Random Slope) chosen by a push button on D4.

 # randomwalk
Random Walk with Gravity Mode. Button toggles between classic Random Walk mode and Gravity Mode.
Gravity Mode pulls the output slowly back to 0 over time. Rate, Bias/Offset, ChaosDepth can be controlled via pots. CV ins for Rate and ChaosDepth . 

 # bezier curve with spike/ jitter mode
Bezier curve random CV generator by Hagiwo adapted for Mod1 and added a second Spike/Jitter Bezier mode, accesible via button. Spike, jitter probability and spike length can be fine tuned in code.  I think i settled for nicely erratic not too random values in the spike mode.   

 # sample and hold with slew
 Classical sample and hold function. Sample and hold output on F4, if nothing is patched into sample input F2 an internal sample source is used. With pot 1 you can choose internal noise source.  
 Slewed output on F3, Pot 2 controls time constant of the slew. Button and F1 input are used as triggers. 
 To Do: Currently slewed output is very jittery, should be stabilized.   
 
## mod1 general Hardware Configuration
Potentiometers
- Potentiometer 1  → A0
- Potentiometer 2  → A1
- Potentiometer 3  → A2

Inputs/Outputs:
- F1    → A3/ D17  in/output
- F2    → A4/ D9   in/output 
- F3    → A5/ D10  in/output 
- F4    → D11 in/output 

- LED    → Pin 3
- Button → Pin 4


 
 
 
 
 
 
 
