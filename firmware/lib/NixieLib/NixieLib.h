#ifndef __NIXIELIB_H__
#define __NIXIELIB_H__

#include <stdint.h>
#include "Arduino.h"

/*
@brief  Maximum number of modules supported. The class will allocate memory
		for this number of modules.
*/
#ifndef NIXIE_MAX_NUM_OF_MODULES
#define NIXIE_MAX_NUM_OF_MODULES 5
#endif

/*!
@brief  SPI clockrate to use. Recommended to set it very low, as you probably
		use long cables, and nixies are slow anyway
*/
#ifndef NIXIE_SPI_CLOCK
#define NIXIE_SPI_CLOCK 10e3
#endif

/*!
@brief  Blanking mode configuration used by @ref setNixieModule()
*/
typedef enum{
	SHOWALLZEROS,		///< Show all numbers
	SHOWLONEZERO,		///< Turn off leading zeros, but show a lone 0 if number is 0
	SHOWNOZEROS,		///< Turn off leading zeros and display nothing if number is 0
	BLANKUPPERDIGIT,	///< Turn off left digit (whatever the number is)
	BLANKLOWERDIGIT,	///< Turn off right digit (whatever the number is)
	BLANKBOTHDIGITS		///< Turn off both digits (whatever the number is)
} blankingMode_t;

/*!
@brief  The modules to use. Note that only TPIC6592 is fully tested
*/
typedef enum {
	NIXIE_MODULE_TYPE_HV5122 = 1, //there is a bug somewhere which requires this not to be zero, but I can't find it
	NIXIE_MODULE_TYPE_TPIC6592,
} nixieType_t;

//private type
typedef struct {
  uint8_t digits; //two digits 0-99
  blankingMode_t blankingMode:8;
  bool dot0;
  bool dot1;
  bool dotupper;
  bool dotlower;
} nixieValue_t;

class NixieLib {
private:
	uint8_t numOfModules;
	nixieType_t moduleType;
	uint8_t latchGpio;
	uint8_t dimGpio;
	nixieValue_t modules[NIXIE_MAX_NUM_OF_MODULES];
	bool dirty;

	void writeNixies(uint32_t* data);
	uint32_t nixieTranslator(nixieValue_t niceValue);
	uint8_t getModuleBytes();
// module specific functions
	uint32_t nixieTranslatorHv5122(nixieValue_t niceValue);
	uint8_t getModuleBytesHv5122();
	uint32_t nixieTranslatorTpic6592(nixieValue_t niceValue);
	uint8_t getModuleBytesTpic6592();

public:

	/*!
	@brief  NixieLib constructor
	@param  numberOfModules  Number of modules in the chain
	@param  moduleType Type of modules in the chain (mixing of modules is
					   not allowed)
	@param  latchPin GPIO to use for latching the data to the nixies. Pin
					 will go down before write and up after it.
	@param  dimPin GPIO used for dimming via PWM/analogWrite, Must be PWM
				   capable!
	@param  startBrightness brightness to use at init. 0 is off and
							PWMRANGE is fully on

	@note   default SPI peripheral will be used to write into the shift
			registers

	@return  NixieLib object.
	*/
	NixieLib(uint8_t numberOfModules, nixieType_t moduleType, uint8_t latchPin, uint8_t dimPin, uint16_t startBrightness = 0);

	/*!
	@brief  Sets a module to a given state in memory. Writing the modules
		    happens when @ref updateNixies() is called
	@param  index Index of the module to write. 0 is the last in the chain
	@param	digits Number on the two nixis on the module. Must be between
				   0-99
	@param	blanking Blanking mode of the nixies, including leading zero
					 config
	@param  dot0 decimal point of the left nixie (note that not all
				 nixies have this)
	@param  dot1 decimal point of the right nixie (note that not all
				 nixies have this)
	@param  dotupper upper point of the colon (if populated)
	@param  dotlower lower point of the colon (if populated)
	*/
	void setNixieModule(uint8_t index, uint8_t digits, blankingMode_t blanking, bool dot0, bool dot1, bool dotupper, bool dotlower);
	
	/*!
	@brief  Sets all modules to a given state in memory via coded sting. Writing
			the modules happens when @ref updateNixies() is called
	@param  newValue A string which codes the new value. Every module is coded
					 in 3-5 characters, e.g. ":01". 
					 The first character is the colon which must be either
					 ':', '.', '^' or ' ', to show both, only the lower. only
					 the upper dot or neither, respectively.
					 The next characters are the first number, it can be
					 eighter a number or a space to turn it off.
					 A decimal point can optionally preceed a number, e.g.
					 ":.0.1" is a valid string.
					 Display is populated from right to left backwards, only
					 full modules are supported (i.e. don't forget the leading
					 colon)
	*/
	void setNixies(String newValue);
	
	/*!
	@brief  Sets all modules to a given state in memory via floating point
			number. Writing the modules happens when @ref updateNixies() is
			called. Leading zeros are omitted always, number will be right
			justified. Colons are off and decimal points in the nixie tubes
			are used.
	@param  newValue The value to display
	@param  decimals Number of decimal places to show
	@param  forceDecimals Always show @ref decimals number of decimal places.
						  If false, rightmost decimals will be ommitted when
						  zero.
	*/
	void setNixies(float newValue, uint8_t decimals, bool forceDecimals);
	
	/*!
	@brief  Sets all modules to a given state in memory via integer number.
			Writing the modules happens when @ref updateNixies() is called.
			Leading zeros are omitted always, number will be right justified.
			Colons and decimal points are off.
	@param  newValue The value to display
	*/
	void setNixies(uint32_t newValue);

	/*!
	@brief  Writes the previously set data to the modules. Data can be
			configured by either @ref setNixieModule() or one of the
			@ref setNixies() functions.
	*/
	void updateNixies();

	/*!
	@brief	Sets the nixie brighness via PWM/analogWrite. 0 means off and
			PWM_RANGE means fully on.
	*/
	void setBrightness(uint16_t brightness);

};

#endif
