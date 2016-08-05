#define DEBUG
#include <DebugUtils.h>

extern "C" {
#include "user_interface.h"
}

#include <BlynkSimpleEsp8266.h>
#include <WiFiManager.h>
#include <EepromUtil.h>
#include <Switch.h>
#include <Ticker.h>
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


// Setup Output LED
// ----------------

const static int FLASHCONFIG = 75;            // Flash LED very fast
const static int FLASHNORMAL = 100;            // Flash LED slow
const static int FLASHSTARTING = 25;           // Flash LED fast
const static int DIMRATE = 5;

pwmLED mainOutputLED( OUTPUT_PIN, false, 100 );       // Main output LED
pwmLED ctrlLED( CTRL_LED, false, 100 );               // Control LED

// For control LED

Ticker updateCtrlLED;

void ctrlLEDtick()
{  
  ctrlLED.autoDim(DIMRATE);             // Move LED to next dim level

  DEBUG_PRINTLN("-");
}


// Wifi Settings
// -------------

const static int WIFI_TIMEOUT = 180;              // Three minutes
const static int EEPROM_MAX = 512;                // Max spaced used in EEPROM
const static char SSID_NAME[] = "Blynk Switch";

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
  DEBUG_PRINTLN(myWiFiManager->getConfigPortalSSID());      //if you used auto generated SSID, print it
  
  // Entered config mode, make led toggle faster
  updateCtrlLED.attach_ms(FLASHCONFIG, ctrlLEDtick);
}


// Reset function
// --------------

void doReset( bool hard = false )
{
  #ifdef DEBUG
    Serial.begin(SERIAL_SPEED);       // Turn on serial
  #endif
  
  updateCtrlLED.attach_ms(FLASHCONFIG, ctrlLEDtick);     // Make led toggle faster 
  
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
  if( param.asInt() != 0 ) mainOutputLED.toggleState();       // Toggle LED state
}

// Dimmer changed

BLYNK_WRITE(BLNK_DIMMER)
{
  mainOutputLED.setLevel( param.asInt() );         // Virtual pin set 0-100
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

Switch actionBtn(INPUT_PIN, INPUT, LOW, DEBOUNCE, LONG_PRESS );


// Main Setup
// ----------

bool isOnline = false;          // Did we initially get connecteed

void setup()
{  
  // Setup EEPROM
  EEPROM.begin(EEPROM_MAX);

  // start flashing as we start in AP mode and try to connect
  updateCtrlLED.attach_ms(FLASHSTARTING, ctrlLEDtick);

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

  if( isOnline ) Blynk.config(blynk_token);          // Configure Blynk session

  // Flash LED for normal operation
  
  updateCtrlLED.attach_ms(FLASHNORMAL, ctrlLEDtick);

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

  if(actionBtn.longPress()) doReset();                    // If long press then restart
  if(actionBtn.pushed()) mainOutputLED.toggleState();     // LED on or off
}

