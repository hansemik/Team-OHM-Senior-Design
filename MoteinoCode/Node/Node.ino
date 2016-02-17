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
//    if (input >= 48 && input <= 57) //[0,9]
//    {
//      TRANSMITPERIOD = 100 * (input-48);
//      if (TRANSMITPERIOD == 0) TRANSMITPERIOD = 1000;
//      Serial.print("\nChanging delay to ");
//      Serial.print(TRANSMITPERIOD);
//      Serial.println("ms\n");
//    }

    //if (input == 'r') //d=dump register values
    //  radio.readAllRegs();
    //if (input == 'E') //E=enable encryption
    //  radio.encrypt(KEY);
    //if (input == 'e') //e=disable encryption
    //  radio.encrypt(null);

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

    if (input == 'a')
    {
      //read adc values
      //todo: enable, get cpp and h files:LT_SPI
      
      menu_1_read_single_ended();
      //char ID = itoa(NODEID);
      char lo = ADCcode & 0xFF;
      char lo1[2];
      sprintf(lo1,"%x",lo);
      char hi = ADCcode >> 8;
      char hi1[2];
      sprintf(hi1,"%x",hi);
      char ID[1] = {' '};
      itoa(NODEID,ID,10);
      //char sending[] = {*ID, ' ', hi1[0],hi1[1], lo1[0],lo1[1]};
      char val[5];
      sprintf(val,"%x",ADCcode);
      char sending[] = {*ID, ' ', val[0],val[1],val[2],val[3],val[4]};
      
      //strncpy(sending, *ID, 10-1);
      //strncat(sending, ' ', 10 - strlen(sending)-1);
      //strncat(sending, "\x"lo, 10 - strlen(sending)-1);
      //strncat(sending, "\x"hi, 10 - strlen(sending)-1);
      //char array[50];
      //s.toCharArray(sending,5);
      //sprintf(buff, sending);
      Serial.println(ID);
      Serial.println(hi,HEX);
      Serial.println(lo,HEX);
      Serial.println(val);
      Serial.println(ADCcode);
      Serial.println(sending);
      radio.send(GATEWAYID, sending, 6, 0);

      
      Serial.print("DONE");
    }
    if (input == 's')
    {
      Serial.print("Set pin 7");
      //set pin 7
      pinMode(2, OUTPUT);
        digitalWrite(2, HIGH);
    }
    if (input == 'u')
    {
      Serial.print("Un-set pin 7");
      //un-set pin 7
      pinMode(LTC1867L_SS, OUTPUT);
      digitalWrite(LTC1867L_SS, LOW);
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

//  int currPeriod = millis()/TRANSMITPERIOD;
//  if (currPeriod != lastPeriod)
//  {
//    lastPeriod=currPeriod;
//
//    //Serial.println("Here");
//    //send FLASH id
//    if(sendSize==0)
//    {
////      sprintf(buff, "FLASH_MEM_ID:0x%X", flash.readDeviceId());
////      byte buffLen=strlen(buff);
////      if (radio.sendWithRetry(GATEWAYID, buff, buffLen))
////        Serial.print(" ok!");
////      else Serial.print(" nothing...");
////      //sendSize = (sendSize + 1) % 11;
//    }
//    else
//    {
////      Serial.print("Sending[");
////      Serial.print(sendSize);
////      Serial.print("]: ");
////      for(byte i = 0; i < sendSize; i++)
////        Serial.print((char)payload[i]);
//
//      //char sending[] = itoa(voltage);
//
//      String s = String(adc_voltage);
//      char sending[] = "";
//      s.toCharArray(sending,5);
//      //sprintf(buff, sending);
//  
////      if (radio.sendWithRetry(GATEWAYID, sending, 5))
////       Serial.print(" ok!");
////      else Serial.print(" nothing...");
//      radio.send(GATEWAYID, sending, 7, 0);
//    }
//    sendSize = (sendSize + 1) % 7;
//    Serial.println();
//    Blink(LED,3);
//  }
}

void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}


