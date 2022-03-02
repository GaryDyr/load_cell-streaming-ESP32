/* 
   uses the Tillaart HX711 load cell library 
  https://github.com/RobTillaart/HX711
  Modified heavily from library example. This is a streaming type of application, 
  monitoring up to an array of size 1000 readings and t1. 
  This is roughly equivalent to data collection of 3 wts/s or up to 5 mins of data.
  Data is not continuously sent to Google Sheet. It is sent out, in a burst
  mode at conclusion of run to the designated Google Script and ultimately GSheet.
  The sketch assumes that the calibration scale factor is long term stable,  
  and variations only due to offset. This was found to be true with the 
  tested load cell and HX711 used here. The sketch assumes anything that is on it
  at the start of a run should be tared (zeroed out, so the _offset variable in 
  the library. The first time value as the starting weight, subtracting all other 
  points from this initial offset.

  CALIBRATION PROCESS
  There is a calibration routine built in, but keep in mind that this sketch was 
  designed to operate without a display or the Serial Monitor (except for testing). 
  To do a calibration:
  Have at the ready your known calibrated weight. 
  Find in this sketch the calibrated_wt variable and add the value.
  The steps of the calibration are guided by the ESP32 onboard led in the following 
  manner:
    1. Turn the encorder ONCE COUNTER CLOCKWISE, THEN DEPRESS THE BUTTON
    2. When the onboard led starts to blink with at 1 second rate, remove any extraneous
       weight, that will not be part of tare. You have 15 s to complete this operation.
    3. When the led begins blinking at a faster rate of 3 time/second, add the calibrated
       weight to the scale. You have a 15 s interval to do this. 
       When the led turns off, the calibration is finished, 
This new calibration factor is not permanently stored, but will be sent to the Google Sheet, 
or if testing, to the Serial Monitor.
You do not need to return the rotary encoder to its original position.
OPERATION STATUS SIGNALS.
Because this project is designed to operate without a display, it uses the onboard ESP32
module led to indicate what is occuring, or will occur.
ALL OPERATIONS ONLY TRANSITION AFTER A BUTTON PRESS.
Idling - Only red led lit.
Press button to star data collection, and stop data collection
Steady blue led - data collection occuring, or during data transmission. When stopping
data collection. transition to end data is automatic, Intial fast pulse will indicate
if the stop button press was "heard". Solid blue will then indicate data transmission 
is occuring. When led goes off operation is complete. 
*/

//#include "AiEsp32RotaryEncoder.h"
#include "Button2.h" //  https://github.com/LennartHennigs/Button2
#include "ESPRotary.h"
#include <HX711.h>
#include <WiFi.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include <stdlib.h>

//Define HX711 pins
uint8_t dataPin = 19;  //DT
uint8_t clockPin = 21; //CLK
#define ONBOARD_LED  2
//Define Rotary Encoder pins
#define ROTARY_ENCODER_CLK_PIN 4 //called A in library
#define ROTARY_ENCODER_DT_PIN 23  //called B in library
#define ROTARY_ENCODER_BUTTON_PIN 22

/* 27 put -1 of Rotary encoder Vcc is connected directly to 3,3V;
// else you can use declared output pin for powering rotary encoder */

//depending on your encoder - try 1,2 or 4 to get expected behaviour
#define CLICKS_PER_STEP 4 //this is really a divisor; if 1 showed 4 steps.

HX711 scale;
ESPRotary r;
Button2 b;

// ******************SCALE INFO*****************************************
String t1;
String wts;
String dataString;
int tests; //used by calibration
//Next two values were determined independently using scale_calibrate.ino and
//scale_tests.ino
long offset_factor = 12698; //This will be determined at run time.
float scale_factor = -214.469;
float calibration_wt = 214.1; //this varies with type of container.
//float margin = 0.8; //this is allowable margin of zero calibration error.
//float calibrated_wt = 104.4; // used as standard if calibration needed.
int attempts; //used to track connection issues to lan router
int8_t rotar_state; //not currently used.will show -255 to +255.
int8_t rotar_direction;
int8_t btn_click;

int8_t process_state;
int count;
const uint8_t PRINT_TO_SERIAL = 1;
const uint8_t SEND_TO_GS = 2;
//
// CHANGE THIS FOR INTERNAL TESTING OR SENDING TO GOOGLE SHEET
//CHOICES: SEND_TO_GS || PRINT_TO_SERIAL
uint8_t output_type = SEND_TO_GS; //PRINT_TO_SERIAL;
//uint8_t output_type = PRINT_TO_SERIAL;

