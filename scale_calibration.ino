// Scale Calibration for ESP32, Dumps to Serial Monitor only. No button
// or encoder used.
// uses the Tillaart HX711 load cell library 
// https://github.com/RobTillaart/HX711
// Modified from library example to make some steps clearer, and to run sensitivity tests

#include "HX711.h"

HX711 scale;

uint8_t dataPin = 19; //DT
uint8_t clockPin = 21; //CLK
float wt;

void setup(){
  Serial.begin(9600);
  delay(500);
  Serial.println("Starting....");

  scale.begin(dataPin, clockPin);
  // Below sets type of averaging output
  // void set_average_mode()
  // void set_median_mode()
  // void set_medavg_mode()
  // void set_runavg_mode() default alpha = 0.5.
  // uint8_t get_mode() gets current mode
  //get_units; gets an average of above type Number is values to get.
  //10 seems to be commonly used. Finally, calculates the _scale factor (_scale is initialized to 1 on startup).
  //then does an average of several things; get_units() returns (read_average(1) - OFFSET) /_scale
  //below 
  uint8_t tests;
  float rawReads;
  tests = 100;
  //Serial.print("RAW UNITS: ");
  //Serial.println(scale.get_units(tests)); //average tests values, instead of typical 10.
  //For reference shows 50 raw values
  //for (int i=0; i<tests; i++) {
  // Serial.println(scale.read());
  //}
  Serial.println("Beginning the calibration.");
  Serial.println("\nEmpty the scale, press a key to continue");
  //if serial Monitor open, it is waiting. 
  while(!Serial.available()); //wait for key pressed, must be in send line for focus.
  while(Serial.available()) Serial.read(); 
  // get the OFFSET, or zero wt scalar; equivalent to 0 intercept.
  // _offset is initially set to zero; 
  // _scale is the value to convert the HX711 ADC output to a known wt.
  // .tare defaults to 10 reads; sets _offset to the average; returns _offset*_scale, (_scale  is initially 1)
  // override to 50 reads, ~ 5 seconds tare sets the offset
  Serial.print("Wait for values to complete; will take ~");
  Serial.print(tests/(1.1*tests/9)); //turns out the averaging process takes a couple seconds???
  Serial.println(" seconds.");
  delay(50);
  //To understand what is going on read different values and print them
  rawReads = 0;
  for (int i=0; i<10; i++) {
    rawReads = rawReads + scale.read(); //get raw value
  }
  Serial.print("Raw zero wt reads (avg of 10): ");
  Serial.println(rawReads/10);

  scale.tare(tests); 
  Serial.print("UNITS: ");
  //get_units returns offset*_scale// at this point scale is 1
  Serial.println(scale.get_units(tests)); //
  Serial.print("Offset from ");
  Serial.print(tests);
  Serial.print(" reads: ");
  Serial.println(scale.get_offset()); //as long
  Serial.print("Raw zero read corrected for offset (avg of 10): " );
  Serial.println(scale.get_value(10)); //read value, corrected for offset.)

  Serial.println("\nPut a weight on the scale; Enter weight as floating point value; Press Send when done.");
  wt = 0;
  while(!Serial.available());
  while(Serial.available()) {
    wt = Serial.parseFloat(); //this is blocking
  }
 // MAKE SURE YOU SET "NO LINE ENDING" AT BOTTOM RIGHT OF SERIAL MONITOR
  Serial.print("Calibration weight that was input: ");
  Serial.println(wt);
  if (wt > 0) {
    tests = 100;
    //calibrate to known weight
    scale.calibrate_scale(wt, tests);
    rawReads = 0;
    for (int i=0; i<10; i++) {
      rawReads =rawReads + scale.read(); //get raw value
  }
    Serial.print("Raw calibration wt reads (avg of 10):");
    Serial.println(rawReads/10);
    Serial.print("Raw calibration wt reads corrected for offset (avg of 10): " );
    Serial.println(scale.get_value(10)); //read value, corrected for offset.)
    Serial.print("UNITS gms: ");
    Serial.println(scale.get_units(tests));
    Serial.print("Offset: ");
    Serial.println(scale.get_offset());
    Serial.print("Scale factor: ");
    Serial.println(scale.get_scale());
    Serial.println("Dumping wt readings");
    //Read out separate weight readings for analysis.
    for (int i=0; i<tests; i++) {
      Serial.println(scale.get_units());
    }
  }

  else {
    Serial.println("Input weight was not received");
  }
  Serial.print("Going to run 20 baseline tests of 100 values. Remove wt; send any key to proceed");   
  while(!Serial.available()); //wait for key pressed, must be in send line for focus.
  while(Serial.available()) Serial.read();  
  Serial.println("Offsets:");
  for (int j=0; j<20; j++) {
    //get_units returns offset*_scale// at this point scale is 1
    scale.tare(tests);
    Serial.println(scale.get_offset()); //as long
  }
}

void loop(){}
