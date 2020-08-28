// **********************************************************************************
// GBM_Moteino
//
// Michael Blank,
#define VERSION "V2020-08-28 SP000"    // VERSION and Gleisspannung  
//  OTA working
//
// **********************************************************************************
// Hardware setup:
// GBM input at A7
// **********************************************************************************
// Copyright Felix Rusu 2016
//           Michael Blank 2020
// License
// **********************************************************************************
// This program is free software; you can redistribute it
// and/or modify it under the terms of the GNU General
// Public License as published by the Free Software
// Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will
// be useful, but WITHOUT ANY WARRANTY; without even the
// implied warranty of MERCHANTABILITY or FITNESS FOR A
// PARTICULAR PURPOSE. See the GNU General Public
// License for more details.
//
// Licence can be viewed at
// http://www.gnu.org/licenses/gpl-3.0.txt
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code
// **********************************************************************************
#include <RFM69.h>         //get it here: https://www.github.com/lowpowerlab/rfm69
#include <RFM69_ATC.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_OTA.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <SPIFlash.h>      //get it here: https://github.com/lowpowerlab/spiflash

#include <EEPROM.h>
#include <SPI.h>           //included with Arduino IDE (www.arduino.cc)

//****************************************************************************************************************
//**** IMPORTANT RADIO SETTINGS - YOU MUST CHANGE/CONFIGURE TO MATCH YOUR HARDWARE TRANSCEIVER CONFIGURATION! ****
//****************************************************************************************************************
#define NETWORKID     100  //the same on all nodes that talk to each other

//Match frequency to the hardware version of the radio on your Moteino (uncomment one):

#define FREQUENCY     RF69_868MHZ
#define ENCRYPTKEY    "ibmklub123456789" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW_HCW
//*****************************************************************************************************************************
//#define ENABLE_ATC      //comment out this line to disable AUTO TRANSMISSION CONTROL
#define ATC_RSSI        -75
//*********************************************************************************************
#define SERIAL_BAUD   115200
#ifdef __AVR_ATmega1284P__
#define LED           15 // Moteino MEGAs have LEDs on D15
#else
#define LED           9 // Moteinos have LEDs on D9
#endif


//*****************************************************************************************************************************
// flash(SPI_CS, MANUFACTURER_ID)
// SPI_CS          - CS pin attached to SPI flash chip (8 in case of Moteino)
// MANUFACTURER_ID - OPTIONAL, 0x1F44 for adesto(ex atmel) 4mbit flash
//                             0xEF30 for windbond 4mbit flash
//                             0xEF40 for windbond 16/64mbit flash
//                             0x1F84 for adesto 4mbit AT25SF041 4MBIT flash
//*****************************************************************************************************************************
SPIFlash flash(SS_FLASHMEM, 0xEF30); //EF30 for windbond 4mbit flash


#define AUTOSEND_INTERVAL   (19900)     // every 20 seconds
#define VERSION_INTERVAL  (300000)    // every 5 minutes

uint8_t nodeID;  // will be read from EEPROM
#define GATEWAY   1   // id of the receiving node

#ifdef ENABLE_ATC
RFM69_ATC radio;
#else
RFM69 radio;
#endif

#define TX_POWER   0    // range 0..31  (+5dBm ... +20dBm)

uint16_t tmp = 0;
int8_t occ = 0, lastOccSent = 0;
int8_t filtOcc = 0;
uint32_t lastSent = 0, versionSent = 0;
uint32_t version_interval, autosend_interval;

char buf[] = "G09 0";     // msg buffer to send
// example 'G09 1' : G=GBM, ID=9, value = 1 (=occupied)
char ver[] = VERSION;