//! Read channels in single-ended mode
//! @return void
void menu_1_read_single_ended()
{
  uint8_t user_command;
  uint8_t adc_command;                             // The LTC1867 command byte
  uint16_t adc_code = 0;                           // The LTC1867 code
  float adc_voltage;                               // The LTC1867 voltage
  
  pinMode(LTC1867L_SS, OUTPUT);
  SPI.begin();
  quikeval_SPI_init();
  
  while (1)
  {
    if (uni_bi_polar == LTC1867_UNIPOLAR_MODE)
      Serial.println(F("Single-Ended, Unipolar mode:"));
    else
      Serial.println(F("Single-Ended, Bipolar mode:"));

    Serial.println(F("*************************"));            // Display single-ended menu
    Serial.println();
    Serial.println(F("0-CH0"));
    Serial.println(F("1-CH1"));
    Serial.println(F("2-CH2"));
    Serial.println(F("3-CH3"));
    Serial.println(F("4-CH4"));
    Serial.println(F("5-CH5"));
    Serial.println(F("6-CH6"));
    Serial.println(F("7-CH7"));
    Serial.println(F("8-ALL"));
    Serial.println(F("m-Main Menu"));
    Serial.print(F("Enter a Command: "));

    user_command = read_int();                                 // Read the single command
    if (user_command == 'm')
      return;

    Serial.println(user_command);

    if (user_command == 8)
    {
      Serial.println(F("ALL"));
      adc_command = BUILD_COMMAND_SINGLE_ENDED[0] | uni_bi_polar;   // Build ADC command for channel 0
      LTC1867_read(LTC1867L_SS, adc_command, &adc_code);             // Throws out last reading
      delay(100);
      uint8_t x;                                                    //!< iteration variable
      for (x = 0; x <= 7; x++)                                      // Read all channels in single-ended mode
      {
        adc_command = BUILD_COMMAND_SINGLE_ENDED[(x + 1) % 8] | uni_bi_polar;
        LTC1867_read(LTC1867L_SS, adc_command, &adc_code);
        LTC1867_read(LTC1867L_SS, adc_command, &adc_code);
        if (uni_bi_polar == LTC1867_UNIPOLAR_MODE)
          adc_voltage = LTC1867_unipolar_code_to_voltage(adc_code, LTC1867_lsb, LTC1867_offset_unipolar_code);
        else
          adc_voltage = LTC1867_bipolar_code_to_voltage(adc_code, LTC1867_lsb, LTC1867_offset_bipolar_code);
        Serial.print(F("  ****"));
        Serial.print(F("CH"));
        Serial.print(x);
        Serial.print(F(": "));
        Serial.print(adc_voltage, 4);
        Serial.println(F("V"));
        Serial.println();
      }
    }
    else
    {
      adc_command = BUILD_COMMAND_SINGLE_ENDED[user_command] | uni_bi_polar;
      Serial.println();
      Serial.print(F("ADC Command: B"));
      Serial.println(adc_command, BIN);
      LTC1867_read(LTC1867L_SS, adc_command, &adc_code); // Throws out last reading
      delay(100);
      LTC1867_read(LTC1867L_SS, adc_command, &adc_code);
      ADCcode = adc_code;
      Serial.print(F("Received Code: 0x"));
      Serial.println(adc_code, HEX);

      if (uni_bi_polar == LTC1867_UNIPOLAR_MODE)
        adc_voltage = LTC1867_unipolar_code_to_voltage(adc_code, LTC1867_lsb, LTC1867_offset_unipolar_code)/1.6384;
      else
        adc_voltage = LTC1867_bipolar_code_to_voltage(adc_code, LTC1867_lsb, LTC1867_offset_bipolar_code)/1.6384;
      Serial.print(F("  ****"));
      Serial.print(F("CH"));
      Serial.print(user_command);
      Serial.print(F(": "));
      Serial.print(adc_voltage, 4);
      Serial.println(F("V"));
      Serial.println();
    }
  }
}

