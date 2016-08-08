/*
The MIT License (MIT)
Copyright (c) 2016 Chris Gregg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

-------------------------------------------------------------------------------------

On start up:
  1. Tries to connect to saved wifi settings if they exist - Control LED fast flashes
  2. If button pressed for a more than 30sec, then reset wifi settings
  3. If no wifi or reset wifi settings go into configuration mode - Control LED very fast flashes
  4. Config mode - creates AP with SSID "BlynkSwitch", with IP 192.168.4.1
  5. Config mode - able to set and save SSID and password, and Blynk token
  6. Config mode - tries to connect to wifi
  7. Config mode times out after 3 mins
  8. If not connected, Blynk functions are not enabled and switch is offline only - key functions still work
  9. Goes into normal running mode - Control LED flashes normally

Running mode:
  1. LED starts off
  2. Double click toggle on or off
  3. Press and hold to dim

 */

#define DEBUG
#include <DebugUtils.h>

extern "C" {
#include "user_interface.h"
}

#include <BlynkSimpleEsp8266.h>
#include <EepromUtil.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include "Switch_v2.h"
#include "PWM_LED_control.h"


// Define GPIO pins and UART
// -------------------------

#define OUTPUT_PIN    0               // GPIO 0 is the output pin
#define INPUT_PIN     2               // GPIO 2 is the main input pin

// If DEBUG, serial is running, so control LED goes to unconnceted GPIO pin

#ifdef DEBUG
  #define CTRL_LED    5               
#else
  #define CTRL_LED    1               // Using TX port on ESP-01
#endif

const static int SERIAL_SPEED = 115200;          // Serial port speed


// Setup Output LEDs
// -----------------

const static int LED_UPRATE_RATE = 50;

const static int LED_DIM_NORMAL = 5;
const static int LED_DIM_FAST = 10;
const static int LED_DIM_VERYFAST = 20;

pwmLED outputLED( OUTPUT_PIN, false, 100, LED_DIM_NORMAL );                           // Main output LED
pwmLED ctrlLED( CTRL_LED, true, 0, LED_DIM_FAST );                                    // Control LED

Ticker updateLEDs;                 // LED update timer

void updateLEDtick()
{  
  ctrlLED.autoDim();               // Move control LED to next dim level
  outputLED.autoDim();             // Move output LED to next dim level
}


// Wifi Settings
// -------------

const static int WIFI_TIMEOUT = 180;              // Three minutes
const static int EEPROM_MAX = 512;                // Max spaced used in EEPROM
const static char SSID_NAME[] = "BlynkSwitch";

// Set up EEPROM usage
// Pairs of variable and address

char blynk_token[34];                 // Blynk tokcen
int add_blynk_token = 0;              // Address of token in EEPROM

// Setup WifiManager

WiFiManager wifiManager;  

// WiFiManager Callback Functions

bool shouldSaveConfig;                // has the config changed after running the captive portal

// Called when the config gets updated from the portal

void saveConfigCallback ()
{
  DEBUG_PRINTLN("Should save config");
  shouldSaveConfig = true;                // Need to save the new config to EEPROM
}

// Gets called when WiFiManager enters configuration mode

void configModeCallback (WiFiManager *myWiFiManager)
{
  DEBUG_PRINTLN("Entered config mode");
  DEBUG_PRINTLN(WiFi.softAPIP());
  DEBUG_PRINTLN(myWiFiManager->getConfigPortalSSID());    // If you used auto generated SSID, print it
  
  ctrlLED.setDimRate( LED_DIM_VERYFAST );                 // Entered config mode, make led toggle faster
}


// Reset function
// --------------

void doReset( bool hard = false )
{
  #ifdef DEBUG
    Serial.begin(SERIAL_SPEED);       // Turn on serial
  #endif
  
  ctrlLED.setDimRate( LED_DIM_VERYFAST );     // Make led toggle faster 
  
  DEBUG_PRINTLN( "Starting reset" );
  
  delay(1000);

  if( hard )
  {
   
    // Reset the Wifi settings

    DEBUG_PRINTLN( "Clearing settings" );

    wifiManager.resetSettings();
  
    delay(1000);
  }
  
  DEBUG_PRINTLN( "Reseting" );
  
  delay(5000);

  ESP.restart();      // This only works correctly if there has been a normal restart after a flash.  
  
  delay(1000); 
}


// Define Blynk environment
// ------------------------

#ifdef DEBUG
  #define BLYNK_PRINT Serial          // Comment this out to disable prints and save space
//  #define BLYNK_DEBUG               // Optional, this enables lots of prints
#endif

// Virtual pin assignments
#define BLYK_MAIN_LED   1             // Virtual pin used for showing main control input
#define BLYK_CTRL_LED   0             // Virtual pin showing normal operation
#define BLNK_MAIN_BTN   2             // Virtual pin to match main button
#define BLNK_DIMMER     3             // Virtual pin for dimmer slider
#define BLNK_GAUGE      4             // Virtual pin for return level
#define BLNK_RESET      30            // Virtual pin to trigger a reset
#define BLNK_HARDRESET  31            // Virtual pin to trigger a hard reset (clearing wifi settings)

const static int BLNK_FLASH_TIME = 15000;                       // Period of Blynk flash
const static char BLNK_PARAM_PROMPT[] = "Enter Blynk token";
const static char BLNK_PARAM_ID[] = "blnk_token";

// Functions called on Blynk actions

// Initiate reset

BLYNK_WRITE(BLNK_RESET)
{
  doReset();                    // Soft reset
}