// convert 0..999 to char[] and include in ver[]
void analogToVerArray(uint16_t v) {
    if (v > 999) v = 999;
    uint8_t tmp = v / 100;
    ver[14] = '0' + tmp;
    v = (v - tmp * 100);
    tmp = v / 10;
    ver[15] = '0' + tmp;
    v = v - tmp * 10;
    ver[16] = '0' + v;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.print("GBM_Moteino - ");
  Serial.println(VERSION);

  analogReference(INTERNAL);    //  1 Volt, ATmega328  - 1 digit is equal to approx 1mV

  nodeID = EEPROM.read(1);
  if (nodeID == 0xFF) {
    Serial.println("ERROR, nodeID not set in EEPROM");
    nodeID = 99;
  }
  if (nodeID > 10) buf[1] = '0' + nodeID / 10;
  buf[2] = '0' + nodeID % 10;


  radio.initialize(FREQUENCY, nodeID, NETWORKID);
#ifdef IS_RFM69HW_HCW
  radio.setHighPower(); //must include this only for RFM69HW/HCW!
  radio.setPowerLevel(TX_POWER);
#endif
  radio.encrypt(ENCRYPTKEY);

#ifdef ENABLE_ATC
  radio.enableAutoPower(ATC_RSSI);
#endif

  randomSeed(analogRead(A0));
  // randomize send intervals
  version_interval = VERSION_INTERVAL + random(100);
  autosend_interval = AUTOSEND_INTERVAL + random(100);
  pinMode(LED, OUTPUT);

  Serial.print("starting... ID=");
  Serial.println(nodeID);

  if (flash.initialize())
    Serial.println("SPI Flash Init OK!");
  else
    Serial.println("SPI Flash Init FAIL!");

#ifdef BR_300KBPS
  radio.writeReg(0x03, 0x00);  //REG_BITRATEMSB: 300kbps (0x006B, see DS p20)
  radio.writeReg(0x04, 0x6B);  //REG_BITRATELSB: 300kbps (0x006B, see DS p20)
  radio.writeReg(0x19, 0x40);  //REG_RXBW: 500kHz
  radio.writeReg(0x1A, 0x80);  //REG_AFCBW: 500kHz
  radio.writeReg(0x05, 0x13);  //REG_FDEVMSB: 300khz (0x1333)
  radio.writeReg(0x06, 0x33);  //REG_FDEVLSB: 300khz (0x1333)
  radio.writeReg(0x29, 240);   //set REG_RSSITHRESH to -120dBm
#endif

}


void loop() {

  // get adc reading
  tmp = 0;
  for (int j = 0; j < 16; j++) {
    tmp += analogRead(A7);   // average
    delay(1);
  }
  tmp = tmp / 64;  // effectivly dividing by 4 / 1 digit = 4 mV

  if (tmp >= 5) {   // => non occupation delay
    filtOcc = 4;
  } else {
    if (filtOcc > 0) filtOcc--;
  }

  if (filtOcc > 0) {
    occ = 1;
  } else {
    occ = 0;
  }

  // send via RFM when value has changed or after AUTOSEND_INTERVAL milliseconds
  if (
    (occ != lastOccSent)
    ||
    ((millis() - lastSent) > autosend_interval )) {

    buf[4] = occ + '0';
    Serial.print(occ);

    uint8_t len = strlen(buf);
    if (radio.sendWithRetry(GATEWAY, buf, len)) {
      lastSent = millis();
      lastOccSent = occ;
      //target node Id, message as string or byte array, message length
      Blink(LED, 40, 1); //blink LED once, 40ms between blinks
      Serial.println(" success");
    }
  }

  // check if version needs to be sent
  if ((millis() - versionSent) > version_interval) {
    // reaq input voltage from track
    uint16_t volts = analogRead(A6) >> 2;
    Serial.print("Volt*10=");
    Serial.println(volts);
    analogToVerArray(volts);
    
    uint8_t vlen = strlen(ver);
    if (radio.sendWithRetry(GATEWAY, ver, vlen)) {
      Serial.println((char *)ver);
    }
    versionSent = millis();  // ignore acknowledge state
  }

  //check if something was received (could be an interrupt from the radio)
  if (radio.receiveDone()) {
    //print message received to serial
    Serial.print('['); Serial.print(radio.SENDERID); Serial.print("] ");
    Serial.print((char*)radio.DATA);
    Serial.print("   [RX_RSSI:"); Serial.print(radio.RSSI); Serial.print("]");
    Serial.println();

    /*//check if sender wanted an ACK
       // NOTE: this does NOT work in combination with CheckForWirelessHEX !!
      if (radio.ACKRequested())
      {
      radio.sendACK();
      }*/

    CheckForWirelessHEX(radio, flash, false);
  }

  radio.receiveDone(); //put radio in RX mode

  Serial.flush(); //make sure all serial data is clocked out before sleeping the MCU

  // ATmega328P, ATmega168 -see idleWakePeriodic.ino

  //LowPower.idle(SLEEP_2S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF,
  //              SPI_OFF, USART0_OFF, TWI_OFF);

  //LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_ON); //sleep Moteino in low power mode (to save battery)

  delay(150);
}

void Blink(byte PIN, byte DELAY_MS, byte loops)
{
  for (byte i = 0; i < loops; i++)
  {
    digitalWrite(PIN, HIGH);
    delay(DELAY_MS);
    digitalWrite(PIN, LOW);
    delay(DELAY_MS);
  }
}
