// MPR03X Library
// Trevor Bruns
// May 23 2014

// This library allows for communication with MPR03X capacitive sensors

#ifndef MPR03X_h
#define MPR03X_h
#include "Arduino.h"

#define Vdd 2.5	// MUST ensure that this is correct for accurate capacitance values!

#define	MPR031_add 0x4A
#define	MPR032_add 0x4B

#define	touch_status	0x00
#define	ELE0_data_low	0x02
#define ELE0_data_high	0x03
#define	ELE1_data_low	0x04
#define ELE1_data_high	0x05
#define	ELE2_data_low	0x06
#define ELE2_data_high	0x07

#define	ELE0_baseline	0x1A
#define	ELE1_baseline	0x1B
#define	ELE2_baseline	0x1C

#define	max_half_delta	 0x26
#define noise_half_delta 0x27
#define noise_count		 0x28

#define ELE0_touch_thresh	0x29
#define ELE0_release_thresh	0x2A
#define ELE1_touch_thresh	0x2B
#define ELE1_release_thresh	0x2C
#define ELE2_touch_thresh	0x2D
#define ELE2_release_thresh	0x2E

#define AFE_config		0x41
#define filter_config	0x43
#define	ELE_config		0x44

void MPR03X_init(byte device, byte CDC, byte CDT);
void MPR03X_readELEdata(byte device, byte num_ELE, unsigned int *ELEdata);
void MPR03X_convtoCap(unsigned int *ELEdata, double *Capdata, byte CDC, byte CDT, double Vin_value);

#endif