// ******************INTERNET CONNECTION STUFF*****************************************
const char* ssid     = "Your_WiFi_router_name";      //"<REPLASE_YOUR_WIFI_SSID>"
const char* password = "Your WiFi_router_password"; //<REPLASE_YOUR_WIFI_PASSWORD>"
//*******************END INTERNET CONNECTION STUFF*************************************

//********************TIME RELATED VARIABLES*****************************************
// future use bootcount; may be needed with time fcn; store value in RTC memory
// RTC_DATA_ATTR int bootCount = 0; 
unsigned long startTime; //used in testing to check various timing intervals
unsigned long finishTime; // used to hold testing interval time in us
//Variable to hold current epoch time (us)
unsigned long nowTime;
unsigned long previousMillis;
unsigned long currentMillis;
const char* ntpServer = "pool.ntp.org"; 
const long  gmtOffset_sec = -6*3600; //CST time offset
const int   daylightOffset_sec = 3600;
struct tm timeinfo; //output from time.h
time_t now;
char timestr[32];
//******************END TIME RELATED VARIABLES*******************************************

//************GOOGLE SHEETS VARIABLES******************************************************
const int httpsPort = 443; //this is the https port.
WiFiClientSecure gsclient;
//use the Google Script ID, not the Google Sheet ID this was anonymous
const char* host = "https://script.google.com";

//version 10              
String GAS_ID = "GOOGLE_SCRIPT_ID_you_were_assigned_on_Deploying_code.gs";
//NOT USED: For reference const char* host = "https://script.google.com/macros/s/GOOGLE_SCRIPT_ID_you_were_assigned_on_Deploying_code.gs/exec";

//For Test Deployment:
//fingerprint not used; using setInsecure
//const char* fingerprint = "46 B2 C3 44 9C 59 09 8B 01 B6 F8 BD 4C FB 00 74 91 2F EF F6";
//************END GOOGLE SHEETS VARIABLES**************************************************

//funciton to get time from NTP server pool
void printLocalTime() {
  //does not get a new NTP time from an NTP server. Reads ESP32 RTC time and 
  //configured as desired. Appears configTime is the function that initializes 
  //the time from the ntp server(s), offsets, and places in RTC. Apparently also sets 
  //a time update cycle function, which defaults to one hour.
  //The function getLocalTime may be reading the RTC time timeinfo
  //structure.  We access the time through the RTC, using getLocalTime. 
  //GetLocalTime is not true world time, unless near the instant after
  //an internal update...I guess. For most stuff the RTC ESP time, is close enough.
  //see https://forum.arduino.cc/t/esp32-time-library-sync-interval/902588

  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
 //Serial.println(&timeinfo, "%a, %b %d %Y %H:%M:%S"); //abrev. day name, abrev. mon., day of month, 4 digit year, 24 hrs, 0-60 min, 0-60 s
// set the current time to a string variable...timestr
  strftime(timestr, sizeof timestr, "%Y.%m.%d:%H.%M.%S", &timeinfo); 
  Serial.println(timestr);
}

unsigned long getEpochTime() {
  //used to get seconds from  01/01/1970
  if(!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return 0;
  }
  //get the epoch time (sec since 1970
  time(&now);
  //Serial.println(now);
  return now;
}
void blinker(int total_blink_time, int blink_interval) {
  //used to biink the led to indicate status or issues
  int ledState = LOW; 
  int blinkMillis;
  blinkMillis = millis();
  previousMillis = millis();
  currentMillis = millis();
  while (millis() - previousMillis < total_blink_time) {
    currentMillis = millis();   
    if (currentMillis - blinkMillis >= blink_interval) {
      //save last time as current for next cycle
      blinkMillis = currentMillis;
       if (ledState == LOW) {
        ledState = HIGH;
      } else {
        ledState = LOW;
      }
    digitalWrite(ONBOARD_LED, ledState);
    }
  }
}//end blinker

