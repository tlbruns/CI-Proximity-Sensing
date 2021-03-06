// Uses Teensy 3.1 to read capacitance values and display them on a TFT screen
// Trevor Bruns
// Last Revised: Sep 25 2014 (Ver 3.5)

#include <Adafruit_GFX.h>    // Core graphics library
#include <ILI9341_t3.h> // Hardware-specific library
#include <SPI.h>
#include <SD.h>    // SD card library
#include <Digimatic.h>  // Read data from Mitutoyo calipers

#define CPU_RESTART_ADDR (uint32_t *)0xE000ED0C
#define CPU_RESTART_VAL 0x5FA0004
#define CPU_RESTART (*CPU_RESTART_ADDR = CPU_RESTART_VAL);

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

//Adafruit_ILI9340 tft = Adafruit_ILI9340(TFT_CS, TFT_DC, TFT_RST);
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST);

const boolean LOG_DATA = true; // set true to enable datalogging
const byte datalog_pin = 4; // button to start/stop datalogging
boolean datalog_flag = false;  // start false, triggered true
volatile boolean datalog_state = false; // current state of datalogging

// set up variables for SD card data logging
File dataFile;

const byte numtouchPins = 10;
//const int touchPin[numtouchPins] = {0,1,15};
const int touchPin[numtouchPins] = {0,1,15,16,17,18,19,22,23,25};
unsigned int ELEdata[numtouchPins]; // array to store raw data from ADC
double Capdata[numtouchPins];	    // array to store capacitance values
double bias[numtouchPins];  // stores bias value for each electrode
double TSI_count2cap = 53.33; // scaling from raw count register to capacitance in pF (division)
int bias_samples = 100; // number of samples to average
int bias_count = 0;  // counts loops for bias function
boolean bias_set = false;

double cur_pos = 0.0;  // current X position of the electrode array
const int pos_increment = 2;  // amount to inc/dec each button press (in 1/1000s of an inch)
const byte button_pos_inc = 26;  // pin # for momentary buttons
const byte button_pos_dec = 24;
const byte button_bias = 29;
const byte button_restart = 28; // unused; sticks a little
volatile boolean inc_pos_flag = false;
volatile boolean dec_pos_flag = false;
volatile boolean bias_flag = false;

const byte BatteryMonitor_pin = 7; // Analog input pin for battery monitoring (digital pin 21)

// define pins for Digimatic
uint8_t req_pin = 27; //mic REQ line goes to pin 5 through q1 (arduino high pulls request line low)
uint8_t data_pin = 30; //mic Data line goes to pin 2
uint8_t clk_pin = 31; //mic Clock line goes to pin 3
// initialize Digimatic
Digimatic caliper = Digimatic(clk_pin, data_pin, req_pin);

double loop_time = 0;
byte loop_counter = 0;

void setup()
{
  Serial.begin(115200);	// start serial communication, NOTE: full USB speed (12 Mbit/s)
  
  // setup ADC for monitoring battery voltage
  analogReference(INTERNAL); // use 1.2V interal reference (less noise)
  analogReadAveraging(32);   // average 32 samples
  analogReadRes(12);         // 12 bit resolution, 0-4095
  
  tft.begin();
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setRotation(1);  

  // print static text to the screen
  tft.setTextColor(COLOR_TEXT2,COLOR_BACKGROUND);
  tft.setTextSize(2);
  tft.setCursor(180, 15);
  tft.print(F("Position"));
  tft.setCursor(280,15);
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
  tft.setCursor(40,36);
  tft.setTextSize(1);
  tft.print(F("Trevor Bruns"));
  
  tft.setCursor(10,230);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT2,COLOR_BACKGROUND);
  tft.print(F("Version 3.5"));

  pinMode(datalog_pin,INPUT);
  
  if(LOG_DATA){
    pinMode(button_pos_inc,INPUT);
    pinMode(button_pos_dec,INPUT);
    pinMode(button_bias,INPUT);
    pinMode(button_restart,INPUT);
//    attachInterrupt(button_pos_inc,int_pos_inc, LOW);
//    attachInterrupt(button_pos_dec,int_pos_dec, LOW);
    attachInterrupt(button_bias,int_bias, LOW);
    attachInterrupt(button_restart,int_restart, LOW);
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
//    bmpDraw("ele.BMP", 220, 60);
    bmpDraw("caos140.bmp",5,2);
    Serial.println(F("card initialized"));
  }
}
//*****************************************************************************//
	
