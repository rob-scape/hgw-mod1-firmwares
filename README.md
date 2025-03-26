# hgw-mod1-firmwares

 Alternative firmwares for Hagiwos Mod1 Arduino based Module.
 https://note.com/solder_state/n/nc05d8e8fd311
 
 Mod1 is an Arduino Nano based eurorack module designed by Hagiwo, that can be utilized with own code.
 Here you will find some slightly changed or completely new code ideas for this module. 
 
 #Mod1 LFO
 Insted of SawRevWave a random slope is used as 5th waveform. 
 SawRevWave is still in the code if you want to return to Hagiwos original code. 
 
 #3chan LFO
 Added a fourth waveform: Random slope. 
 Selectable waveforms (Triangle, Square, Sine, Random Slope) chosen by a push button on D4 (INPUT_PULLUP).

 #randomwalk
Random Walk with Gravity Mode. Button toggles between classic Random Walk mode and Gravity Mode.


 
  #mod1 general Hardware Configuration
Potentiometers
- Potentiometer 1  → A0
- Potentiometer 2  → A1
- Potentiometer 3  → A2

Inputs/Outputs:
- F1    → A3  in/output
- F2    → A4  in/output 
- F3    → A5  in/output 
- F4    → D11 in/output 

- LED    → Pin 3
- Button → Pin 4


 
 
 
 
 
 
 
