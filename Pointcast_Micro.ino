/*
  Pointcast_Micro.ino

2015-09-17 V1.0.0 Created


contact sd661@cam.ac.uk

//FAILURE MODES//
//-occasionally gets stuck trying to connect, no print out after getting the ip address, eventually times out with wdt
//-occasionally can't get DHCP resets loop and carries on (not full reset failure)
//-for some reason, the CC3000 will often randomly reset about a minute after starting it...
//-sometimes gets stuck mid way through sending the data... so between connection to the gateway and trying to send the json, times out with wdt...

*/

/**************************************************************************/
// Init
/**************************************************************************/

#include <SPI.h>
#include <Adafruit_CC3000.h>
#include <avr/wdt.h>
#include <limits.h>     //used for watchdog timer extension
#include "Device_specific_settings.h"
    
//Set up the CC3000 shield 
      // These are the interrupt and control pins
      #define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
      // These can be any two pins
      #define ADAFRUIT_CC3000_VBAT  5
      #define ADAFRUIT_CC3000_CS    10
      // Use hardware SPI for the remaining pins
      // On an UNO, SCK = 13, MISO = 12, and MOSI = 11
      Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                               SPI_CLOCK_DIVIDER); // you can change this clock speed
      #define IDLE_TIMEOUT_MS  3000


// Interrupt mode for Geiger counter and count
const int interruptMode = FALLING;
unsigned long counts_per_sample; //increases each time an event happens

void onPulse()                    //ISR for Geiger interrupts
  {
    counts_per_sample++;
  }

//Settings for wdt
//watchdog timer setup to be 32s
volatile int wdt_counter;      // Count number of times ISR is called.
volatile int wdt_countmax = 4; //4*8s=32s


//Set up control variables
typedef struct
{
    unsigned char state;
    unsigned char conn_fail_cnt;
    unsigned char conn_success_cnt;
    unsigned char DHCP_fail_cnt;
} devctrl_t;     
enum states
{
    NORMAL = 0,
    RESET = 1
};
static devctrl_t ctrl;

  
//Time variables
unsigned long updateIntervalInMillis;     
long lastConnectionTime = 0;              // Variable to track time since last connected to gateway
unsigned long elapsedTime(unsigned long startTime);   //function definition for elapsed time function


//Initiate Data sending variables
#define SENT_SZ 150  //Max size of JSON buffer -the CC3000 can't actually handle more than about 100 chars
char CPM_string[15];
char json_buf[200];
uint32_t ip=0; 

//the timeout for aquiring DHCP
int DHCP_count; 

/**************************************************************************/
// Setup()
/**************************************************************************/
void setup() {
       
        Serial.begin(115200);                             
        Serial.println(F("\n\nHello Pointcast micro"));
        if (!cc3000.begin()) {                                //connecting to the CC3000 shield
          Serial.println(F("Check connections please"));
          while (1);
        }
        
        // init the control info
        memset(&ctrl, 0, sizeof(devctrl_t));
      
         
        // enable watchdog to allow reset if anything goes wrong      
        watchdogEnable(); // set up watchdog timer in interrupt-only mode, if need to turn off: wdt_disable();
        
        //Serial.println(F("Deleting old connection profiles"));
        if (!cc3000.deleteProfiles()) {
          Serial.println(F("Failed!"));
          while(1);
        }

        
        // Attach an interrupt to the digital pin and start counting
        // Note:
        // Most Arduino boards have two external interrupts:
        // numbers 0 (on digital pin 2) and 1 (on digital pin 3) 
        pinMode(3, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(2), onPulse, interruptMode);                // comment out to disable the GM Tube

        // Calculate update time in ms,
        updateIntervalInMillis = updateIntervalInSeconds * 1000;              
                
        wdt_reset();
      
}

/**************************************************************************/
// Send data to server
/**************************************************************************/

