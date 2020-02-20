//#define RAW_PACKET_DEBUG 1

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define NUM_KITS 2
#define KIT_OFFSET 16

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     30 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#include <wavTrigger.h>

/*
 Example sketch for the PS3 USB library - developed by Kristian Lauszus
 For more information visit my blog: http://blog.tkjelectronics.dk/ or
 send me an e-mail:  kristianl@tkjelectronics.com
 */

#include <PS3USB.h>

// Satisfy the IDE, which needs to see the include statment in the ino too.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>

USB Usb;
/* You can create the instance of the class in two ways */
PS3USB PS3(&Usb); // This will just create the instance
//PS3USB PS3(&Usb,0x00,0x15,0x83,0x3D,0x0A,0x57); // This will also store the bluetooth address - this can be obtained from the dongle when running the sketch

bool collecting = true;
bool isKickDown = false;
uint8_t currentKit = 0;

uint8_t currentBuf[EP_MAXPKTSIZE];
uint8_t newBuf[EP_MAXPKTSIZE];

char intensityStr[30];

bool drumsActive = false;
uint8_t prevDpad = 0xFF;
uint8_t garbageReads = 0;

wavTrigger wTrig;
int masterGain = 0;

enum DrumColor {
  DBLU = 0x01,
  DGRE = 0x02,
  DRED = 0x04,
  DYEL = 0x08,
  DORG = 0x30
};

enum DrumType {
  DRUM = 0x04,
  CYM = 0x08
};


#define CRASH_TRACK   1
#define HIHAT_TRACK   2
#define HITOM_TRACK   3
#define KICK_TRACK    4
#define LOWTOM_TRACK  5
#define MIDTOM_TRACK  6
#define RIDE_TRACK    7
#define SNARE_TRACK   8

#define SECONDARY_OFFSET 8

#define MIN_GAIN      -2
#define MAX_GAIN      10

