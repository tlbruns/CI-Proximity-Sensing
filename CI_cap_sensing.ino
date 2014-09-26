// Uses Teensy 3.1 to read capacitance values and display them on a TFT screen
// Trevor Bruns
// Last Revised: July 30 2014 (Ver 3.0)

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9340.h> // Hardware-specific library
#include <SPI.h>
#include <SD.h>    // SD card library

#if defined(__SAM3X8E__)
    #undef __FlashStringHelper::F(string_literal)
    #define F(string_literal) string_literal
#endif

#define COLOR_BACKGROUND 0x0000
#define COLOR_TEXT1      ILI9340_GREEN
#define COLOR_TEXT2      ILI9340_WHITE
#define COLOR_TEXT3      ILI9340_BLUE
#define COLOR_TEXT4      0XFCC0
#define COLOR_RECT       ILI9340_WHITE

const char TFT_RST = 15;
const char TFT_DC = 19;
const char TFT_CS = 10;
const char SD_CS = 9;

Adafruit_ILI9340 tft = Adafruit_ILI9340(TFT_CS, TFT_DC, TFT_RST);

const boolean LOG_DATA = true; // set true to enable datalogging
const byte datalog_pin = 4; // button to start/stop datalogging
boolean datalog_flag = false;  // start false, triggered true
volatile boolean datalog_state = false; // current state of datalogging

// set up variables for SD card data logging
File dataFile;

const byte numtouchPins = 3;
const int touchPin[numtouchPins] = {0,1,18};
unsigned int ELEdata[numtouchPins]; // array to store raw data from ADC
double Capdata[numtouchPins];	    // array to store capacitance values

double loop_time = 0;

void setup()
{
  Serial.begin(115200);	// start serial communication, NOTE: full USB speed (12 Mbit/s)
  
  tft.begin();
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setRotation(1);
  tft.drawRect(2, 5, 210, 170, COLOR_RECT);  

  // print static text to the screen
  tft.setCursor(10, 20);
  tft.setTextColor(COLOR_TEXT1,COLOR_BACKGROUND);  
  tft.setTextSize(2);
  tft.print(F("ELE0 ="));
  tft.setCursor(10, 36);
  tft.print(F("ELE1 ="));
  tft.setCursor(10, 52);
  tft.print(F("ELE2 ="));
  tft.setCursor(10, 84);
  tft.print(F("Cap0 ="));
  tft.setCursor(10, 100);
  tft.print(F("Cap1 ="));
  tft.setCursor(10, 116);
  tft.print(F("Cap2 ="));
  tft.setCursor(10, 148);
  
  tft.setCursor(222,10);
  tft.setTextSize(4);
  tft.setTextColor(COLOR_TEXT3,COLOR_BACKGROUND);
  tft.print(F("CAOS"));
  
  tft.setTextColor(COLOR_TEXT2,COLOR_BACKGROUND);
  tft.setCursor(235,45);
  tft.setTextSize(1);
  tft.print(F("Trevor Bruns"));
  
  tft.setCursor(10,230);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT1,COLOR_BACKGROUND);
  tft.print(F("Version 3.0"));

  pinMode(datalog_pin,INPUT);
  attachInterrupt(datalog_pin,int_datalog,CHANGE);
  
  if(LOG_DATA){
    Serial.print(F("Initializing SD card..."));
    if (!SD.begin(SD_CS)) {
      Serial.println(F("failed!"));
      tft.setCursor(25,150);
      tft.setTextColor(ILI9340_RED);
      tft.setTextSize(4);
      tft.print(F("NO SD CARD!!"));
      delay(2000);
      return;
    }
    bmpDraw("ele.BMP", 220, 55);
    Serial.println(F("card initialized"));
  }
}
	
