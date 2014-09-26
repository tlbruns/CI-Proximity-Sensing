// Uses Teensy 3.1 to read capacitance values and display them on a TFT screen
// Trevor Bruns
// Last Revised: Aug 5 2014 (Ver 3.1)

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9340.h> // Hardware-specific library
#include <SPI.h>
#include <SD.h>    // SD card library

#if defined(__SAM3X8E__)
    #undef __FlashStringHelper::F(string_literal)
    #define F(string_literal) string_literal
#endif

#define COLOR_BACKGROUND 0x0000
#define COLOR_TEXT1      0x07E0 // GREEN
#define COLOR_TEXT2      0xFFFF // WHITE
#define COLOR_TEXT3      0x001F // BLUE
#define COLOR_TEXT4      0XFCC0
#define COLOR_TEXT5      0xF800 // RED
#define COLOR_RECT       0xFFFF // WHITE

const char TFT_RST = 14;
const char TFT_DC = 20;
const char TFT_CS = 10;
const char SD_CS = 9;

Adafruit_ILI9340 tft = Adafruit_ILI9340(TFT_CS, TFT_DC, TFT_RST);

const boolean LOG_DATA = true; // set true to enable datalogging
const byte datalog_pin = 4; // button to start/stop datalogging
boolean datalog_flag = false;  // start false, triggered true
volatile boolean datalog_state = false; // current state of datalogging

// set up variables for SD card data logging
File dataFile;

const byte numtouchPins = 9;
const int touchPin[numtouchPins] = {0,1,15,16,17,18,19,22,23};
unsigned int ELEdata[numtouchPins]; // array to store raw data from ADC
double Capdata[numtouchPins];	    // array to store capacitance values

int cur_pos = 0;  // current X position of the electrode array
const int pos_increment = 2;  // amount to inc/dec each button press (in 1/1000s of an inch)
const byte button_pos_inc = 6;  // pin # for button to increase current electrode position
const byte button_pos_dec = 5;
volatile boolean inc_pos_flag;
volatile boolean dec_pos_flag;

double loop_time = 0;
byte loop_counter = 0;

void setup()
{
  Serial.begin(115200);	// start serial communication, NOTE: full USB speed (12 Mbit/s)
  
  tft.begin();
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setRotation(1);
//  tft.drawRect(2, 5, 205, 170, COLOR_RECT);  

  // print static text to the screen
  tft.setTextColor(COLOR_TEXT2,COLOR_BACKGROUND);
  tft.setTextSize(2);
  tft.setCursor(5, 15);
  tft.print(F("Position"));
  tft.setCursor(105,15);
  tft.print(F("="));
  
  tft.setTextColor(COLOR_TEXT1,COLOR_BACKGROUND);  
  for(byte i=0; i<numtouchPins; i++){
    tft.setCursor(5, 50+i*16);
    tft.print(F("Cap"));
    tft.print(touchPin[i]);
    tft.setCursor(70, 50+i*16);
    tft.print(F("="));
    }
  
  tft.setTextColor(COLOR_TEXT2,COLOR_BACKGROUND);
  tft.setCursor(220,50);
  tft.setTextSize(1);
  tft.print(F("Trevor Bruns"));
  
  tft.setCursor(10,230);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT1,COLOR_BACKGROUND);
  tft.print(F("Version 3.1"));

  pinMode(datalog_pin,INPUT);
  
  if(LOG_DATA){
    pinMode(button_pos_inc,INPUT);
    pinMode(button_pos_dec,INPUT);
    attachInterrupt(button_pos_inc,int_pos_inc, LOW);
    attachInterrupt(button_pos_dec,int_pos_dec, LOW);
  }
  
  attachInterrupt(datalog_pin,int_datalog, CHANGE);
  
  if(LOG_DATA){
    Serial.print(F("Initializing SD card..."));
    if (!SD.begin(SD_CS)) {
      Serial.println(F("failed!"));
      tft.setCursor(25,150);
      tft.setTextColor(COLOR_TEXT5);
      tft.setTextSize(4);
      tft.print(F("NO SD CARD!!"));
      delay(2000);
      return;
    }
    bmpDraw("ele.BMP", 220, 60);
    bmpDraw("caos140.bmp",175,10);
    Serial.println(F("card initialized"));
  }
}
	
void loop()
{
  unsigned int loop_start_time = micros();
  
  // check if position has changed
  if(inc_pos_flag){
    cur_pos += pos_increment;
    inc_pos_flag = false;
  }
  else if(dec_pos_flag){
    cur_pos -= pos_increment;
    dec_pos_flag = false;
  }
  
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
        tft.setCursor(20, 205);
        tft.setTextColor(COLOR_TEXT4,COLOR_BACKGROUND);  
        tft.setTextSize(2);
        tft.print(F("data logging stopped  "));
      }
    }
    
    // write data to SD card
    if(datalog_flag) {
      datalog(Capdata);
      if(loop_counter>100) {
        dataFile.flush();  // "saves" data every 100 samples (~4 sec)
        loop_counter=0; 
      }    
    }
  
  // suppress screen 7/8 loops when logging to increase speed
  if(datalog_flag && loop_counter%8 != 0)
    delay(20); // slow down to limit amount of data
  else{
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT2, COLOR_BACKGROUND);
    tft.setCursor(120,15);
    tft.print(cur_pos);
    tft.print("  "); // erases extra characters when number decreases
    tft.setTextColor(COLOR_TEXT1,COLOR_BACKGROUND);  
    for(byte i=0; i<numtouchPins; i++){
      tft.setCursor(85, 50+i*16);
      tft.print(Capdata[i],1);
      tft.print(F(" pF "));
    }
  }

  tft.setCursor(260,230);
  tft.setTextColor(COLOR_TEXT1,COLOR_BACKGROUND);
  tft.setTextSize(1);
  loop_time = (micros()-loop_start_time)/1000.0;
  tft.print(loop_time,2);
  tft.print(F(" ms   "));
  loop_counter++;
  }
}

//*****************************************************************************//
//*****************************************************************************//

void newfile()
{
  char filename[] = "trial00.csv";
  for (uint8_t i = 0; i < 100; i++) {
    filename[5] = i/10 + '0';
    filename[6] = i%10 + '0';
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
          dataString += "Position, Loop Time";  
      }
      // if the file is available, write to it:
      if (dataFile)
        dataFile.println(dataString);
    
    
      // Output status to TFT
      tft.setCursor(20, 205);
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
  static unsigned long last_int_datalog = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 100ms, assume it's a bounce and ignore
  if (interrupt_time - last_int_datalog > 100) 
    datalog_state = digitalRead(datalog_pin);
  last_int_datalog = interrupt_time;
}

//*****************************************************************************//
void int_pos_inc()
{
  static unsigned long last_int_pos_inc = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 300ms, assume it's a bounce and ignore
  if (interrupt_time - last_int_pos_inc > 80) 
    inc_pos_flag = true;
  last_int_pos_inc = interrupt_time;
}

//*****************************************************************************//
void int_pos_dec()
{
  static unsigned long last_int_pos_dec = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 300ms, assume it's a bounce and ignore
  if (interrupt_time - last_int_pos_dec > 80) 
    dec_pos_flag = true;
  last_int_pos_dec = interrupt_time;
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
    if (i == (numtouchPins-1)){  // log position and loop time as last entries
      itoa(cur_pos,cbuf,10);
      dataString += cbuf;
      dataString += ",";
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
