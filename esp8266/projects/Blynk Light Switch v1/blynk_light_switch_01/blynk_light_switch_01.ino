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

Configured for ESP-12

On start up:
  1. Tries to connect to saved wifi settings if they exist - control LED flashes orange
  2. If button pressed for a more than 10sec, then reset wifi settings
  3. If no wifi or reset wifi settings go into configuration mode - Control LED very fast flashes
  4. Config mode - creates AP with SSID "BlynkSwitch", with IP 192.168.4.1
  5. Config mode - able to set and save SSID and password, and Blynk token
  6. Config mode - tries to connect to wifi
  7. Config mode times out after 90 seconds
  8. If not connected, Blynk functions are not enabled and switch is offline only - key functions still work
  9. Goes into normal running mode
  10. Control LED flashes blue if online or orange if offline

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

#include <WiFiManager.h>
#include <Ticker.h>
#include <BlynkSimpleEsp8266.h>
#include <EepromUtil.h>
#include "Switch_v2.h"
#include "PWM_LED_control.h"
#include <SimpleTimer.h>



// Define GPIO pins and UART
// -------------------------

#define OUTPUT_PIN    4               // GPIO 4 is the LED
#define INPUT_PIN     2               // GPIO 2 is button
#define BLUE_LED_PIN  0               // GPIO 0 is Blue LED
#define ORANGE_LED_PIN  5             // GPIO 5 is the orange LED

const static int SERIAL_SPEED = 115200;          // Serial port speed


// Setup Output LEDs
// -----------------

const static int LED_UPRATE_RATE = 20;

const static int LED_DIM_NORMAL = 1;
const static int LED_DIM_FAST = 5;
const static int LED_DIM_VERYFAST = 10;

pwmLED outputLED( OUTPUT_PIN, false, 100, LED_DIM_NORMAL, false, false );         // Main output LED
pwmLED blueLED( BLUE_LED_PIN, false, 100, LED_DIM_NORMAL, false, true );          // Blue LED
pwmLED orangeLED( ORANGE_LED_PIN, false, 100, LED_DIM_NORMAL, false, true );      // Orange LED

Ticker updateLEDs;                 // LED update timer

void updateLEDtick()
{
  // Move output LED to next dim level
  
  outputLED.autoDim();
  blueLED.autoDim();
  orangeLED.autoDim();
}


// Wifi Settings
// -------------

const static int WIFI_TIMEOUT = 90;               // 90 seconds
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

  orangeLED.setDimRate(LED_DIM_FAST);
}


// Reset function
// --------------