void loop()
{
  unsigned int loop_start_time = micros();
  
  if(!loop_counter%100)  // check every 100 loops (~2.5 seconds)
    checkbattery(BatteryMonitor_pin);
  
  // request position measurement
  if(!loop_counter%5)  // check every 5 loops (~125ms seconds)
    cur_pos = caliper.fetch();
  
  // read and store data from all electrodes into ELEdata
  for (byte i=0; i<numtouchPins; i++)
    ELEdata[i] = touchRead(touchPin[i]);
    
  // convert to capacitance values (pF)
  for (byte i=0; i<numtouchPins; i++)
    Capdata[i] = ELEdata[i]/TSI_count2cap;
    
  // check if biasing active
  if(bias_flag){
    bias_function();
  }
    
  // apply bias
  if(bias_set){
    for (int i = 0; i < numtouchPins; i++){
      Capdata[i] = Capdata[i] - bias[i];
//      if (Capdata[i]<0) Capdata[i]=0;
    }
  }
  	
  if (LOG_DATA){
    if (datalog_state != datalog_flag){  // only do something if state is different than before
      datalog_flag = datalog_state;
      Serial.println(datalog_flag);
      if (datalog_flag)
        newfile();  // start new file
      else{
        dataFile.close();  // close file
        tft.setCursor(20, 210);
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
  }
  
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT2, COLOR_BACKGROUND);
  tft.setCursor(295,15);
  tft.print(cur_pos);
  tft.print("  "); // erases extra characters when number decreases
  tft.setTextColor(COLOR_TEXT1,COLOR_BACKGROUND);  
  for(byte i=0; i<numtouchPins; i++){
    tft.setCursor(85, 50+i*16);
    tft.print(Capdata[i],1);
    tft.print(F(" pF "));
    draw_bargraph(i,(int)Capdata[i]);
  }
  
  

  tft.setCursor(260,230);
  tft.setTextColor(COLOR_TEXT2,COLOR_BACKGROUND);
  tft.setTextSize(1);
  loop_time = (micros()-loop_start_time)/1000.0;
  tft.print(loop_time,2);
  tft.print(F(" ms   "));
  loop_counter++;
}

//*****************************************************************************//
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
        
      // if bias has been set, write bias values as first row of data
      if(bias_set){
        dataString = "";
        for (byte i = 0; i < numtouchPins; i++) {
          char cbuf[10]; // temporary buffer to store double
          dtostrf(bias[i],5,2,cbuf); // converts from double to char
          dataString += cbuf;
          dataString += ","; // for CSV file
          if (i == (numtouchPins-1)){
            itoa(0,cbuf,10);
            dataString += cbuf;
            dataString += ",";
            dtostrf(0.0,5,2,cbuf);
            dataString += cbuf;
          }
        }
        if (dataFile)
          dataFile.println(dataString);
      }
    
      // Output status to TFT
      tft.setCursor(20, 210);
      tft.setTextColor(COLOR_TEXT4,COLOR_BACKGROUND);  
      tft.setTextSize(2);
      tft.print(F("saving to ")); 
      tft.print(filename);
      break;
    }
  }
}

//*****************************************************************************//
void checkbattery(byte batt_pin)
{
  double voltage = analogRead(batt_pin)*4.4807/4096.0; // based on R1 = 21770, R2 = 7963 (3291+4672)
  tft.setCursor(150,230);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT2,COLOR_BACKGROUND);
  tft.print(F("Battery = "));
  tft.print(voltage,3);
  tft.print("V");
}