void loop()
{
  unsigned int loop_start_time = micros();
  
  // read and store data from all electrodes into ELEdata
  for (byte i=0; i<numtouchPins; i++)
    ELEdata[i] = touchRead(touchPin[i]);
    
  // convert to capacitance values (pF)
  for (byte i=0; i<numtouchPins; i++)
    Capdata[i] = ELEdata[i]/50.0;
  	
  if (LOG_DATA){
    if (datalog_state != datalog_flag){  // only do something if state is different than before
      datalog_flag = datalog_state;
      Serial.println(datalog_flag);
      if (datalog_flag)
        newfile();  // start new file
      else{
        dataFile.close();  // close file
        tft.setCursor(20, 195);
        tft.setTextColor(COLOR_TEXT4,COLOR_BACKGROUND);  
        tft.setTextSize(2);
        tft.print(F("data logging stopped  "));
      }
    }
    
    // save data to SD card
    if(datalog_flag){
      datalog(Capdata);
//      delay(5);  // allow for data to reshresh
    }
  }
  
  tft.setTextColor(COLOR_TEXT1,COLOR_BACKGROUND);  
  tft.setTextSize(2);
  
  if (!datalog_flag){  // suppress screen when logging to increase loop rate
    tft.setCursor(90, 20);
    tft.print(ELEdata[0]);
    
    tft.setCursor(90, 36);
    tft.print(ELEdata[1]);
    
    tft.setCursor(90, 52);
    tft.print(ELEdata[2]);
  }
    
  tft.setCursor(90, 84);
  tft.print(Capdata[0],2);
  tft.print(F(" pF "));
  
  tft.setCursor(90, 100);
  tft.print(Capdata[1],2);
  tft.print(F(" pF "));
  
  tft.setCursor(90, 116);
  tft.print(Capdata[2],2);
  tft.print(F(" pF "));

  tft.setCursor(260,230);
  tft.setTextColor(COLOR_TEXT1,COLOR_BACKGROUND);
  tft.setTextSize(1);
  loop_time = (micros()-loop_start_time)/1000.0;
  tft.print(loop_time,2);
  tft.print(F(" ms   "));
}

//*****************************************************************************//
//*****************************************************************************//

void newfile()
{
  char filename[] = "trial00.csv";
  for (uint8_t i = 0; i < 100; i++) {
    filename[5] = i/10 + '0';
    filename[6] = i%10 + '0';
//    Serial.print(F("checking suffix "));
//    Serial.println(filename);
    if (!SD.exists(filename)) {
      // only open a new file if it doesn't exist
      dataFile = SD.open(filename, FILE_WRITE);
      
      // write headers for CSV file
      String dataString = "";
      for (int i = 0; i < numtouchPins; i++) {
        dataString += "Pin ";
        dataString += String(touchPin[i]);
        dataString += ","; // for CSV file
        if (i == (numtouchPins-1))  
          dataString += "Loop Time";  
      }
      // if the file is available, write to it:
      if (dataFile)
        dataFile.println(dataString);
    
    
      // Output status to TFT
      tft.setCursor(20, 195);
      tft.setTextColor(COLOR_TEXT4,COLOR_BACKGROUND);  
      tft.setTextSize(2);
      tft.print(F("saving to ")); 
      tft.print(filename);
      break;
    }
  }
}

//*****************************************************************************//
void int_datalog()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 100ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 100) 
    datalog_state = digitalRead(datalog_pin);
  last_interrupt_time = interrupt_time;
}

//*****************************************************************************//

void datalog(double *Capdata)
{
  // make a string for assembling the data to log:
  String dataString = "";

  // read data and append to the string:
  for (byte i = 0; i < numtouchPins; i++) {
    char cbuf[10]; // temporary buffer to store double
    dtostrf(Capdata[i],5,2,cbuf); // converts from double to char
    dataString += cbuf;
    dataString += ","; // for CSV file
    if (i == (numtouchPins-1)){  // log loop time as last entry
      dtostrf(loop_time,5,2,cbuf);
      dataString += cbuf;
    }
  }

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    //dataFile.close();
  }  
  // if the file isn't open, pop up an error:
  else {
    Serial.println(F("error opening file"));
    datalog_flag = false;
    dataFile.close();  // close file
    tft.setCursor(20, 200);
    tft.setTextColor(COLOR_TEXT2,COLOR_BACKGROUND);  
    tft.setTextSize(2);
    tft.print(F("file error      "));
  } 
}

//**********************************************************************//

// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.  20 pixels seems a
// good balance.

#define BUFFPIXEL 20

void bmpDraw(char *filename, uint8_t x, uint8_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  if((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print("Loading image '");
  Serial.print(filename);
  Serial.println('\'');

  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print("File not found");
    return;
  }

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print("File size: "); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print("Image Offset: "); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print("Header size: "); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print("Bit Depth: "); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print("Image size: ");
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x+w-1, y+h-1);

        for (row=0; row<h; row++) { // For each scanline...

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col=0; col<w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.Color565(r,g,b));
          } // end pixel
        } // end scanline
        Serial.print("Loaded in ");
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) Serial.println("BMP format not recognized.");
}

//*****************************************************************************//

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File & f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File & f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
