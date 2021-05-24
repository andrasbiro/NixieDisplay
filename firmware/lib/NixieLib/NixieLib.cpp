#include "NixieLib.h"
#include <Arduino.h>
#include <SPI.h>

NixieLib::NixieLib(uint8_t numberOfModules, nixieType_t moduleType, uint8_t latchPin, uint8_t dimPin, uint16_t startBrightness){
  numOfModules = numberOfModules;
  latchGpio = latchPin;
  dimGpio = dimPin;
  moduleType = moduleType;
  pinMode(latchGpio, OUTPUT);
  pinMode(dimGpio, OUTPUT);
  SPI.begin();
  analogWrite(dimGpio, startBrightness);
  digitalWrite(latchGpio, LOW);
  dirty = true;
}

//HV5122 based functions
uint32_t NixieLib::nixieTranslatorHv5122(nixieValue_t niceValue){
  // first 16bits:  digit0, dot0, dotLower
  // second 16bits: digit1, dot1, dotUpper
  if( niceValue.digits > 99 )
    return 0;

  uint32_t returnVal = 0;
  if ( niceValue.blankingMode != BLANKBOTHDIGITS ){
    //digit0, lower 10 bit
    if ( niceValue.blankingMode != BLANKLOWERDIGIT )
      returnVal |= 1 << (niceValue.digits%10);
    //digit1, 10bit above 16
    if ( niceValue.blankingMode != BLANKUPPERDIGIT )
      returnVal |= 1 << (niceValue.digits/10 + 16);
  }

  //just delete zero bits, depending on showzero setup
  if ( niceValue.blankingMode != SHOWALLZEROS ){
    returnVal &= ~0x1;
    if ( niceValue.blankingMode == SHOWNOZEROS ){
      returnVal &= ~(1<<16);
    }
  }

  if ( niceValue.dot0 ){
    returnVal |= 1<<10;
  }
  if ( niceValue.dotlower ){
    returnVal |= 1<<11;
  }
  if ( niceValue.dot1 ){
    returnVal |= 1<<(10+16);
  }
  if ( niceValue.dotupper ){
    returnVal |= 1<<(11+16);
  }
  return returnVal;
}

uint8_t NixieLib::getModuleBytesHv5122(){
  return 4;
}

//TPIC6592 specific functions
uint32_t NixieLib::nixieTranslatorTpic6592(nixieValue_t niceValue){
  // dot0, digit0, dot1, digit1, dotLower, dotUpper
  
  if( niceValue.digits > 99 )
    return 0;

  uint32_t returnVal = 0;
  if ( niceValue.blankingMode != BLANKBOTHDIGITS ){
    if ( niceValue.blankingMode != BLANKUPPERDIGIT )
      returnVal |= 1 << (niceValue.digits/10 + 1);
    if ( niceValue.blankingMode != BLANKLOWERDIGIT )
      returnVal |= 1 << (niceValue.digits%10 + 12);
  }

  //just delete zero bits, depending on showzero setup
  if ( niceValue.blankingMode != SHOWALLZEROS ){
    returnVal &= ~(1<<1);
    if ( niceValue.blankingMode == SHOWNOZEROS && niceValue.digits == 0){
      returnVal &= ~(1<<12);
    }
  }

  if ( niceValue.dot0 ){
    returnVal |= 1<<0;
  }
  if ( niceValue.dot1 ){
    returnVal |= 1<<11;
  }
  if ( niceValue.dotlower ){
    returnVal |= 1<<22;
  }
  if ( niceValue.dotupper ){
    returnVal |= 1<<23;
  }
  return returnVal;
}

uint8_t NixieLib::getModuleBytesTpic6592(){
  return 3;
}

uint32_t NixieLib::nixieTranslator(nixieValue_t niceValue){
  if ( moduleType == NIXIE_MODULE_TYPE_HV5122 )
    return nixieTranslatorHv5122(niceValue);
  else
    return nixieTranslatorTpic6592(niceValue);
}

uint8_t NixieLib::getModuleBytes(){
  if ( moduleType == NIXIE_MODULE_TYPE_HV5122 )
    return getModuleBytesHv5122();
  else
    return getModuleBytesTpic6592();
}

void NixieLib::writeNixies(uint32_t* data){
  digitalWrite(latchGpio, LOW);
  SPI.beginTransaction(SPISettings(NIXIE_SPI_CLOCK, MSBFIRST, SPI_MODE0));
  for(int i=0; i<numOfModules;i++){
    for(int j=(getModuleBytes()-1)*8; j>=0; j-=8){
      SPI.write(data[i] >> j);
    }
  }
  SPI.endTransaction();
  digitalWrite(latchGpio, HIGH);
}


void NixieLib::setNixieModule(uint8_t index, uint8_t digits, blankingMode_t blankingMode, bool dot0, bool dot1, bool dotupper, bool dotlower){
  if ( index > numOfModules || digits > 99  )
    return;

  if ( dirty ){
    //if we're dirty, write directly to the right place: it's faster to overwrite with the same thing
    modules[index].digits = digits;
    modules[index].blankingMode = blankingMode;
    modules[index].dot0 = dot0;
    modules[index].dot1 = dot1;
    modules[index].dotupper = dotupper;
    modules[index].dotlower = dotlower;

  } else {
    nixieValue_t newValue = {
      .digits = digits,
      .blankingMode = blankingMode,
      .dot0 = dot0,
      .dot1 =  dot1,
      .dotupper = dotupper,
      .dotlower = dotlower,
    };

    if ( memcmp(&modules[index], &newValue, sizeof(nixieValue_t)) != 0 ){
      modules[index] = newValue;
      dirty = true;
    }
  }
}