void SendDataToServer(float CPM){

//set up the json
       dtostrf(CPM, 0, 0, CPM_string);
               
       memset(json_buf, 0, SENT_SZ);
       sprintf_P(json_buf, PSTR("{\"longitude\":\"%s\",\"latitude\":\"%s\",\"value\":\"%s\",\"unit\":\"cpm\",\"device_id\":\"%s\"}"), \
                       longitude, \
                       latitude, \
                       CPM_string, \
                       user_id);
       
        
        json_buf[strlen(json_buf)] = '\0';
        //Serial.print(json_buf);
                

//connect to WiFi and send data
       Serial.print(F("Connect to ")); Serial.println(WLAN_SSID); //Between cc3000 and wifi network
        if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
          Serial.println(F("Failed to connect to the WiFi network"));
          return;
        }
       // else Serial.println(F("Connection succesful"));

      DHCP_count=0;
      while (!cc3000.checkDHCP()) //Between cc3000 and beginning of internet (setting up?)
      {
        delay(100); 
       DHCP_count++;
       if (DHCP_count==100){
        Serial.println(F("Couldn't get DHCP, reset..."));
        ctrl.DHCP_fail_cnt++;
        return;
       }
      }
      Serial.println(F("DHCP Acquired"));
      

      
      uint32_t s1 = 107;   //IP is 107.161.164.166              //Code calculates ip in number format for the function: cc3000.connectTCP(ip, 80)
      uint32_t s2 = 161;                                        //couldn't find an easier way to do this
      uint32_t s3 = 164;                                        //For IP:107.161.164.166, ip=1805951398
      uint32_t s4 = 163;
      uint32_t ip = (s1 << 24) | (s2 << 16) | (s3 << 8) | s4;
       
      Serial.println(F("ip address:"));
      cc3000.printIPdotsRev(ip);                                //don't know why, but this is necessary for the connection to work.
      Serial.println();
      
      Adafruit_CC3000_Client client = cc3000.connectTCP(ip, 80);
      if (client.connected()) {
          lastConnectionTime = millis();        //set last connection variable
          Serial.println(F("Sending data"));
          client.fastrprintln(F("POST /scripts/indextest.php?api_key=AzQLKPwQqkyCTDGZHSdy HTTP/1.1"));
          client.fastrprintln(F("Accept: application/json"));
          client.fastrprintln(F("Host: 107.161.164.166"));
          client.fastrprint(F("Content-Length: "));
          client.println(strlen(json_buf));
          client.fastrprintln(F("Content-Type: application/json"));
          client.println();  
          //Serial.println(F("Attempt to send json"));
          client.fastrprintln(json_buf); 
          Serial.print(F("JSON sent \nresponse: ")); 
      }
      else {
        Serial.println(F("Connection failed"));
        ctrl.conn_fail_cnt++;
        if (ctrl.conn_fail_cnt >= MAX_FAILED_CONNS){
          ctrl.state = RESET;
          ctrl.conn_fail_cnt=0;
        }
      }
      
      while (client.connected()){
        while (client.available()){
          char c = client.read();
          Serial.print(c);                  //to print the response of the gateway 
          if (c=='K'){                      //if prints HTTP/1.1 200 OK stops full printout and increases successfull send count.
            ctrl.conn_success_cnt++;
            Serial.println();
            client.close(); 
          }
        }
      }
      
      if (client.connected()) client.close();
      
      
      cc3000.disconnect(); //not the end of the 'begin' but rather the end of the connection to wifi
      Serial.println(F("Disconnected"));



}


/**************************************************************************/
// loop()
/**************************************************************************/



void loop() {

      if (ctrl.state != RESET){
            wdt_reset();  //Stag resets counter for wdt (ie 8s from now)
            wdt_counter=0;
      }
      else return;
      if (elapsedTime(lastConnectionTime) < updateIntervalInMillis)
      {
        return;
      }
      Serial.print(F("\n\tSuccessful sends: ")); Serial.println(ctrl.conn_success_cnt);
      Serial.print(F("\tFailed sends: ")); Serial.println(ctrl.conn_fail_cnt);
      Serial.print(F("\tDHCP fails: ")); Serial.println(ctrl.DHCP_fail_cnt);

         
      
      float CPM = (float)counts_per_sample / ((float)updateIntervalInSeconds/60.0);
      
      //Serial.print("\t\tcount: "); Serial.println(counts_per_sample);
      
      counts_per_sample = 0;
      Serial.print("\t\tCPM = "); Serial.println(CPM);  //Sadd
      SendDataToServer(CPM);
}

/******************************************************************************************************/
//set up the watchdog timer with a 32s delay (long delay needed for time it takes to 
/*****************************************************************************************************/

void watchdogEnable()
{
 wdt_counter=0;
 cli();                              // disable interrupts
 MCUSR = 0;                          // reset status register flags
  WDTCSR |= 0b00011000;              // Put timer in interrupt-only mode:                                        
                                     // Set WDCE (5th from left) and WDE (4th from left) to enter config mode,  WDCE watchdog clear? enable, WDE watchdog enable?
                                     // using bitwise OR assignment (leaves other bits unchanged).
 WDTCSR =  0b01000000 | 0b100001;    // set WDIE (interrupt enable...7th from left, on left side of bar)
                                     // clr WDE (reset enable...4th from left)
                                     // and set delay interval (right side of bar) to 8 seconds,
                                     // using bitwise OR operator.
 sei();                              // re-enable interrupts
 //wdt_reset();                      // this is not needed...timer starts without it
}

// watchdog timer interrupt service routine
ISR(WDT_vect) {     
 wdt_counter+=1;
 if (wdt_counter < wdt_countmax)
 {
   wdt_reset(); // start timer again (still in interrupt-only mode)
 }
 else             // then change timer to reset-only mode with short (16 ms) fuse
 {
  MCUSR = 0;                          // reset flags

                                       // Put timer in reset-only mode:
   WDTCSR |= 0b00011000;               // Enter config mode.
   WDTCSR =  0b00001000 | 0b000000;    // clr WDIE (interrupt enable...7th from left)
                                       // set WDE (reset enable...4th from left), and set delay interval
                                       // reset system in 16 ms...
                                       // unless wdt_disable() in loop() is reached first

   //wdt_reset(); // not needed
 }
}


/**************************************************************************/
// calculate elapsed time, taking into account rollovers
/**************************************************************************/

unsigned long elapsedTime(unsigned long startTime)
{
  unsigned long stopTime = millis();

  if (startTime >= stopTime)
  {
    return startTime - stopTime;
  }
  else
  {
    return (ULONG_MAX - (startTime - stopTime));
  }
}