//runs if output_type = PRINT_TO_SERIAL
void printData() { //3 min digits; 2 decimals; 
  //gets tricky; to avoid blowing up the heap we have to minimize the output
  //String CANNOT BE USED; MUST USE A CHAR ARRAY.
  //can limit the size of the char array, but limiting each value to the minimum.
  //for times we only want two decimal places. The times are all positive and 
  //the maximum char length should be 999.99 or six chars. wts could be up to 
  //9999.99 or another 7 chars. Array is then 500*6 + 500*7 = 6500 chars
  //char dataString[6600]//generate the data stream.
  //must strip off last comma on t1 and wts.
  String comma = ",";
  String sfactr =  "scalefactor=[";
  String tkey  = "]&times=[";
  String wkey = "]&wts=[";
  String endit = "]";
  //truncate last commas from time and wt data strings.
  int alen;
  alen = t1.length()-1;
  t1.remove(alen);
  delay(10);
   alen = wts.length()-1;
  wts.remove(alen);
  delay(10);
  dataString = sfactr + String(scale_factor)  + tkey;
  dataString = dataString + t1;
  dataString = dataString + wkey + wts + endit;
  Serial.print("lengths:");
  Serial.print("time string length=");
  Serial.println(t1.length());
  Serial.print("wts string length=");
  Serial.println(wts.length());
  Serial.print("Final dataString length=");
  Serial.println(dataString.length());
  
  //Used for testing string out is proper
  if (output_type == PRINT_TO_SERIAL) { //SEND_TO_GS; 
    if (dataString.length() > 100) {
      for (int n = 0; n<dataString.length()-100; n+=100) {
      Serial.println(dataString.substring(n, n+100));
      }
    //show remainder
    Serial.println(dataString.substring(dataString.length() - (dataString.length() % 100), dataString.length()));   
    }
  }
  delay(1000);   
 }

//send trigger data to Google Script/Google Sheet
//runs if output_type = PRINT_TO_GS
void sendData() {
  Serial.println ("in sendData");
  int interval = 200; //will blink 1/s
  int waittime = 10000; //10 s
  int ledState = HIGH;
  
  //generate the data stream.
  printData(); 
  /*
  // used for testing proper data string
  if (dataString.length() > 100) {
      for (int n = 0; n<dataString.length()-100; n+=100) {
      Serial.println(dataString.substring(n, n+100));
      }
    //show remainder
    Serial.println(dataString.substring(dataString.length() - (dataString.length() % 100), dataString.length()));   
  }
  */
  Serial.println("connecting to WiFi");
  attempts = 0;
  WiFi.mode(WIFI_STA);
  startTime = micros(); 
  WiFi.begin(ssid, password);
  //Connect to router/lan
  //allow up to 10 attempts to connect  @ 250ms = 5s to get connected.
  while ((WiFi.status() != WL_CONNECTED) && (attempts <= 20)) {
    delay(500); //may be less, but play it safe
    Serial.print(".");
    ++attempts;
    //Connection not established in 10s, stop.  
    if (attempts == 20) {
      Serial.println("Failed to connect to router.");
     //do rapid blink for 10 s signify transmission issue.
     waittime = 100;
     interval = 100;
     blinker(waittime, interval); 
      break;
    }
  }
  //get time to check how long it took to connect to router
  finishTime = micros() - startTime;
  Serial.println("");
  Serial.print("Connected to router with IP address: ");
  Serial.println(WiFi.localIP());  //Print the local IP
  Serial.print("; time to connect (us) was: ");
  Serial.print(finishTime);
  Serial.print(", in ");
  Serial.print(attempts);
  Serial.println(" attempts.");
  //found that for my lan took less than 1 s with at most 7 attemps to connect, but that is with 500 ms delay. 
  //get NTP time and how long to get time, it is the basis of how often we can send
  //emails
  startTime = micros();
  //init and fill ESP RTC memory with the latest NTP time from the server pool
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); 
  //get the time from the RTC; not used right now; another indicator of a problem connection to internet.
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain NTP time");
  }  
  finishTime = micros() - startTime;
  Serial.print("NTP access Time, us: ");
  Serial.println(finishTime);
  printLocalTime(); //prints last NTP time that was stored.
  //the config and getLocalTime process_state too a max of 150 ms, although average is about 70 ms.
  //get current Epoch time 
  nowTime = getEpochTime(); //access the last local time configured in RTC, i.e, now()
  if (nowTime != 0) {
     Serial.print("epoch time stored in RTC: ");
  }
  Serial.print("connecting to Google Script: ");
  Serial.println(host);
  delay(3000);
  ESP.restart(); //reboot the ESP to clear memory for next cycle
  //memory check stuff for testing
  /*
  uint32_t fhp = esp_get_free_heap_size();
  uint32_t fih = esp_get_free_internal_heap_size();
  uint32_t fhs = esp_get_minimum_free_heap_size();
  Serial.print("fhp: ");
  Serial.println(fhp);
  Serial.print("fih: ");
  Serial.println(fih);
  Serial.print("fhs: ");
  Serial.println(fhs);
  */
  
  //if not setting setInsecure; we have to get fingerprint, which changes regularly, and is unacceptable.
  gsclient.setInsecure();  // weakens security a bit.
  String url = "";
  url = String("/macros/s/") + GAS_ID + String("/exec?") + dataString;
  if (!gsclient.connect("script.google.com", httpsPort)) {
    Serial.println("Connection to script failed");
    gsclient.stop();
    return;
  }else {
    Serial.println("Connected to script.");
  }
 /* don"t use fingerprint, should work without it if using insecure
  if (gsclient.verify(fingerprint, host)) {
    Serial.println("certificate matches");
  } else {
    Serial.println("certificate doesn"t match");
  }
  //GET is used to request data, and uses a simple query string format after the "?"
 */
 
   gsclient.print(String("GET ") + String(url) + " HTTP/1.1\r\n" +
                 "Host: " + "script.google.com" + "\r\n" +
                 "User-Agent: BuildFailureDetectorESP8266\r\n" + 
                 "Connection: close\r\n\r\n");
  Serial.println("request sent");
  delay(2000);
  while (gsclient.connected()) {
    String line = gsclient.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = gsclient.readStringUntil('\n');
  Serial.print("reply was: ");
  
  Serial.println(line);
  //For whatever reason, initially saw "state", but later code was 287
  //According to HTTP registry codesin range 2xx are success.
    if (line.startsWith("{\"state\":\"success\"") || (line.startsWith("2"))) {
    Serial.println("Data transfer successfull!");
  } else {
    Serial.println("Data transfer failed");
    //indicate some sort of transfer failure.
    blinker(waittime, interval);
  }
  Serial.println("closing connection");
  gsclient.println("Connection:close"); 
  gsclient.stop();
}