//*****************************************************************************//
void bias_function()
{  
  if(bias_count == 0){  
    // reset bias values
    bias_set = false;
    for (int i = 0; i < numtouchPins; i++){
      bias[i] = 0.0;
    }
    tft.setCursor(20, 210);
    tft.setTextColor(COLOR_TEXT4,COLOR_BACKGROUND);  
    tft.setTextSize(2);
    tft.print(F("biasing..."));
    bias_count++;
    return;
  }    
  else if(bias_count<10)  // delay to allow finger to be removed from button
  {
    bias_count++;
    return;
  }
    
  bias_count++;
  // sum Capdata values for averaging
  for (int i = 0; i < numtouchPins; i++)
    bias[i] = bias[i] + Capdata[i];
    
  if(bias_count == bias_samples+11){
    for (int i = 0; i < numtouchPins; i++)
      bias[i] = bias[i]/(double)(bias_samples);
      
    tft.setCursor(20, 210);
    tft.setTextSize(2);
    tft.print(F("          "));
    bias_set = true;
    bias_flag = false;
    bias_count = 0;
  }
  
}

//*****************************************************************************//
void int_datalog()
{
  static unsigned long last_int_datalog = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 500ms, assume it's a bounce and ignore
  if (interrupt_time - last_int_datalog > 500) 
    datalog_state = digitalRead(datalog_pin);
  last_int_datalog = interrupt_time;
}

//*****************************************************************************//
void int_pos_inc()
{
  static unsigned long last_int_pos_inc = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 80ms, assume it's a bounce and ignore
  if (interrupt_time - last_int_pos_inc > 80) 
    inc_pos_flag = true;
  last_int_pos_inc = interrupt_time;
}

//*****************************************************************************//
void int_pos_dec()
{
  static unsigned long last_int_pos_dec = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 80ms, assume it's a bounce and ignore
  if (interrupt_time - last_int_pos_dec > 80) 
    dec_pos_flag = true;
  last_int_pos_dec = interrupt_time;
}

//*****************************************************************************//
void int_bias()
{
  static unsigned long last_int_bias = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 80ms, assume it's a bounce and ignore
  if (interrupt_time - last_int_bias > 80) 
    bias_flag = true;
  last_int_bias = interrupt_time;
}

//*****************************************************************************//
void int_restart()
{
  static unsigned long last_int_restart = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 80ms, assume it's a bounce and ignore
  if (interrupt_time - last_int_restart > 80)
    CPU_RESTART; 
  last_int_restart = interrupt_time;
}

//*****************************************************************************//
void draw_bargraph(int ELEnum, int value)
{
  int x0 = 180;
  int y0 = 50;
  int maxvalue = 140;  
  int graph_colors[9] = {ILI9341_BLUE,ILI9341_GRAY,ILI9341_GREEN,ILI9341_CYAN,ILI9341_MAGENTA,ILI9341_YELLOW,ILI9341_WHITE,ILI9341_RED,ILI9341_TEAL};
  if(value>maxvalue) value=maxvalue;
  if(value<0) value = 0;
  int barwidth = 16;
  
  tft.fillRect(x0, y0+ELEnum*barwidth, value, barwidth, graph_colors[ELEnum]);
  tft.fillRect(x0+value, y0+ELEnum*barwidth, maxvalue-value, barwidth, COLOR_BACKGROUND);
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
      dtostrf(cur_pos,6,caliper.decimal_places(),cbuf);
      dataString += cbuf;
      dataString += ",";
      dtostrf(loop_time,5,2,cbuf);
      dataString += cbuf;
    }
  }

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
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
//            tft.pushColor(tft.Color565(r,g,b));
            tft.pushColor(tft.color565(r,g,b));
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
