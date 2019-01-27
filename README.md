# vgm2smps

A tool that converts VGM to SMPS (Sonic 1) that was made to help you port music from other games or from your own VGMs.

## Features
  -Easy for use.
  
  
  -DAC sequence detecting (only for optimized VGMs with data banks as Defle Mask or SMPSPlay exports).
  
  
  -DAC samples export.
  
  
  -Ability to set exported channels and song start offset and its length.
  
  
  -Ability to set parsing speed in FPS.
  
  
  -Alternate "smart" instrument detecting mode. If final note's frequency is lower, than SMPS supports, all notes transposes in octave up,
  but instrument's multiply transpose down (make instrument lower).


## *Known bugs* 
  -Sometimes (but rarely) notes may be wrong (frequencies). 
  
  
  -Sometimes notes are wrong if -altins is used. 
  
  
  -Sometimes (but rarely) notes length may be detected wrong.
