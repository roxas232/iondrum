# iondrum
Arduino code to use a PS3 ION Drum Rocker as a standalone electric drum kit.

Small modifications were made to the USB Host Shield library to let an ION Drum Rocker be processed as a PS3 controller.

# Parts List
* Arduino Mega 2560
* Sparkfun USB Host Shield
  * https://www.sparkfun.com/products/9947
* Wav Trigger
  * https://www.sparkfun.com/products/13660
* OLED 128x64 Display
  * Other sizes could work, but would require some code tweaks
* Micro USB breakout board
* MicroSD card (just has to be big enough to hold all the WAV drum samples you plan to use)
* PS3 Ion Drum Rocker

# Wiring Guide
* Add male headers to the bottom of the USB Host Shield
* Add male headers to the Micro USB breakout board and solder it to the holes in the bottom right corner of the USB Host Shield (so it won't interfere with the Arduino USB port)
* Solder a jumper wire from the Micro USB breakout board's VCC to the USB Host Shield's VCC, and the same for ground
* Solder a jumper wire from pin 7 on the USB Host Shield to the RST pin
* Run jumpers from the USB Host Shield to the Arduino pins 11 -> 51, 12 -> 50, and 13 -> 52
* Add male headers to the Wav Trigger's serial inputs
* Run VCC and Ground from the Micro USB breakout to both the Wav Trigger and the OLED Display
* Run SDA and SCL from the Arduino to the OLED display
* Run pin 30 from the Arduino to the OLED display reset pin
* Run TX and RX to the opposite pins on the Wav Trigger

# MicroSD card setup
Each track must be duplicated twice per kit because this reduces the volume spikes if you vary the intensity of your hits.

ex. 001_mycrash.wav and 009_mycrash.wav

* CRASH_TRACK   1
* HIHAT_TRACK   2
* HITOM_TRACK   3
* KICK_TRACK    4
* LOWTOM_TRACK  5
* MIDTOM_TRACK  6
* RIDE_TRACK    7
* SNARE_TRACK   8

To add more kits, continue following this pattern with the next set of higher numbers.

See youtube.com/watch?v=B9iIxnMUk7E&feature=youtu.be for details on how to convert the files to the appropriate format.

Put the MicroSD card in the Wav Trigger

# Programming the Arduino
* Copy the sub-folders in the included libraries folder into your Arduino libraries folder (for Windows by default this is in your My Documents/Arduino folder)
* Load the PS3USB.ino file into the Arduino IDE
* Select Tools -> Arduino/Genuino Mega or Mega 2560
* Plug in both USB cables to your computer
* Ensure the correct COM port is selected under the Tools menu
* Modify the NUM_KITS and kitNames values to reflect your setup
* Click the Upload (->) button to put the code on the Arduino
* Plug in your Ion Drum Rocker USB cable into the USB Host Shield and headphones into the 1/8" jack on the Wav Trigger
* Enjoy!



