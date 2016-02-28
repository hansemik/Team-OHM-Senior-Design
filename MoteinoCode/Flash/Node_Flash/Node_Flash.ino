// Sample RFM69 sender/node sketch, with ACK and optional encryption, and Automatic Transmission Control
// Sends periodic messages of increasing length to gateway (id=1)
// It also looks for an onboard FLASH chip, if present
// RFM69 library and sample code by Felix Rusu - http://LowPowerLab.com/contact
// Copyright Felix Rusu (2015)

#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include <RFM69_ATC.h>//get it here: https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>
#include <SPIFlash.h> //get it here: https://www.github.com/lowpowerlab/spiflash
#include <LTC1867.h>
#include <UserInterface.h>
#include <QuikEval_EEPROM.h>
#include <LT_SPI.h>
#include <String.h>

//*********************************************************************************************
//************ IMPORTANT SETTINGS - YOU MUST CHANGE/CONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
#define NODEID        2    //must be unique for each node on same network (range up to 254, 255 is used for broadcast)
#define NETWORKID     100  //the same on all nodes that talk to each other (range up to 255)
#define GATEWAYID     1
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
//#define FREQUENCY   RF69_433MHZ
//#define FREQUENCY   RF69_868MHZ
#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
//#define ENABLE_ATC    //comment out this line to disable AUTO TRANSMISSION CONTROL
//*********************************************************************************************

#ifdef __AVR_ATmega1284P__
  #define LED           15 // Moteino MEGAs have LEDs on D15
  #define FLASH_SS      23 // and FLASH SS on D23
#else
  #define LED           9 // Moteinos have LEDs on D9
  #define FLASH_SS      8 // and FLASH SS on D8
  #define LTC1867L_SS   7 // Pin for chip select of ADC
#endif

#define SERIAL_BAUD   115200

int TRANSMITPERIOD = 10; //transmit a packet to gateway so often (in ms)
char payload[] = "1234";
char buff[20];
byte sendSize=0;
boolean requestACK = false;
SPIFlash flash(FLASH_SS, 0xEF40); //EF40 for 8mbit  Windbond chip (W25X40CL)
uint32_t addr = 0000;

#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif

static uint8_t uni_bi_polar = LTC1867_UNIPOLAR_MODE;    //!< The LTC1867 unipolar/bipolar mode selection
static float LTC1867_lsb = 6.25009537E-5;               //!< Ideal LSB voltage for a perfect part
static int32_t LTC1867_offset_unipolar_code = 0;        //!< Ideal unipolar offset for a perfect part
static int32_t LTC1867_offset_bipolar_code = 0;         //!< Ideal bipolar offset for a perfect part
uint16_t ADCcode = 0;

//! Lookup table to build the command for single-ended mode, input with respect to GND
const uint8_t BUILD_COMMAND_SINGLE_ENDED[8] = {LTC1867_CH0, LTC1867_CH1, LTC1867_CH2, LTC1867_CH3,
    LTC1867_CH4, LTC1867_CH5, LTC1867_CH6, LTC1867_CH7}; //!< Builds the command for single-ended mode, input with respect to GND

void setup() {
  pinMode(A1, INPUT);
  
  Serial.begin(SERIAL_BAUD);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);
  //radio.setFrequency(919000000); //set frequency to some custom frequency
  
//Auto Transmission Control - dials down transmit power to save battery (-100 is the noise floor, -90 is still pretty good)
//For indoor nodes that are pretty static and at pretty stable temperatures (like a MotionMote) -90dBm is quite safe
//For more variable nodes that can expect to move or experience larger temp drifts a lower margin like -70 to -80 would probably be better
//Always test your ATC mote in the edge cases in your own environment to ensure ATC will perform as you expect
#ifdef ENABLE_ATC
  radio.enableAutoPower(-70);
#endif
  
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  
  if (flash.initialize())
  {
    Serial.print("SPI Flash Init OK ... UniqueID (MAC): ");
    flash.readUniqueId();
    for (byte i=0;i<8;i++)
    {
      Serial.print(flash.UNIQUEID[i], HEX);
      Serial.print(' ');
    }
    Serial.println();
  }
  else
    Serial.println("SPI Flash MEM not found (is chip soldered?)...");
    
#ifdef ENABLE_ATC
  Serial.println("RFM69_ATC Enabled (Auto Transmission Control)\n");
#endif
}

long lastPeriod = 0;
int voltage = 0;
//int A_pin = 0