// Initiate Hard Reset 

BLYNK_WRITE(BLNK_HARDRESET)
{
  doReset(true);                // Hard reset (clear settings)
}

// Blynk button pressed

BLYNK_WRITE(BLNK_MAIN_BTN)
{
  if( param.asInt() != 0 ) outputLED.toggleState();       // Toggle LED state
}

// Dimmer changed

BLYNK_WRITE(BLNK_DIMMER)
{
  outputLED.setLevel( param.asInt() );         // Virtual pin set 0-100
}

// Flash Blynk control LED

Ticker updateBlnkLED;
bool blnkFlash = false;

void blnkLEDtick()
{  
  Blynk.virtualWrite(BLYK_CTRL_LED, blnkFlash*255);       // Toggle state of control LED
  blnkFlash = !blnkFlash; 
}


// Switch functions
// ----------------

const static int LONG_PRESS = 20000;       // Need to press for 20s to initiate long press
const static int DEBOUNCE = 50;            // 50ms for switch debounce
const static int START_TIME = 10000;       // 10 secs at start up to go into config mode

Switch actionBtn(INPUT_PIN, INPUT, LOW, DEBOUNCE, LONG_PRESS );         // Setup switch management


// Main Setup
// ----------

bool isOnline = false;          // Did we initially get connecteed

void setup()
{  
  // Setup EEPROM
  EEPROM.begin(EEPROM_MAX);

  // Setup control LED
  updateLEDs.attach_ms(LED_UPRATE_RATE, updateLEDtick);   // start LED update timer
  ctrlLED.setDimRate( LED_DIM_FAST );                     // Flash LED for setup operation
  ctrlLED.dimLED(true,true);                              // Start cyclick flashing
  
  // Turn on serial
  #ifdef DEBUG
    Serial.begin(SERIAL_SPEED);
    DEBUG_PRINTLN( "\n\n" );
    DEBUG_PRINTVB("Debug ON");
  #else
    wifiManager.setDebugOutput(false);
  #endif

  DEBUG_PRINTLN("Waiting for config mode request");
  delay(1000);

  unsigned const long interval = START_TIME; // the time we need to wait
  unsigned long startMillis = millis();  
  
  while ((unsigned long)(millis() - startMillis) <= interval)
  {
    yield();
    actionBtn.poll();  

    // If button pressed, then extend time
    
    if(actionBtn.on()) startMillis = millis();

    // If long press then reset setttings

    if(actionBtn.longPress())
    {
      // Reset the Wifi settings
      DEBUG_PRINTLN( "Clearing settings" );
      wifiManager.resetSettings();

      break;     // Its a log press so keep going
    }
  }
  DEBUG_PRINTLN("Continue");
  
  // Add Captive Portal parameter for Blynk token
  WiFiManagerParameter custom_blynk_token(BLNK_PARAM_ID, BLNK_PARAM_PROMPT, blynk_token, 34);
  wifiManager.addParameter(&custom_blynk_token);

  // Setup WiFi Manager call backs
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setAPCallback(configModeCallback);

  // Set Wifi autoconnect timeout
  wifiManager.setConfigPortalTimeout(WIFI_TIMEOUT);   

  // Try to connect to Wifi - if not, the run captive portal
  // If still no connect, then restart and try again
  
  shouldSaveConfig = false;

  isOnline = wifiManager.autoConnect( SSID_NAME );          // Try to connect
  
  if( !isOnline ) DEBUG_PRINTLN("Failed to connect and hit timeout");
  
  // If new config loaded, then save to EEPROM
  if( shouldSaveConfig )
  {
    // Copy values from parameters
    strcpy(blynk_token, custom_blynk_token.getValue());  

    DEBUG_PRINT( "New token: -" );
    DEBUG_PRINT( blynk_token );
    DEBUG_PRINTLN( "-" );
    
    DEBUG_PRINTLN( "Writing to EEPROM" );
    EepromUtil::eeprom_write_string(0, blynk_token);
    EEPROM.commit();
  }
  else
  {
    DEBUG_PRINTLN( "Using saved token" );
  }

  // Read Blynk token from EEPROM
  EepromUtil::eeprom_read_string(0, blynk_token, 34 );

  DEBUG_PRINT( "Read token: -" );
  DEBUG_PRINT( blynk_token ); 
  DEBUG_PRINTLN( "-" );

  if( isOnline ) Blynk.config(blynk_token);         // Configure Blynk session

  ctrlLED.setDimRate( LED_DIM_NORMAL );             // Flash LED for normal operation

  if( isOnline ) updateBlnkLED.attach_ms(BLNK_FLASH_TIME, blnkLEDtick);    // Start Blynk LED flashing
  
  DEBUG_PRINTLN( "Up and running ..." );

  delay(1000);
}


// Main run loop
// -------------


void loop()
{
  if( isOnline ) Blynk.run();                       // Let Blynk do its stuff - it will also try to reconnect wifi if disconnected

  actionBtn.poll();                                 // Poll main button
  
  // Payloads

  if(actionBtn.pushed())
  {
    if( outputLED.getState() ) outputLED.dimLED(true, false);       // If pushed and on then start dimming
    else outputLED.setState(true);                                  // If pushed and off then turn on
  }

  if(actionBtn.released()) outputLED.dimLED(false, false);          // If released then stop dimming
    
  if(actionBtn.doubleClick()) outputLED.toggleState();              // If double click then toggle state
  
  if(actionBtn.longPress()) doReset();                              // If long press then restart

}