typedef enum {
  RIGHTDIGIT = 0,
  RIGHTDOT = 1,
  LEFTDIGIT = 2,
  LEFTDOT = 3,
  COLON = 4,
  CHANGEMODULE = 5,
} stringReadState_t;

void NixieLib::setNixies(String newValue){
  int currentModule = 0;
  int stringReadState = RIGHTDIGIT;
  uint8_t digits = 0;
  bool dot0 = false;
  bool dot1 = false;
  bool dotupper = false;
  bool dotlower = false;
  blankingMode_t blank = SHOWALLZEROS;
  int charIndex = newValue.length()-1;
  bool noDataOrRoomLeft = false;
  while ( !noDataOrRoomLeft ){
    char current = newValue.charAt(charIndex);
    if ( (stringReadState == RIGHTDOT || stringReadState == LEFTDOT) && current != '.'){
      //omitting dots are allowed, so in this case, we advance to the next required char
      stringReadState++;
    }
    if ( current == '.' ){
      if (stringReadState == RIGHTDOT){
        dot1 = true;
      } else if ( stringReadState == LEFTDOT ) {
        dot0 = true;
      } else { //colon
        dotlower = true;
      }
    } else if ( stringReadState == COLON && current == ':'){
      dotupper = true;
      dotlower = true;
    } else if ( stringReadState == COLON && current == '^'){
      dotupper = true;
    } else if ( stringReadState != COLON && current > '0' && current < '9'){
      if(stringReadState == RIGHTDIGIT){
        digits = current-48;
      } else {
        digits += 10*(current-48);
      }
    } else { //blanking, expected code is space, but any unknown char is handled as blanking
      if(stringReadState == RIGHTDIGIT){
        blank = BLANKLOWERDIGIT;
      } else if (stringReadState == LEFTDIGIT){
        if ( blank == SHOWALLZEROS ){
          blank = BLANKUPPERDIGIT;
        } else {
          blank = BLANKBOTHDIGITS;
        }
      } //colon: do nothing, default state is off
    }

    stringReadState ++;
    if ( stringReadState == CHANGEMODULE ){
      setNixieModule(currentModule, digits, blank, dot0, dot1, dotupper, dotlower);
      currentModule++;
      stringReadState = RIGHTDIGIT;
      dot0 = false;
      dot1 = false;
      dotupper = false;
      dotlower = false;    
      blank = SHOWALLZEROS;
      digits = 0;
    }

    charIndex--;
    if ( charIndex < 0 || currentModule >= numOfModules ){
      noDataOrRoomLeft = true;
    }
  }
}

void NixieLib::setNixies(float newValue, uint8_t decimals, bool forceDecimals){
  int intPart = trunc(newValue);
  int decimal = pow(10, decimals) * (newValue - intPart);
  int remainingDecimals = decimals;
  bool intDigitsPrinted = false;
  if (!forceDecimals){
    while ( decimal % 10 == 0 ){
      remainingDecimals--;
      decimal /= 10;
    }
  }
  for(int i=0; i<numOfModules; i++){
    if ( remainingDecimals > 0 ){
      uint8_t digits = decimal%100;
      decimal /= 100;
      if ( remainingDecimals == 1 ){
        digits+= 10 * (intPart%10);
        intPart /=10;
        setNixieModule(i, digits, SHOWALLZEROS, false, true, false, false); 
        intDigitsPrinted = true;
      } else {
        setNixieModule(i, digits, SHOWALLZEROS, remainingDecimals==2?true:false, false, false, false); 
      }
    } else {
      uint8_t digits = intPart%100;
      intPart /= 100;
      if ( intPart > 0 ){
        setNixieModule(i, digits, SHOWALLZEROS, false, false, false, false);
      } else if ( !intDigitsPrinted ){
        setNixieModule(i, digits, SHOWLONEZERO, false, false, false, false);
      } else{
        setNixieModule(i, digits, SHOWNOZEROS, false, false, false, false);
      }
      intDigitsPrinted = true;
    }
    remainingDecimals-=2;
  }
}


void NixieLib::setNixies(uint32_t newValue){
  uint32_t valueLeft = newValue;
  bool digitsPrinted = false;
  for(int i=0; i<numOfModules; i++){
    uint8_t digits = valueLeft%100;
    valueLeft /= 100;
    if ( valueLeft > 0 ){
      setNixieModule(i, digits, SHOWALLZEROS, false, false, false, false);
    } else if ( !digitsPrinted ){
      setNixieModule(i, digits, SHOWLONEZERO, false, false, false, false);
    } else {
      setNixieModule(i, digits, SHOWNOZEROS, false, false, false, false);
    }
    digitsPrinted = true;
  }
}


void NixieLib::updateNixies(){
  if ( dirty ){
    uint32_t nixieValues[numOfModules];
    for(int i=0; i<numOfModules;i++){
      nixieValues[i] = nixieTranslator(modules[i]);
    }
    writeNixies(nixieValues);
    dirty = false;
  }
}

void NixieLib::setBrightness(uint16_t brightness){
  analogWrite(dimGpio, brightness);
}
