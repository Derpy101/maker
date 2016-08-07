#include <DebugUtils.h>

#ifndef PWM_LED_CONTROL_H
#define PWM_LED_CONTROL_H

#if defined(ARDUINO) && ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

#include <math.h>


class pwmLED {

public:

  // Constructor
  pwmLED( int outputPin, bool startState, int startLevel, int dimRate );

  // Get the current state
  bool getState();

  // Set state
  void setState(bool newState);

  // Toggle state
  void toggleState();

  // Get the current level
  int getLevel();

  // Set level
  void setLevel(int newLevel);

  // Step to next auto dim level
  void autoDim();

  // Set dim rate
  void setDimRate(int dimRate);

  // Set dim direction
  void setDimDirection(int dimUp);
  
  // Set dim mode
  void dimLED(bool startDimming, bool isCyclick);
  
private:

  // Constants
  constexpr const static int _PWM_MAX = 1023;                       // PWM out range is 0 to _PWM_MAX
  constexpr const static int _PWM_LED_LEVEL_IN_MAX = 100;           // Input range is 0 to LEVEL_IN_MAX

  // The output pin to control
  int _outputPin;

  // The current on/off status of the output
  bool _outputState = false;

  // The current dimmer level set for the output (0-LEVEL_IN_MAX)
  int _outputLevel = 0;

  // The change in dim rate, mode and dim direction
  int _dimRate = 1;
  bool _dimUp = true;
  bool _dimLED = false;
  bool _isCyclick = false;

  // Update the pin PWM
  void setPinPWM( int newLevel );
};


#endif
