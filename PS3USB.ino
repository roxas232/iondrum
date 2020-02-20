// Uncomment the following line to debug the raw USB packets
//#define RAW_PACKET_DEBUG 1

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Change this to reflect the number of kits you have
#define NUM_KITS 2

// Add a name for each of your kits here
String kitNames[NUM_KITS] = { 
  "Acoustic",
  "808"
};

// Modify this to change the bass volume
// 64 = Loudest
// 225 = Quietest
#define KICK_VOLUME 100

// Each track should be duplicated twice per kit
// This reduces the volume spikes if you vary the intensity of your hits
// ex. 001_mycrash.wav and 009_mycrash.wav
#define CRASH_TRACK   1
#define HIHAT_TRACK   2
#define HITOM_TRACK   3
#define KICK_TRACK    4
#define LOWTOM_TRACK  5
#define MIDTOM_TRACK  6
#define RIDE_TRACK    7
#define SNARE_TRACK   8

// 16 sounds are used per kit (2/drum)
#define KIT_OFFSET 16

// Offset to the secondary track for each drum (1,9 2,10 etc)
#define SECONDARY_OFFSET 8

// Modify this if you have a different OLED display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     30 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#include <wavTrigger.h>
#include <PS3USB.h>

// Satisfy the IDE, which needs to see the include statment in the ino too.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>

USB Usb;
PS3USB PS3(&Usb);

// Are we still collecting data for the current drum hit?
bool collecting = true;
// Is the kick drum pedal currently down?
bool isKickDown = false;
// The current kit in use, 0 based
uint8_t currentKit = 0;

// The current USB data packet
uint8_t currentBuf[EP_MAXPKTSIZE];
// The next USB data packet
uint8_t newBuf[EP_MAXPKTSIZE];

// Buffer used to convert numbers to strings for display purposes
char numberDispStr[30];

// Tracks if the ION drumset has been detected
bool drumsActive = false;
// Previous state of the DPAD
uint8_t prevDpad = 0xFF;
// How many bogus USB packets we've received on bootup
uint8_t garbageReads = 0;

// Handles serial commuincations to the Wav Trigger
wavTrigger wTrig;
// Master gain for the Wav Trigger
int masterGain = 0;

// Flag enum representing the color of the drum hit
enum DrumColor {
  DBLU = 0x01,
  DGRE = 0x02,
  DRED = 0x04,
  DYEL = 0x08,
  DORG = 0x30
};

// Flag enum representing the type of drum hit
enum DrumType {
  DRUM = 0x04,
  CYM = 0x08
};

// Min/max gain for each drum channel, scaled by the related intensity value
#define MIN_GAIN      -2
#define MAX_GAIN      10

// Tracks whether to use the primary or secondary channel for each drum track
bool useOffsetForTrack[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

// Prints a number in hex format to the serial monitor
void printHex(int num)
{
  char tmp[16];
  char format[128];
  
  sprintf(format, "%%.%dX", 2);
  
  sprintf(tmp, format, num);
  Serial.print(tmp);
}

// Plays an individual drum track at the specified intensity
// Handles the switching from primary to secondary tracks
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
}

// Queues an individual drum track at the specified intensity
// Handles the switching from primary to secondary tracks
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

// Plays a combination of drums
// Assumes a 0C value for drum/cymbal mask
void playDrumCombo(uint8_t color)
{
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

  bool isRed = (color & DrumColor::DRED) == DrumColor::DRED;
  bool isYellow = (color & DrumColor::DYEL) == DrumColor::DYEL;
  bool isBlue = (color & DrumColor::DBLU) == DrumColor::DBLU;
  bool isGreen = (color & DrumColor::DGRE) == DrumColor::DGRE;
  
  if (isYellow && isBlue)
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
  else if (isYellow && isGreen)
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
  else if (isBlue && isGreen)
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
  else if (isRed)
  {
    if (isYellow)
    {
      playMultiDrum(SNARE_TRACK, redIntensity);
      playMultiDrum(HIHAT_TRACK, yellowIntensity);
    }
    else if (isBlue)
    {
      playMultiDrum(SNARE_TRACK, redIntensity);
      playMultiDrum(RIDE_TRACK, blueIntensity);
    }
    else if (isGreen)
    {
      playMultiDrum(SNARE_TRACK, redIntensity);
      playMultiDrum(CRASH_TRACK, greenIntensity);
    }
  }
  else
  {
    Serial.print("Invalid combo hit\r\n");
  }

  wTrig.resumeAllInSync();
}