bool useOffsetForTrack[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

void printHex(int num) {
     char tmp[16];
     char format[128];

     sprintf(format, "%%.%dX", 2);

     sprintf(tmp, format, num);
     Serial.print(tmp);
}

void drawLastHit(uint8_t track, uint8_t trackToUse) {
  display.clearDisplay();
  display.setCursor(0, 0);     // Start at top-left corner
  
  switch (track)
  {
    case CRASH_TRACK:
      display.println("CRASH");
      break;
    case HIHAT_TRACK:
      display.println("HIHAT");
      break;
    case HITOM_TRACK:
      display.println("HITOM: ");
      break;
    case KICK_TRACK:
      display.println("KICK: ");
      break;
    case LOWTOM_TRACK:
      display.println("LOWTOM: ");
      break;
    case MIDTOM_TRACK:
      display.println("MIDTOM: ");
      break;
    case RIDE_TRACK:
      display.println("RIDE: ");
      break;
    case SNARE_TRACK:
      display.println("SNARE: ");
      break;
  }

  itoa(trackToUse, intensityStr, 10);
  display.println(intensityStr);
  display.display();
}

inline void playDrum(uint8_t track, uint8_t intensity)
{
  int triggerVol = map(intensity, 225, 64, MIN_GAIN, MAX_GAIN);
  uint8_t trackToUse = track + (currentKit * KIT_OFFSET);
  if (useOffsetForTrack[track - 1])
  {
    trackToUse += SECONDARY_OFFSET;
  }
  useOffsetForTrack[track - 1] = !useOffsetForTrack[track - 1];
  wTrig.trackPlayPoly(trackToUse);
  wTrig.trackGain(trackToUse, triggerVol);
  //drawLastHit(track, trackToUse);
}

inline void playMultiDrum(uint8_t track, uint8_t intensity)
{
  int triggerVol = map(intensity, 225, 64, MIN_GAIN, MAX_GAIN);
  uint8_t trackToUse = track + (currentKit * KIT_OFFSET);
  if (useOffsetForTrack[track - 1])
  {
    trackToUse += SECONDARY_OFFSET;
  }
  useOffsetForTrack[track - 1] = !useOffsetForTrack[track - 1];
  wTrig.trackLoad(trackToUse);
  wTrig.trackGain(trackToUse, triggerVol);
}

// Assume 0C value for drum/cymbal mask
void printDrumCombo(uint8_t color) {
  // DYEL + CBLU => FF at 10
  // CYEL + DBLU => FF at 9
  // DYEL + CGRE => None?
  // CYEL + DGRE => FF at 9
  // DBLU + CGRE => None?
  // CBLU + DGRE => FF at 10

  uint8_t redIntensity = currentBuf[12];
  uint8_t yellowIntensity = currentBuf[11];
  uint8_t blueIntensity = currentBuf[14];
  uint8_t greenIntensity = currentBuf[13];
  
  uint8_t flag9 = currentBuf[9];
  uint8_t flag10 = currentBuf[10];
  if (color == (DrumColor::DYEL | DrumColor::DBLU))
  {
    if (flag10 == 0xFF)
    {
      playMultiDrum(HITOM_TRACK, yellowIntensity);
      playMultiDrum(RIDE_TRACK, blueIntensity);
    }
    else // Could check flag9
    {
      playMultiDrum(HIHAT_TRACK, yellowIntensity);
      playMultiDrum(MIDTOM_TRACK, blueIntensity);
    }
  }
  else if (color == (DrumColor::DYEL | DrumColor::DGRE))
  {
    if (flag9 == 0xFF)
    {
      playMultiDrum(HIHAT_TRACK, yellowIntensity);
      playMultiDrum(LOWTOM_TRACK, greenIntensity);
    }
    else
    {
      playMultiDrum(HITOM_TRACK, yellowIntensity);
      playMultiDrum(CRASH_TRACK, greenIntensity);
    }
  }
  else if (color == (DrumColor::DBLU | DrumColor::DGRE))
  {
    if (flag10 == 0xFF)
    {
      playMultiDrum(RIDE_TRACK, blueIntensity);
      playMultiDrum(LOWTOM_TRACK, greenIntensity);
    }
    else
    {
      playMultiDrum(MIDTOM_TRACK, blueIntensity);
      playMultiDrum(CRASH_TRACK, greenIntensity);
    }
  }
  else if ((color & DrumColor::DRED) == DrumColor::DRED)
  {
    if ((color & DrumColor::DYEL) == DrumColor::DYEL)
    {
      playMultiDrum(SNARE_TRACK, redIntensity);
      playMultiDrum(HIHAT_TRACK, yellowIntensity);
    }
    else if ((color & DrumColor::DBLU) == DrumColor::DBLU)
    {
      playMultiDrum(SNARE_TRACK, redIntensity);
      playMultiDrum(RIDE_TRACK, blueIntensity);
    }
    else if ((color & DrumColor::DGRE) == DrumColor::DGRE)
    {
      playMultiDrum(SNARE_TRACK, redIntensity);
      playMultiDrum(CRASH_TRACK, greenIntensity);
    }
  }
  else
  {
    Serial.print("WTF\r\n");
  }

  wTrig.resumeAllInSync();
}

void printDrumsHit() {
  uint8_t color = currentBuf[0];
  uint8_t drumCym = currentBuf[1];
  uint8_t intensity;

  if (drumCym == (DrumType::DRUM | DrumType::CYM) 
    && color != DrumColor::DYEL && color != DrumColor::DBLU && color != DrumColor::DGRE)
  {
    printDrumCombo(color);
    return;
  }

  if ((color & DrumColor::DBLU) == DrumColor::DBLU)
  {
    if ((drumCym & DrumType::DRUM) == DrumType::DRUM)
    {
      intensity = currentBuf[14];
      playDrum(MIDTOM_TRACK, intensity);
    }
    if ((drumCym & DrumType::CYM) == DrumType::CYM)
    {
      intensity = currentBuf[14];
      playDrum(RIDE_TRACK, intensity);
    }
  }

  if ((color & DrumColor::DGRE) == DrumColor::DGRE)
  {
    if ((drumCym & DrumType::DRUM) == DrumType::DRUM)
    {
      intensity = currentBuf[13];
      playDrum(LOWTOM_TRACK, intensity);
    }
    if ((drumCym & DrumType::CYM) == DrumType::CYM)
    {
      intensity = currentBuf[13];
      playDrum(CRASH_TRACK, intensity);
    }
  }

  if ((color & DrumColor::DRED) == DrumColor::DRED)
  {
    if ((drumCym & DrumType::DRUM) == DrumType::DRUM)
    {
      intensity = currentBuf[12];
      playDrum(SNARE_TRACK, intensity);
    }
    if ((drumCym & DrumType::CYM) == DrumType::CYM)
    {
      Serial.print(" FAKE cymbal?\r\n");
    }
  }

  if ((color & DrumColor::DYEL) == DrumColor::DYEL)
  {
    if ((drumCym & DrumType::DRUM) == DrumType::DRUM)
    {
      intensity = currentBuf[11];
      playDrum(HITOM_TRACK, intensity);
    }
    if ((drumCym & DrumType::CYM) == DrumType::CYM)
    {
      intensity = currentBuf[11];
      playDrum(HIHAT_TRACK, intensity);
    }
  }
}

void printRawBuf()
{ 
  for (uint8_t i = 0; i < 17; ++i)
  {
    printHex(currentBuf[i]);
    Serial.print(" ");
  }
  Serial.print("\r\n");
}

void setup() {
  Serial.begin(115200);

  // Init the OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3D);

  // Clear the buffer
  display.clearDisplay();

  display.setTextSize(2);      // Draw 2X-scale text
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  
#if !defined(__MIPSEL__)
  //while (!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
#endif
  if (Usb.Init() == -1) {
    Serial.print(F("\r\nOSC did not start"));
    display.clearDisplay();
    display.println("USB FAILURE");
    display.display();
    while (1); //halt
  }
  Serial.print(F("\r\nPS3 USB Library Started"));
  display.clearDisplay();
  display.println();
  display.println("USB");
  display.println("ACTIVE");
  display.display();

  // If the Arduino is powering the WAV Trigger, we should wait for the WAV
  //  Trigger to finish reset before trying to send commands.
  delay(1000);

  // WAV Trigger startup at 57600
  wTrig.start();
  delay(10);

  // Send a stop-all command and reset the sample-rate offset, in case we have
  //  reset while the WAV Trigger was already playing.
  wTrig.stopAllTracks();
  wTrig.samplerateOffset(0);  
}
void loop() {
  Usb.Task();


  if (PS3.PS3Connected || PS3.PS3NavigationConnected) {
    // void PS3USB::getRawBuffer(uint8_t* buf) {
     //memcpy(buf, readBuf, EP_MAXPKTSIZE);
    //}

    if (!drumsActive)
    {
      drumsActive = true;
      display.clearDisplay();
      display.setCursor(0, 0);     // Start at top-left corner
      display.println();
      display.println("DRUMS");
      display.println("ACTIVE");
      display.display();
      delay(250);
    }

    PS3.getRawBuffer(newBuf);
    if (memcmp(currentBuf, newBuf, EP_MAXPKTSIZE) != 0)
    {
      #ifndef RAW_PACKET_DEBUG
      if (garbageReads < 4)
      {
        ++garbageReads;
        return;
      }
      // Handle gain changes
      uint8_t dpad = newBuf[2];
      // Dpad keyup
      if (prevDpad != 0x08 && dpad == 0x08 && (newBuf[0] & 0x0F) == 0x00)
      {
        switch (prevDpad)
        {
          case 0x00: // UP
            if (currentKit + 1 < NUM_KITS)
            {
              ++currentKit;
            }
            break;
          case 0x04: // DOWN
            if (currentKit != 0)
            {
              --currentKit;
            }
            break;
          case 0x06: // LEFT
            masterGain = max(-70, masterGain - 1);
            wTrig.masterGain(masterGain);
            break;
          case 0x02: // RIGHT
            masterGain = min(4, masterGain + 1);
            wTrig.masterGain(masterGain);
            break;
        }
    
        display.clearDisplay();
        display.setCursor(0, 0);     // Start at top-left corner
        if (prevDpad == 0x06 || prevDpad == 0x02)
        {
          display.println("GAIN");
          itoa(masterGain, intensityStr, 10);
          display.println(intensityStr);
        }
        else
        {
          display.println("KIT");
          itoa(currentKit, intensityStr, 10);
          display.println(intensityStr);
        }
        display.display();
      }

      prevDpad = dpad;

      // This is what a clean "bass drum" hit look like
      // 20 00 08 80 80 80 80 00 00 00 00 00 00 00 00 00 FF
      // 30 00 08 80 80 80 80 00 00 00 00 00 00 00 00 FF FF 
      if (!isKickDown && (newBuf[0] & 0x10) == 0x10)
      {
        Serial.print("Kick up\r\n");
        isKickDown = true;
      }
      else if (isKickDown && (newBuf[0] & 0x10) == 0x00)
      {
        // Always MAXIMUM BASS
        playDrum(KICK_TRACK, 64);
        printHex(0xFF);
        
        Serial.print(" Kick down\r\n");
        isKickDown = false;
      }

      
      if ((newBuf[0] & ~DrumColor::DORG) == 0x00 && newBuf[1] == 0x00)
      {
        collecting = false;
      }

      if (collecting)
      {
        for (uint8_t i = 0; i < EP_MAXPKTSIZE; ++i)
        {
          if (currentBuf[i] < newBuf[i])
          {
            currentBuf[i] = newBuf[i];
          }
        }
      }
      else
      {
        // Strip bass drum data
        currentBuf[0] = currentBuf[0] & ~0x30;
        printDrumsHit();
        for (uint8_t i = 0; i < EP_MAXPKTSIZE; ++i)
        {
          currentBuf[i] = 0;
        }
        collecting = true;
      }
      #else
      memcpy(currentBuf, newBuf, EP_MAXPKTSIZE);
      printRawBuf(); 
      #endif
    }
  }
}