void loop() {
  float adc_voltage;
  uint8_t user_command;
  uint8_t adc_command;                             // The LTC1867 command byte
  uint16_t adc_code = 0;                           // The LTC1867 code

    Blink(LED,3);  
  //pinMode(LTC1867L_SS, OUTPUT);
  //SPI.begin();
  //quikeval_SPI_init();

  //adc_command = BUILD_COMMAND_SINGLE_ENDED[0] | uni_bi_polar;
  //LTC1867_read(LTC1867L_SS, adc_command, &adc_code); // Throws out last reading
  //LTC1867_read(LTC1867L_SS, adc_command, &adc_code);
  //Serial.print(F("Received Code: 0x"));
  //Serial.println(adc_command, HEX);

  //if (uni_bi_polar == LTC1867_UNIPOLAR_MODE)
  //  adc_voltage = LTC1867_unipolar_code_to_voltage(adc_code, LTC1867_lsb, LTC1867_offset_unipolar_code)/1.6384;

  //Serial.print(adc_voltage, 4);
  
  
  
  //process any serial input
  //voltage = analogRead(A0);
  //Serial.print("Voltage =  ");
  //Serial.print(voltage);
  //Serial.println();
  
  if (Serial.available() > 0)
  {
    char input = Serial.read();

    if (input == 'd') //d=dump flash area
    {
      Serial.println("Flash content:");
      uint16_t counter = 0;

      Serial.print("0-256: ");
      while(counter<=256){
        Serial.print(flash.readByte(counter++), HEX);
        Serial.print('.');
      }
      while(flash.busy());
      Serial.println();
    }
    if (input == 'e')
    {
      Serial.print("Erasing Flash chip ... ");
      flash.chipErase();
      while(flash.busy());
      Serial.println("DONE");
    }
    if (input == 'i')
    {
      Serial.print("DeviceID: ");
      word jedecid = flash.readDeviceId();
      Serial.println(jedecid, HEX);
    }


    if (input == 'w') 
    {
      Serial.print("Enter string to save to flash chip:");
      if (readline(buff, 6, 10000) > 0)
      {
        Serial.println();
        Serial.print("Writing ");
        Serial.print(buff);
        Serial.println(" to flash memory.");
        Serial.println(strlen(buff));

        flash.writeBytes(addr, buff, strlen(buff));        
        addr = addr + (uint32_t)strlen(buff);
        Serial.println(addr);
      }
    }

    if (input == 'r')
    {
      flash.readBytes(addr - strlen(buff), buff, strlen(buff));
      Serial.print("Read ");
      Serial.print(buff);
      Serial.print(" from flash.");
      Serial.println();
    }
  }


  

  //check for any received packets
  if (radio.receiveDone())
  {
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
    for (byte i = 0; i < radio.DATALEN; i++)
      Serial.print((char)radio.DATA[i]);
    Serial.print("   [RX_RSSI:");Serial.print(radio.RSSI);Serial.print("]");

    if (radio.ACKRequested())
    {
      radio.sendACK();
      Serial.print(" - ACK sent");
    }
    Blink(LED,3);
    Serial.println();
  }
}

void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}



void flushSerial() {
  while (Serial.available())
    Serial.read();
}

char readBlocking() {
  while (!Serial.available());
  return Serial.read();
}
uint16_t readnumber() {
  uint16_t x = 0;
  char c;
  while (! isdigit(c = readBlocking())) {
    //Serial.print(c);
  }
  Serial.print(c);
  x = c - '0';
  while (isdigit(c = readBlocking())) {
    Serial.print(c);
    x *= 10;
    x += c - '0';
  }
  return x;
}


uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout) {
  uint16_t buffidx = 0;
  boolean timeoutvalid = true;
  if (timeout == 0) timeoutvalid = false;

  while (true) {
    if (buffidx > maxbuff) {
      //Serial.println(F("SPACE"));
      break;
    }

    while (Serial.available()) {
      char c =  Serial.read();

      //Serial.print(c, HEX); Serial.print("#"); Serial.println(c);

      if (c == '\r') continue;
      if (c == 0xA) {
        if (buffidx == 0)   // the first 0x0A is ignored
          continue;

        timeout = 0;         // the second 0x0A is the end of the line
        timeoutvalid = true;
        break;
      }
      buff[buffidx] = c;
      buffidx++;
    }

    if (timeoutvalid && timeout == 0) {
      //Serial.println(F("TIMEOUT"));
      break;
    }
    delay(1);
  }
  buff[buffidx] = 0;  // null term
  return buffidx;
}