// Play a drum hit, including multi-hits
void playDrumsHit()
{
  uint8_t color = currentBuf[0];
  uint8_t drumCym = currentBuf[1];
  uint8_t intensity;

  if (drumCym == (DrumType::DRUM | DrumType::CYM) 
    && color != DrumColor::DYEL && color != DrumColor::DBLU && color != DrumColor::DGRE)
  {
    playDrumCombo(color);
    return;
  }

  bool isDrum = (drumCym & DrumType::DRUM) == DrumType::DRUM;

  if ((color & DrumColor::DBLU) == DrumColor::DBLU)
  {
    if (isDrum)
    {
      intensity = currentBuf[14];
      playDrum(MIDTOM_TRACK, intensity);
    }
    else
    {
      intensity = currentBuf[14];
      playDrum(RIDE_TRACK, intensity);
    }
  }

  if ((color & DrumColor::DGRE) == DrumColor::DGRE)
  {
    if (isDrum)
    {
      intensity = currentBuf[13];
      playDrum(LOWTOM_TRACK, intensity);
    }
    else
    {
      intensity = currentBuf[13];
      playDrum(CRASH_TRACK, intensity);
    }
  }

  if ((color & DrumColor::DRED) == DrumColor::DRED)
  {
    if (isDrum)
    {
      intensity = currentBuf[12];
      playDrum(SNARE_TRACK, intensity);
    }
    else
    {
      Serial.print("FAKE cymbal?\r\n");
    }
  }

  if ((color & DrumColor::DYEL) == DrumColor::DYEL)
  {
    if (isDrum)
    {
      intensity = currentBuf[11];
      playDrum(HITOM_TRACK, intensity);
    }
    else
    {
      intensity = currentBuf[11];
      playDrum(HIHAT_TRACK, intensity);
    }
  }
}

#ifdef RAW_PACKET_DEBUG
// Prints the current USB packet to the serial monitor for debug purposes
void printRawBuf()
{ 
  for (uint8_t i = 0; i < 17; ++i)
  {
    printHex(currentBuf[i]);
    Serial.print(" ");
  }
  Serial.print("\r\n");
}
#endif

void setup()
{
  Serial.begin(115200);

  // Init the OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3D);

  // Clear the buffer
  display.clearDisplay();

  display.setTextSize(2);      // Draw 2X-scale text
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  
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

void displayGain()
{
  display.print(masterGain > -70 ? "^" : " ");
  display.print(" GAIN ");
  display.println(masterGain < 4 ? "V" : " ");
  itoa(masterGain, numberDispStr, 10);
  display.println(numberDispStr);
}

void displayKit()
{
  display.print(currentKit > 0 ? "<" : " ");
  display.print(" KIT ");
  itoa(currentKit, numberDispStr, 10);
  display.print(numberDispStr);
  display.println(currentKit < NUM_KITS - 1 ? ">" : " ");

  display.println(kitNames[currentKit]);
}

// Handle kit/gain changes
void handleDpad()
{
  uint8_t dpad = newBuf[2];
  // Dpad keyup
  if (prevDpad != 0x08 && dpad == 0x08 && (currentBuf[0] & 0x0F) == 0x00 && (newBuf[0] & 0x0F) == 0x00)
  {
    switch (prevDpad)
    {
      case 0x02: // RIGHT
        if (currentKit + 1 < NUM_KITS)
        {
          ++currentKit;
        }
        break;
      case 0x06: // LEFT
        if (currentKit != 0)
        {
          --currentKit;
        }
        break;
      case 0x04: // DOWN
        masterGain = max(-70, masterGain - 1);
        wTrig.masterGain(masterGain);
        break;
      case 0x00: // UP
        masterGain = min(4, masterGain + 1);
        wTrig.masterGain(masterGain);
        break;
    }

    display.clearDisplay();
    display.setCursor(0, 0);     // Start at top-left corner
    if (prevDpad == 0x06 || prevDpad == 0x02)
    {
      displayGain();
    }
    else
    {
      displayKit();
    }
    display.display();
  }

  prevDpad = dpad;
}

// Process USB data to determine if a bass drum hit occurred
// and play noise
void handleKick()
{
  // This is what a clean "bass drum" hit look like
  // 20 00 08 80 80 80 80 00 00 00 00 00 00 00 00 00 FF
  // 30 00 08 80 80 80 80 00 00 00 00 00 00 00 00 FF FF 
  if (!isKickDown && (newBuf[0] & 0x10) == 0x10)
  {
    isKickDown = true;
  }
  else if (isKickDown && (newBuf[0] & 0x10) == 0x00)
  {
    // Always MAXIMUM BASS...or close
    playDrum(KICK_TRACK, KICK_VOLUME); // 64
    isKickDown = false;
  }
}

// Process USB data to determine if a normal drum and/or cymbal hit occurred
// and play noise
void handleDrumHit()
{
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
    playDrumsHit();
    for (uint8_t i = 0; i < EP_MAXPKTSIZE; ++i)
    {
      currentBuf[i] = 0;
    }
    collecting = true;
  }
}

void loop() 
{
  Usb.Task();
  
  if (PS3.PS3Connected)
  {
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
      
      handleDpad();
      handleKick();
      handleDrumHit();
      
      #else
      memcpy(currentBuf, newBuf, EP_MAXPKTSIZE);
      printRawBuf(); 
      #endif
    }
  }
}