void Calibrate_scale(){
  //Calibration sequence depends on LED blink changes. The test ESP32 module, 
  //has two leds, one for power and another normally off, which is blue and 
  //controlled by module pin 2. /THE CALIBRATED WEIGHT MUST  BE SPECIFIED IN 
  //IN VARIABLE - calibration_wt.
  //A 600 msec LED pulsing for 10 s is notice to set tare conditions. Remove 
  //all stuff except that which is to be included in tare.
  //When pulsing shifts to 300 ms (for 10 sec) add known wt to pan.
  tests = 10;
  int interval; 
  int waittime = 10000; //10 s
  //do slow blink via blink without delay
  interval = 600;
  blinker(waittime, interval); 
   scale.tare(tests); //must run; sets  _offset for  calibate_scale
  interval = 300;
  blinker(waittime, interval); 
  tests = 10;
  //calibrate to known weight
  scale.calibrate_scale(calibration_wt, tests);
  // for testing
  Serial.print("calculated UNITS gms: ");
  Serial.println(scale.get_units(tests));
  Serial.print("Scale factor: ");
  Serial.println(scale.get_scale());
   //reset the encoder position to zero.
  process_state = 0;
  digitalWrite(ONBOARD_LED, LOW);
} //end calibration

void rotary_onButtonClick(){
  //Controls scale functioning;
  //process_state conditions:
  //  0 - idle only watch for interrupts; changes process_state to 1
  //  1 - take data on first click; stop and send/print data on next click
  // -1 - calibrate scale
	//static unsigned long lastTimePressed = 0;
	Serial.print("button pressed; Process state ");
  Serial.print(process_state);
  Serial.print(" rotar_state: ");
  Serial.println(rotar_state);
  // to get current process_state
  // to get here a button was clicked; change operation
  // on return to loop will start reading scale
  switch (process_state) {
      case 0: //start data collection;
        scale.tare(10);
        previousMillis = millis();
        process_state = 1;
        count = 0;
        wts = "";
        t1 = "";
        digitalWrite(ONBOARD_LED,HIGH);         
        break;
      case 1:
        // was collecting data; send data out; return to idle 
        //blink to acknowledge call to output;  
        digitalWrite(ONBOARD_LED, LOW);
        for (int i=0; i<5; i++) {
          digitalWrite(ONBOARD_LED, LOW);
          delay(300);
          digitalWrite(ONBOARD_LED, HIGH);
          delay(200);
        }
         digitalWrite(ONBOARD_LED, HIGH);
        if (output_type == PRINT_TO_SERIAL) {
          printData();
        } else {
           Serial.println("Data to WiFi");
           sendData();
        }
         process_state = 0; 
         //reset arrays to zero
         //memset(t1, 0.0, sizeof(t1));
         //memset(wts, 0.0, sizeof(wts));
         count = 1;
         digitalWrite(ONBOARD_LED, LOW);
         delay(20);
         rotar_state = 0;
         rotar_direction = 0;
         //reset wt string;
         wts = "";
         btn_click = 0;
         //re-zero the encoder position 
         //r.resetPosition();       
         break;
       case -1:
        //calibrate mode
        Calibrate_scale();
        //reset states
        process_state = 0;
        rotar_state = 0;
        rotar_direction = 0;
        wts = "";
        btn_click = 0;
        //r.resetPosition();
        break;
        digitalWrite(ONBOARD_LED, LOW);
      }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(ONBOARD_LED, OUTPUT);
  Serial.println("Starting....");
  rotar_state = 0; //Default 0; do nothing; what for click to process.
  process_state = 0; // set scale idle..not reading
  rotar_direction = 0;

  // setup ESPRotary Encoder
  r.begin(ROTARY_ENCODER_DT_PIN, ROTARY_ENCODER_CLK_PIN, CLICKS_PER_STEP);
  r.setChangedHandler(rotate);
  r.setLeftRotationHandler(showDirection);
  r.setRightRotationHandler(showDirection);
  //initiate rotary button part from separate library
  b.begin(ROTARY_ENCODER_BUTTON_PIN);
  b.setTapHandler(click);
  b.setLongClickHandler(resetPosition);

	//set scale parameters
  scale.begin(dataPin, clockPin);
  scale.set_scale(scale_factor); 
  scale.set_offset(offset_factor);
  //Check current tare wt average of 10 values, about 1 s
  scale.tare(10);
  Serial.print("tare: ");
  Serial.println(scale.get_offset());
  previousMillis = millis();
  //memory check
  /*
  uint32_t fhp = esp_get_free_heap_size();
  uint32_t fih = esp_get_free_internal_heap_size();
  uint32_t fhs = esp_get_minimum_free_heap_size();
  Serial.print("fhp: ");
  Serial.println(fhp);
  Serial.print("fih: ");
  Serial.println(fih);
  Serial.print("fhs: ");
  Serial.println(fhs);
  */ 
} //end setup

