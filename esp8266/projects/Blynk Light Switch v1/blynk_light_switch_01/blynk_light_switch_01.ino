//#define DEBUG
#include <DebugUtils.h>

extern "C" {
#include "user_interface.h"
}

#include <BlynkSimpleEsp8266.h>
#include <WiFiManager.h>
#include <EepromUtil.h>
#include <Switch.h>
#include <Ticker.h>
#include <SimpleTimer.h>

#ifdef DEBUG
  #define BLYNK_PRINT Serial      // Comment this out to disable prints and save space
//  #define BLYNK_DEBUG             // Optional, this enables lots of prints
#endif

//s
// Wifi Settings
#define WIFI_TIMEOUT  180         // Three minutes

// Set up EEPROM usage
// Pairs of variable and address
#define EEPROM_MAX  512           // Max spaced used in EEPROM
char blynk_token[34];             // Blynk tokcen
int add_blynk_token = 0;          // Address of token in EEPROM

// Define GPIO pins and UART
#define INPUT_PIN     2               // GPIO 2 is the main input pin
#define CTRL_BTN      3               // GPIO 3 is the control button
#define CTRL_LED      1               // GPIO 1 is the control LED
#define OUTPUT_PIN    0               // GPIO 0 is the output pin
#define SERIAL_SPEED  115200          // Serial port speed

// Define Blynk environment
#define BLYK_MAIN_LED   1             // Virtual pin used for showing main control input
#define BLYK_CTRL_LED   0             // Virtual pin showing normal operation
#define BLNK_MAIN_BTN   2             // Virtual pin to match main button
#define BLNK_RESET      30            // Virtual pin to trigger a reset
#define BLNK_HARDRESET  31            // Virtual pin to trigger a hard reset (clearing wifi settings)
#define BLNK_FLASH_TIME 1500L         // Period of Blynk flash

const char blnkParamPrompt[] = "Enter Blynk token";
const char blnkParamID[] = "blnk_token";


// Main payload
// ------------

// Button press or Blynk button press toggles main output

bool outputState = false;       // State of output

void toggleOutput(){
  DEBUG_PRINTLN("Toggling output");
  outputState = !outputState;
  digitalWrite(OUTPUT_PIN, outputState);
  Blynk.virtualWrite(BLYK_MAIN_LED, outputState*255);
}


// For LED status
// --------------

#define FLASHCONFIG     0.15          // Flash LED very fast
#define FLASHNORMAL     2.0           // Flash LED slow
#define FLASHSTARTING   0.5           // Flash LED fast

Ticker flashLED;

bool nowrunning = false;
void flashLEDtick()
{
  // Toggle state of LED
  
  int state = digitalRead(CTRL_LED);  // get the current state of pin
  digitalWrite(CTRL_LED, !state);     // set pin to the opposite state
}

// WiFiManager Callback Functions
// ------------------------------

// Setup WifiManager
WiFiManager wifiManager;  

bool shouldSaveConfig;                // has the config changed after running the captive portal

// Called when the configu gets updated from the portal

void saveConfigCallback () {

  DEBUG_PRINTLN("Should save config");
  shouldSaveConfig = true;                // Need to save the new config to EEPROM
}

// Gets called when WiFiManager enters configuration mode

