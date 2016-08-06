#define DEBUG
#include "PWM_LED_control.h"


// Constructor
pwmLED::pwmLED( int outputPin, bool startState, int startLevel, int dimRate, int dimMode )
{
  _outputPin = outputPin;         // Set output pin for this instance
  _outputLevel = startLevel;      // Set starting dim level
  _outputState = startState;      // Set starting state
  _dimRate = dimRate;             // Set starting dim rate
  _dimMode = dimMode;             // Set starting dim mode

  pinMode(_outputPin, OUTPUT);
}


// Get the current state
bool pwmLED::getState()
{
  return _outputState;
}


// Set state
void pwmLED::setState(bool newState)
{
  if( newState != _outputState )          /// Do nothing if no change in state
  {
    _outputState = newState;

    if( _outputState ) this->setPinPWM( _outputLevel );     // Set on at stored level
    else this->setPinPWM( 0 );                              // Turn off 
  }
}


// Toggle state
void pwmLED::toggleState()
{
  bool newState = !_outputState;
  
  this->setState( newState );             // Toggle it
}


// Get the current level
int pwmLED::getLevel()
{
  return _outputLevel;
}


// Set level
void pwmLED::setLevel(int newLevel)
{
  if( newLevel != _outputLevel )              // Do nothing if no change to level
  {
    _outputLevel = newLevel;
    
    if( _outputState ) this->setPinPWM( _outputLevel );   // If on, then set it.
  }
}


// Update the pin PWM
void pwmLED::setPinPWM( int newLevel )
{
  int newOutputPWM = (int)(pow( (float)newLevel/PWM_LED_LEVEL_IN_MAX, 1/0.5 ) * PWM_MAX);       // Linearisation
    
  analogWrite( _outputPin, newOutputPWM );   // Set output

  DEBUG_PRINT("Pin ");
  DEBUG_PRINT( _outputPin );
  DEBUG_PRINT(" : Level set to ");
  DEBUG_PRINT( newLevel );
  DEBUG_PRINT(", PWM set to ");
  DEBUG_PRINTLN( newOutputPWM );
}


// Step to next auto dim level - typically called by a timer
void pwmLED::autoDim()
{
  if( _dimMode == PWM_LED_MODE_BINARY ) return;
  
  // Go up or down
  
  if( _dimUp ) _outputLevel += _dimRate;
  else _outputLevel -= _dimRate;

  // No over/under run
  if( _outputLevel >= PWM_LED_LEVEL_IN_MAX ) _outputLevel = PWM_LED_LEVEL_IN_MAX;
  if( _outputLevel <= 0 ) _outputLevel = 0; 

  if( _dimMode == PWM_LED_MODE_CYCLICK )         // Change direction
  {
    if( _outputLevel == PWM_LED_LEVEL_IN_MAX ) _dimUp = false;
    
    if( _outputLevel == 0 ) _dimUp = true;
  }

  if( _outputState ) this->setPinPWM( _outputLevel );   // If on, then set it.
}

// Set dim rate
void pwmLED::setDimRate(int dimRate)
{
  _dimRate = dimRate;
}