////! Calibrate ADC given two known inputs
////! @ return void
//void menu_4_calibrate()
//{
//  uint8_t user_command;
//  uint8_t adc_command;                             // The LTC1867 command byte
//  float fs_voltage;                                // Measured cal voltage
//  float zero_voltage = 0.0;                        // Zero Voltage
//  uint16_t zero_bipolar_code;                      // Cal zero code
//  uint16_t zero_unipolar_code;                     // Cal zero code
//  uint16_t fs_code;                                // Cal full scale code
//  
//  // Used to wake up the ADC if it is in sleep mode. 
//  LTC1867_read(LTC1867_CS, BUILD_COMMAND_SINGLE_ENDED[0], &zero_unipolar_code);
//  delay(500); 
//     
//  // Calibration
//  // Accuracy: +- 2 lsb between channels
//  Serial.println(F("Apply 100mV to CH0 and connect CH1 to GND."));  
//  Serial.println(F("Enter the measured input voltage for CH0:"));
//  zero_voltage = read_float();
//  Serial.println(zero_voltage, 8);
//  
//  adc_command = BUILD_COMMAND_SINGLE_ENDED[0]| LTC1867_UNIPOLAR_MODE;   // Build ADC command byte for voltage input
//  LTC1867_read(LTC1867_CS, adc_command, &zero_unipolar_code);           // Throw away previous reading
//  delay(200);
//  LTC1867_read(LTC1867_CS, adc_command, &zero_unipolar_code);           // Measure zero
//  
//  adc_command = BUILD_COMMAND_DIFF[0] | LTC1867_BIPOLAR_MODE;           // Build ADC command byte for CH0 and CH1
//  LTC1867_read(LTC1867_CS, adc_command, &zero_bipolar_code);            // Throw away previous reading
//  delay(200);
//  LTC1867_read(LTC1867_CS, adc_command, &zero_bipolar_code);            // Measure zero
//  
//  Serial.println(F("Apply ~4.00V input voltage to CH0."));
//  Serial.println(F("Enter the measured input voltage:"));
//  fs_voltage = read_float();
//  Serial.println(fs_voltage, 8);
//  
//  adc_command = BUILD_COMMAND_SINGLE_ENDED[0] | LTC1867_UNIPOLAR_MODE;  // Build ADC command byte for voltage input
//  LTC1867_read(LTC1867_CS, adc_command, &fs_code);                      // Throw away previous reading
//  delay(200);
//  LTC1867_read(LTC1867_CS, adc_command, &fs_code);                      // Measure full scale
//
//  LTC1867_cal_voltage(zero_unipolar_code, zero_bipolar_code, fs_code, zero_voltage, fs_voltage, &LTC1867_lsb, &LTC1867_offset_unipolar_code, &LTC1867_offset_bipolar_code);
//  
//  Serial.print(F("ADC unipolar offset : "));
//  Serial.println(LTC1867_offset_unipolar_code);
//  Serial.print(F("ADC bipolar offset : "));
//  Serial.println(LTC1867_offset_bipolar_code);
//  Serial.print(F("ADC lsb : "));
//  Serial.print(LTC1867_lsb*1.0e9, 4);
//  Serial.println(F("nV (32-bits)"));
//  store_calibration();
//}
//
//
////! Store measured calibration parameters to nonvolatile EEPROM on demo board
////! @return void
//void store_calibration()
//// Store the ADC calibration to the EEPROM
//{
//  eeprom_write_int16(EEPROM_I2C_ADDRESS, EEPROM_CAL_KEY, EEPROM_CAL_STATUS_ADDRESS);                    // cal key
//  eeprom_write_int32(EEPROM_I2C_ADDRESS, LTC1867_offset_unipolar_code, EEPROM_CAL_STATUS_ADDRESS+2);    // offset
//  eeprom_write_int32(EEPROM_I2C_ADDRESS, LTC1867_offset_bipolar_code, EEPROM_CAL_STATUS_ADDRESS+6);     // offset
//  eeprom_write_float(EEPROM_I2C_ADDRESS, LTC1867_lsb, EEPROM_CAL_STATUS_ADDRESS+10);                    // lsb
//  Serial.println(F("Calibration Stored to EEPROM"));
//}
//
////! Read stored calibration parameters from nonvolatile EEPROM on demo board
////! @return Return 1 if successful, 0 if not
//int8_t restore_calibration()
//// Read the calibration from EEPROM
//// Return 1 if successful, 0 if not
//{
//  int16_t cal_key;
//  // read the cal key from the EEPROM
//  eeprom_read_int16(EEPROM_I2C_ADDRESS, &cal_key, EEPROM_CAL_STATUS_ADDRESS);
//  if (cal_key == EEPROM_CAL_KEY)
//  {
//    // Calibration has been stored, read offset and lsb
//    eeprom_read_int32(EEPROM_I2C_ADDRESS, &LTC1867_offset_unipolar_code, EEPROM_CAL_STATUS_ADDRESS+2);    // offset
//    eeprom_read_int32(EEPROM_I2C_ADDRESS, &LTC1867_offset_bipolar_code, EEPROM_CAL_STATUS_ADDRESS+6);     // offset
//    eeprom_read_float(EEPROM_I2C_ADDRESS, &LTC1867_lsb, EEPROM_CAL_STATUS_ADDRESS+10);                    // lsb
//    Serial.println(F("Calibration Restored"));
//    return(1);
//  }
//  else
//  {
//    Serial.println(F("Calibration not found"));
//    return(0);
//  }
//}
//


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

