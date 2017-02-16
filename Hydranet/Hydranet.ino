#include <Bridge.h>
#include <Process.h>

//
// Code for Arduino Uno to use OneWire temperature 
// sensors to display on a DFRobot LCD
//
// Written for a LinkIT Smart 7688 Duo
//
// Author: Jennifer Liddle <jennifer@jsquared.co.uk>
//
// Based on example code :
//
// http://www.arduino.cc/en/Tutorial/LiquidCrystal
// and
// http://www.pjrc.com/teensy/td_libs_OneWire.html
//
#include <OneWire.h>
#include <LiquidCrystal.h>

#define DEBUG 0
#define COLUMNS 16
#define LINES 2

byte device_number = 0;

// initialize the LCD with the numbers of the interface pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// initialise the OnewWire device
OneWire  ds(2);  // on pin 10 (a 4.7K resistor is necessary)

int min_temp, max_temp;

void setup(void) {
  lcd.begin(COLUMNS, LINES); 	   // set up the LCD's number of columns and rows:
  lcd.print("J-Squared");  // Print a message to the LCD.
  min_temp = 99;
  max_temp = -99;
  Serial.begin(9600);
  while (!Serial);
  Serial.println("Initialising Bridge...");
  delay(2000);
  Bridge.begin();
  Serial.println("Bridge initialised");
}

void loop(void) {
  byte i;  
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius;
  char buff[256];
  Process proc;
  
  
  if ( !ds.search(addr)) {
    if (DEBUG) Serial.println("No more addresses.");
    if (DEBUG) Serial.println();
    ds.reset_search();
    device_number = 0;
    delay(60000);
    return;
  }

  if (DEBUG) {
    Serial.print("ROM =");
    for( i = 0; i < 8; i++) {
      Serial.write(' ');
      Serial.print(addr[i], HEX);
    }
  }
  
  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return;
  }
  if (DEBUG) Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      if (DEBUG) Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      if (DEBUG) Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      if (DEBUG) Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return;
  } 

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  if (DEBUG) {
    Serial.print("  Data = ");
    Serial.print(present, HEX);
    Serial.print(" ");
  }
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    if (DEBUG) Serial.print(data[i], HEX);
    if (DEBUG) Serial.print(" ");
  }

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = raw / 16.0;

  sprintf(buff,"python /root/update.py %d %d", device_number, (int)celsius);
  Serial.println(buff);
  int r = proc.runShellCommand(buff);
  
  if (device_number < LINES) {
  if (device_number == 0) {
    lcd.setCursor(0,device_number);
    lcd.print("Current ");
    lcd.print((int)celsius);
    lcd.print((char)223); lcd.print("C");
    lcd.print("   ");
    lcd.setCursor(0,1);
    if (celsius < min_temp) min_temp = celsius;
    if (celsius > max_temp) max_temp = celsius;
    lcd.print("Min "); lcd.print(min_temp); lcd.print("  ");
    lcd.print("Max "); lcd.print(max_temp); lcd.print("  ");
  }
  }
  device_number++;
}
