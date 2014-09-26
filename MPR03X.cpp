//#include "Arduino.h"
#include "D:\Programs\Arduino\libraries\Wire\Wire.h"
#include "MPR03X.h"

boolean Wirestarted = false;

void MPR03X_init(int device, int CDC, int CDT)	// MPR031 -> device = 1, MPR032 -> device = 2
{
  
  int dev_add;
  if(!Wirestarted){
    Serial.println("Starting I2c Communication");
    Wire.begin();  // join i2c bus
    Wirestarted = true;
  }
  	
  if(device == 1){
    Serial.println("Setting up Device 1");
    dev_add = MPR031_add;
  }
  else if (device == 2)
    dev_add = MPR032_add;
  else{
    Serial.println("invalid device number");
    return;
  }
  // Ensure device is in stop mode
  Wire.beginTransmission(dev_add);
  Wire.write(byte(ELE_config));
  Wire.write(0x00);
  Wire.endTransmission();
  	
  // Set AFE_config
  Wire.beginTransmission(dev_add);
  Wire.write(byte(AFE_config));
  Wire.write(byte(CDC));	// FFI = 6 samples, charge current = CDC
  Wire.endTransmission();
  Serial.print("set AFE_config to ");
  Serial.println(byte(CDC),BIN);
  	
  // Set filter_config
  Wire.beginTransmission(byte(dev_add));
  Wire.write(byte(filter_config));
  Wire.write(byte(CDT<<5));	// Charge/Discharge time = CDT, SFI = 4 samples, ESI = 1ms
  Wire.endTransmission();
  Serial.print("set filter_config to ");
  Serial.println(CDT<<5,BIN);
	
  // Set ELE_config
  Wire.beginTransmission(byte(dev_add));
  Wire.write(byte(ELE_config));
  Wire.write(byte(B00000011));	// baseline calibration disabled, Run1 Mode, ELE0/1/2 enabled
  Wire.endTransmission();	// stop mode is exited and now running in Run1 Mode
  Serial.println("setup ELE_config");

}

void MPR03X_readELEdata(int device, int num_ELE, unsigned int *ELEdata)
{
	unsigned char dev_add;
	byte rec_bytes[num_ELE*2];	// array to store receive bytes
	byte cnt = 0;
	
	if(device == 1)
	  dev_add = MPR031_add;
	else if (device == 2)
	  dev_add = MPR032_add;
	else{
	  Serial.println("invalid device number");
	  return;
	}
	
	Wire.beginTransmission(dev_add);
	Wire.write(ELE0_data_low);
	Wire.endTransmission(false);  // false parameter ensures repeated start
        Wire.requestFrom((int)dev_add, num_ELE*2);    // request high and low bytes for each electrode
        while(Wire.available() == 0); // wait until data is available
        //Serial.println("Data available");
	while(Wire.available())	// read incoming data
	{ 
	  rec_bytes[cnt] = Wire.read();
	  cnt++;
	}

        // combine high/low bytes and check that data is in valid range 
        // of ADC counts (assumes 2.5V nominal Vdd)
        int ADClow = 0.7*1024/2.5;
        int ADChigh = 1.8*1024/2.5;
	for(int i=0; i<num_ELE; i++)
        {
          ELEdata[i] = word(rec_bytes[2*i+1],rec_bytes[2*i]);
        }
}

void MPR03X_convtoCap(unsigned int *ELEdata, double *Capdata, int CDC, int CDT, double Vin_value)
{
  int T = 2^(CDT-2);  // convert from CDT register value to charge time in us
  
  for(int i = 0; i<3; i++)
    Capdata[i] = (CDC*T*1024.0)/(Vin_value*ELEdata[i]);
}
