# vgm2smps

A tool that converts VGM to SMPS (Sonic 1) that was made to help you port music from other games or from your own VGMs.

## Features
  - Easy to use.
  - DAC sequence detecting (only for optimized VGMs with data banks e.g. Furnace, DefleMask or SMPSPlay exports).
  - DAC samples export.
  - Ability to choose exported channels and song start offset and its length.
  - Ability to set parsing speed in FPS.
  - Alternate "smart" instrument detecting mode. If final note's frequency is lower, than SMPS supports, all notes transposes in octave up,
  but instrument's multiply transpose down (make instrument lower).


## *Known bugs* 
  - Sometimes (but rarely) notes may be wrong (frequencies). 
  - Sometimes notes are wrong if `-altins` is used. 
  - Sometimes (but rarely) notes length may be detected wrong.

## Project status

> [!WARNING]
> This version of the project is no longer supported, and a new, enhanced version is currently under private development.
> This means I don't plan to support this project further. However, you can still build it or download the latest release.