void configModeCallback (WiFiManager *myWiFiManager) {

  // Debug it
  DEBUG_PRINTLN("Entered config mode");
  DEBUG_PRINTLN(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  DEBUG_PRINTLN(myWiFiManager->getConfigPortalSSID());
  
  // Entered config mode, make led toggle faster
  flashLED.attach(FLASHCONFIG, flashLEDtick);
}

// Switch functions
// ----------------

#define LONG_PRESS  20000     // Need to press for 20s to initiate long press
#define DEBOUNCE    50        // 50ms for switch debounce
#define START_TIME  10000     // 10 secs at start up to go into config mode

Switch actionBtn(INPUT_PIN, INPUT, LOW, DEBOUNCE, LONG_PRESS );


// Reset function
// --------------

void doReset( bool hard = false ) {
  // Turn on serial
  
  #ifdef DEBUG
    Serial.begin(SERIAL_SPEED);
  #else
    flashLED.attach(FLASHCONFIG, flashLEDtick);
  #endif

  DEBUG_PRINTLN( "Starting reset" );
  
  delay(1000);

  if( hard ) {
   
    // Reset the Wifi settings

    DEBUG_PRINTLN( "Clearing settings" );

    wifiManager.resetSettings();
  
    delay(1000);
  }
  
  DEBUG_PRINTLN( "Reseting" );
  
  delay(5000);

  ESP.reset();      // This only works correctly if there has been a normal restart after a flash.   
}


// Functions called on Blynk actions
// ----------------------------------

// Initiate reset

BLYNK_WRITE(BLNK_RESET)
{
  doReset();
}


// Initiate Hard Reset 

BLYNK_WRITE(BLNK_HARDRESET)
{
  doReset(true);
}


// Blynk button pressed

BLYNK_WRITE(BLNK_MAIN_BTN)
{
  if( param.asInt() != 0 ) toggleOutput();
}


// Flash Blynk control LED

SimpleTimer blnkFlashTimer;
bool blnkFlash = false;

void flashBlnkLED()
{
  Blynk.virtualWrite(BLYK_CTRL_LED, blnkFlash*255);
  blnkFlash = !blnkFlash;
}

// Main Setup
// ----------


void setup()
{
  // Set up GPIO
  
  pinMode(INPUT_PIN, INPUT);
  pinMode(OUTPUT_PIN, OUTPUT);

  // Setup EEPROM
  EEPROM.begin(EEPROM_MAX);

  // start flashing as we start in AP mode and try to connect
  flashLED.attach(FLASHSTARTING, flashLEDtick);

  // Turn on serial
  #ifdef DEBUG
    Serial.begin(SERIAL_SPEED);
    DEBUG_PRINTLN( "" );
    DEBUG_PRINTVB("Debug ON");
  #else
    wifiManager.setDebugOutput(false);
    pinMode(CTRL_BTN, INPUT);
    pinMode(CTRL_LED, OUTPUT);
  #endif

  DEBUG_PRINTLN("Waiting for config mode request");

  unsigned const long interval = START_TIME; // the time we need to wait
  unsigned long startMillis = millis();  
  while ((unsigned long)(millis() - startMillis) <= interval) {
    yield();
    actionBtn.poll();  

    // If button pressed, then extend time
    
    if(actionBtn.on()) startMillis = millis();

    // If long press then reset setttings

    if(actionBtn.longPress()) {
   
      // Reset the Wifi settings
      DEBUG_PRINTLN( "Clearing settings" );
      wifiManager.resetSettings();

      break;     // Its a log press so keep going
    }
  }
  DEBUG_PRINTLN("Continue");
  
  // Add Captive Portal parameter for Blynk token
  WiFiManagerParameter custom_blynk_token(blnkParamID, blnkParamPrompt, blynk_token, 34);
  wifiManager.addParameter(&custom_blynk_token);

  // Setup WiFi Manager call backs
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setAPCallback(configModeCallback);

  // Set Wifi autoconnect timeout
  wifiManager.setTimeout(WIFI_TIMEOUT);   

  // Try to connect to Wifi - if not, the run captive portal
  // If still no connect, then restart and try again
  
  shouldSaveConfig = false;

  if(!wifiManager.autoConnect("BlynkIFTTT")) {
    DEBUG_PRINTLN("Failed to connect and hit timeout");
    
    doReset();
  } 
  
  // If new config loaded, then save to EEPROM
  if( shouldSaveConfig ) {

    // Copy values from parameters
    strcpy(blynk_token, custom_blynk_token.getValue());  

    DEBUG_PRINT( "New token: -" );
    DEBUG_PRINT( blynk_token );
    DEBUG_PRINTLN( "-" );
    
    DEBUG_PRINTLN( "Writing to EEPROM" );
    EepromUtil::eeprom_write_string(0, blynk_token);
    EEPROM.commit();
  }
  else {
    DEBUG_PRINTLN( "Using saved token" );
  }

  // Read Blynk tocken from EEPROM
  EepromUtil::eeprom_read_string(0, blynk_token, 34 );

  DEBUG_PRINT( "Read token: -" );
  DEBUG_PRINT( blynk_token ); 
  DEBUG_PRINTLN( "-" );

  // Configure Blynk session
  Blynk.config(blynk_token);

  // Flash LED for normal operation
  flashLED.attach(FLASHNORMAL, flashLEDtick);

  // Start Blynk LED flashing
  blnkFlashTimer.setInterval(BLNK_FLASH_TIME, flashBlnkLED);
  
  DEBUG_PRINTLN( "Up and running ..." );

  delay(1000);

  // Turn off serial
  #ifdef DEBUG
//    Serial.end();
//    pinMode(CTRL_BTN, INPUT);
//    pinMode(CTRL_LED, OUTPUT);
  #endif
}


// Main run loop
// -------------

void loop()
{
  Blynk.run();                       // Let Blynk do its stuff
  blnkFlashTimer.run();              // Initiates SimpleTimer

  // Poll input switches
  actionBtn.poll();                           // Poll main button
  if(actionBtn.longPress()) doReset(true);    // If long press then reset settings

  // Main payload
  
  if(actionBtn.pushed()) {
    toggleOutput();
  }
}

