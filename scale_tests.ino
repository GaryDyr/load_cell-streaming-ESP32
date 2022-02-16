/* 
  Used to test variance of weight readings at various average read per reading
  Dumps to Serial Monitor only. No button or encoder used.
  THIS SETS THE SCALE FACTOR AND TARE AT SPECIFIC VALUES. SO WEIGHTS WILL BE CLOSE
  BUT NOT TRUE WEIGHT, BECAUSE TARE IS NOT ALLOWED TO FLOAT.
  loop() has a commented out process for getting a tare value. However, have to 
  remove the weight on the fly, or not use a weight for entire run.
  uses the Tillaart HX711 load cell library 
  https://github.com/RobTillaart/HX711
  Modified from library example to make some steps clearer, and to run sensitivity tests
  Assumes that the calibration scale is stable,long term, and variations are do
  to an added known weight - container_wt which is determined independently, either from a
  previously accurately determined weight. 
  In this case the weight of a container was established and used as the standard. 
*/

#include "HX711.h"

HX711 scale;

uint8_t dataPin = 19; //DT
uint8_t clockPin = 21; //CLK

float wt;
int tests;
long t_interval = 120000; //every two minutes
float nowtime = 0.0;
unsigned long previousMillis = 0;    

long offset_factor = 12698;
float scale_factor = -214.469;
float container_wt = 214.1; //this varies with type of container.
float margin = 0.5; //this is allowable margin of zero calibration error.
//
// output_type
uint8_t output_type = 0;

void setup(){
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting....");

  scale.begin(dataPin, clockPin);
  scale.set_scale(scale_factor); 
  scale.set_offset(offset_factor);
  //Check what current tare wt is averaging 3 values 
  wt = scale.get_units(3);
    tests = 100;
  Serial.println("\nPut a weight on the scale; Enter weight as floating point value; Press Send when done.");
  wt = 0;
  while(!Serial.available());
  while(Serial.available()) {
    wt = Serial.parseFloat(); //this is blocking input
  }
  // MAKE SURE YOU SET "NO LINE ENDING" AT BOTTOM RIGHT OF SERIAL MONITOR
  Serial.print("Calibration weight that was input: ");
  Serial.println(wt);
 
  //set test sequence parameters for looping
  for (int j=0; j<3; j++){
    if (j==0){
      tests = 3;
      Serial.println("running 20 x 3");
    }
    if (j==1) {
      tests = 100;
      Serial.println("Running 20 x 100");
     }
    
    if (j==2) {
      tests = 1;
      Serial.println("Running 20 x 1");
     } 
    for (int i=0; i<20; i++){
      Serial.println(scale.get_units(tests));
    }
  }
Serial.println("Finished");
}

void loop(){
  //Uncomment to just get offset; does 20 100 average reads  
 /* unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= t_interval) {
    nowtime = nowtime  + (currentMillis - previousMillis)/1000.;
    previousMillis = currentMillis;
    Serial.print(nowtime);
    Serial.print(", ");
    Serial.println(scale.get_units(tests));
  }
*/
}