void doReset( bool hard = false )
{
  #ifdef DEBUG
    Serial.begin(SERIAL_SPEED);       // Turn on serial
  #endif
  
  DEBUG_PRINTLN( "Starting reset" );

  orangeLED.setDimRate(LED_DIM_VERYFAST);
  blueLED.setState(false);
  orangeLED.setState(true);
  
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
#define BLYK_CTRL_LED   0             // Virtual pin showing normal operation
#define BLYK_MAIN_LED   1             // Virtual pin used for showing main control input
#define BLNK_MAIN_BTN   2             // Virtual pin to match main button
#define BLNK_DIMMER     3             // Virtual pin for dimmer slider
#define BLNK_GAUGE      4             // Virtual pin for return level
#define BLNK_RESET      30            // Virtual pin to trigger a reset
#define BLNK_HARDRESET  31            // Virtual pin to trigger a hard reset (clearing wifi settings)

const static int BLNK_FLASH_TIME = 2000;                       // Period of Blynk flash
const static char BLNK_PARAM_PROMPT[] = "Enter Blynk token";
const static char BLNK_PARAM_ID[] = "blnk_token";

// Functions called on Blynk actions

// Initiate reset

BLYNK_WRITE(BLNK_RESET)
{
  DEBUG_PRINTLN( "Blynk reset" );

  doReset();                    // Soft reset
}

// Initiate Hard Reset 

BLYNK_WRITE(BLNK_HARDRESET)
{
  DEBUG_PRINTLN( "Blynk hard reset" );

  doReset(true);                // Hard reset (clear settings)
}

// Blynk button pressed

BLYNK_WRITE(BLNK_MAIN_BTN)
{
  if( param.asInt() != 0 ) outputLED.toggleState();                   // Toggle LED state
  Blynk.virtualWrite(BLYK_MAIN_LED, outputLED.getState()*255);        // Update Blynk LED
}

// Dimmer changed

BLYNK_WRITE(BLNK_DIMMER)
{
  outputLED.setLevel( param.asInt() );                        // Virtual pin set 0-100
  Blynk.virtualWrite(BLNK_GAUGE, outputLED.getLevel());       // update Blynk gauge
}

// Flash Blynk control LED

SimpleTimer blnkFlashTimer;
bool blnkFlash = false;

void flashBlnkLED()
{
  Blynk.virtualWrite(BLYK_CTRL_LED, blnkFlash*255);
  blnkFlash = !blnkFlash;
}


// Switch functions
// ----------------

const static int LONG_PRESS = 10000;       // Need to press for 20s to initiate long press
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

  // Setup LEDs

  analogWriteFreq( 100 );                                 // Slow down the PWM duty cycle to give MOSFET time to respond
  updateLEDs.attach_ms(LED_UPRATE_RATE, updateLEDtick);   // start LED update timer

  orangeLED.setState(true);
  orangeLED.dimLED(true);
  blueLED.setState(false);
  blueLED.dimLED(true);

  // Turn on serial
  
  #ifdef DEBUG
    Serial.begin(SERIAL_SPEED);
    DEBUG_PRINTLN( "" );
    DEBUG_PRINTLN( "" );
    DEBUG_PRINTLN("--------------");
    DEBUG_PRINTLN(" Blynk Switch ");
    DEBUG_PRINTLN("--------------");
    DEBUG_PRINTLN( "" );
    DEBUG_PRINTLN("Debug ON");
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

    if(actionBtn.longPress())           // If long press then reset setttings
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

  wifiManager.setTimeout(WIFI_TIMEOUT);                       // Set Wifi autoconnect timeout
  
  shouldSaveConfig = false;

  isOnline = wifiManager.autoConnect( SSID_NAME );            // Try to connect to Wifi - if not, the run captive portal
  
  if( !isOnline ) DEBUG_PRINTLN("Failed to connect and hit timeout");
  
  if( shouldSaveConfig )                                     // If new config loaded, then save to EEPROM
  {
    strcpy(blynk_token, custom_blynk_token.getValue());      // Copy values from parameters

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

  EepromUtil::eeprom_read_string(0, blynk_token, 34 );      // Read Blynk token from EEPROM

  DEBUG_PRINT( "Read token: -" );
  DEBUG_PRINT( blynk_token ); 
  DEBUG_PRINTLN( "-" );

  if( isOnline ) Blynk.config(blynk_token);                 // Configure Blynk session

  if( isOnline ) blnkFlashTimer.setInterval(BLNK_FLASH_TIME, flashBlnkLED);   // Start Blynk LED flashing

  orangeLED.setDimRate(LED_DIM_NORMAL);                     // Normal mode

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

  blueLED.setState((!actionBtn.on())^(!isOnline));  // If online then blue flashing and orange when pressed
  orangeLED.setState((actionBtn.on())^(!isOnline)); // If offline then vise versa
  
  if( actionBtn.on() )                               // Do flashing  when pressed
  {
    blueLED.setLevel(100);
    orangeLED.setLevel(100);
  }

  if( actionBtn.doubleClick() )                     // Toggle on/offf
  {
    if( outputLED.getLevel() == 0 )                 // If off then set dim to full on
    {
      outputLED.setLevel(100);
      outputLED.setDimDirection(true);
    }
    outputLED.toggleState();                                            // If double click then toggle state
  }
  else if( actionBtn.released() ) outputLED.toggleDimDirection();       // Change dim direction on release
  else if( outputLED.getState() ) outputLED.dimLED(actionBtn.on());     // Dim if on and pushed
  else if( actionBtn.singleClick() )                                    // Click to dim
  {
      outputLED.setLevel(0);
      outputLED.setDimDirection(true);
      outputLED.setState(true);
  }
   
  if(actionBtn.longPress()) doReset();                                  // If long press then restart
}