void loop(){
  // process_state 0 is idling; 1 is start reading.
  //controlled by 
   String awt = "";
   String atime = "";
   float t;
   float wt_value; 
   //in loop call your custom function which will process_state rotary encoder values
   //look for encoder changes;
  //read the rotor and button state, respectively.
  r.loop();
  b.loop(); 
  //click() funcion controls going to rotary_OnButtionClick, which sets/resets conditions
  if (btn_click == 1) {
    //There are three process states: 0, 1 and -1, but only calibrate via
    //left turn is controlled by user. The other states are toggled via 
    //button presses. Because of possibe bouncing, rotar_state might jump. 
    //The "safest", but not most robust solution is just read if the 
    //rotation has been CCW (left turn; decrement.
    //NOTE ALL OPERATIONSREQUIRE NOT ONLY A TURN OF ENCODER, BUT A BUTTON CLICK
    //THE POSITION IS AUTOMATICALLY RESET TO ZERO AFTER CALIBRATION
    //In practice, the encoder did seem to function accurately with the
    //ESPRotary library.
    Serial.print("rotary state: " );
    Serial.print(rotar_state);
    Serial.print("; rotar change: ");
    Serial.println(rotar_direction);
    if (rotar_direction < 0) { //i.e., CCW
      process_state = -1;
      rotary_onButtonClick(); //handles what happens
    }
    //reset btn state
    btn_click = 0;
  }
    //should be ~ 3 pts/s
    if (process_state == 1) {
    if (count < 500) {
      wt_value = scale.get_units(3);
      awt = String(wt_value,2); //convert wt to string
      wts += awt;
      wts += ",";
      //Serial.println(wts);
      currentMillis = millis();
      atime = String((currentMillis - previousMillis)/1000.,2); 
      Serial.print(count);
      Serial.print("; ");
      Serial.print(atime);
      Serial.print("; ");
      Serial.println(awt);
      t1 += atime;
      t1 += ",";
      count++;    
    }
  }  
  delay(10);
}
// on change these are the callbacks to get info.
void rotate(ESPRotary& r) {
 Serial.print("rotar position: ");
 Serial.println(r.getPosition());
  rotar_state = r.getPosition(); 
}

// on left or right rotation
void showDirection(ESPRotary& r) {
 Serial.println(r.directionToString(r.getDirection()));
  rotar_direction = r.getDirection();
  
}
 
// single click
void click(Button2& btn) {
  Serial.println("Btn Click!");
  btn_click = 1;
  rotary_onButtonClick();
}

//long click
void resetPosition(Button2& btn) {
  Serial.println("Long Click!");
  r.resetPosition();
  Serial.println("Reset!");

}
