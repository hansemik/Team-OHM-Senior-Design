// Sample RFM69 receiver/gateway sketch, with ACK and optional encryption, and Automatic Transmission Control
// Passes through any wireless received messages to the serial port & responds to ACKs
// It also looks for an onboard FLASH chip, if present
// RFM69 library and sample code by Felix Rusu - http://LowPowerLab.com/contact
// Copyright Felix Rusu (2015)

#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include <RFM69_ATC.h>//get it here: https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>      //comes with Arduino IDE (www.arduino.cc)
#include <SPIFlash.h> //get it here: https://www.github.com/lowpowerlab/spiflash
#include <LTC1867.h>
#include <UserInterface.h>
#include <QuikEval_EEPROM.h>
#include <LT_SPI.h>
#include "Adafruit_FONA.h"

//*********************************************************************************************
//************ IMPORTANT SETTINGS - YOU MUST CHANGE/CONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
#define NODEID        1    //unique for each node on same network
#define NETWORKID     100  //the same on all nodes that talk to each other
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
//#define FREQUENCY     RF69_433MHZ
//#define FREQUENCY     RF69_868MHZ
#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
//#define ENABLE_ATC    //comment out this line to disable AUTO TRANSMISSION CONTROL
//*********************************************************************************************

#define SERIAL_BAUD   115200

#ifdef __AVR_ATmega1284P__
  #define LED           15 // Moteino MEGAs have LEDs on D15
  #define FLASH_SS      23 // and FLASH SS on D23
#else
  #define LED           9 // Moteinos have LEDs on D9
  #define FLASH_SS      8 // and FLASH SS on D8
  #define LTC1867L_SS   7 // Pin for chip select of ADC
  #define FONA_RX       2
  #define FONA_TX       3
  #define FONA_RST      4
#endif

#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif

// this is a large buffer for replies
char replybuffer[255];

SPIFlash flash(FLASH_SS, 0xEF40); //EF40 for 8mbit  Windbond chip (W25X40CL)
bool promiscuousMode = false; //set to 'true' to sniff all packets on the same network

// We default to using software serial. If you want to use hardware serial
// (because softserial isnt supported) comment out the following three lines 
// and uncomment the HardwareSerial line
#include <SoftwareSerial.h>
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;
// Use this one for FONA 3G
Adafruit_FONA_3G fona = Adafruit_FONA_3G(FONA_RST);

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);

uint8_t type;

static uint8_t uni_bi_polar = LTC1867_UNIPOLAR_MODE;    //!< The LTC1867 unipolar/bipolar mode selection
static float LTC1867_lsb = 6.25009537E-5;               //!< Ideal LSB voltage for a perfect part
static int32_t LTC1867_offset_unipolar_code = 0;        //!< Ideal unipolar offset for a perfect part
static int32_t LTC1867_offset_bipolar_code = 0;         //!< Ideal bipolar offset for a perfect part

//! Lookup table to build the command for single-ended mode, input with respect to GND
const uint8_t BUILD_COMMAND_SINGLE_ENDED[8] = {LTC1867_CH0, LTC1867_CH1, LTC1867_CH2, LTC1867_CH3,
    LTC1867_CH4, LTC1867_CH5, LTC1867_CH6, LTC1867_CH7}; //!< Builds the command for single-ended mode, input with respect to GND

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(10);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);
  radio.promiscuous(promiscuousMode);
  //radio.setFrequency(919000000); //set frequency to some custom frequency
  char buff[50];
  sprintf(buff, "\nListening at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  if (flash.initialize())
  {
    Serial.print("SPI Flash Init OK. Unique MAC = [");
    flash.readUniqueId();
    for (byte i=0;i<8;i++)
    {
      Serial.print(flash.UNIQUEID[i], HEX);
      if (i!=8) Serial.print(':');
    }
    Serial.println(']');
    
    //alternative way to read it:
    //byte* MAC = flash.readUniqueId();
    //for (byte i=0;i<8;i++)
    //{
    //  Serial.print(MAC[i], HEX);
    //  Serial.print(' ');
    //}
  }
  else
    Serial.println("SPI Flash MEM not found (is chip soldered?)...");
    
#ifdef ENABLE_ATC
  Serial.println("RFM69_ATC Enabled (Auto Transmission Control)");
#endif

  fonaSerial->begin(115200);
  if (! fona.begin(*fonaSerial)) {
    Serial.println(F("Couldn't find FONA"));
    while (1);
  }
  Serial.println(F("Hello?"));
  type = fona.type();
  Serial.println(F("FONA is OK"));
  Serial.print(F("Found "));
  switch (type) {
    case FONA800L:
      Serial.println(F("FONA 800L")); break;
    case FONA800H:
      Serial.println(F("FONA 800H")); break;
    case FONA808_V1:
      Serial.println(F("FONA 808 (v1)")); break;
    case FONA808_V2:
      Serial.println(F("FONA 808 (v2)")); break;
    case FONA3G_A:
      Serial.println(F("FONA 3G (American)")); break;
    case FONA3G_E:
      Serial.println(F("FONA 3G (European)")); break;
    default: 
      Serial.println(F("???")); break;
  }
  
  // Print module IMEI number.
  char imei[15] = {0}; // MUST use a 16 character buffer for IMEI!
  uint8_t imeiLen = fona.getIMEI(imei);
  if (imeiLen > 0) {
    Serial.print("Module IMEI: "); Serial.println(imei);
  }

  // Optionally configure a GPRS APN, username, and password.
  // You might need to do this to access your network's GPRS/data
  // network.  Contact your provider for the exact APN, username,
  // and password values.  Username and password are optional and
  // can be removed, but APN is required.
  fona.setGPRSNetworkSettings(F("wholesale"));

  // Optionally configure HTTP gets to follow redirects over SSL.
  // Default is not to follow SSL redirects, however if you uncomment
  // the following line then redirects over SSL will be followed.
  //fona.setHTTPSRedirect(true);
}

byte ackCount=0;
uint32_t packetCount = 0;
unsigned long time1;
unsigned long time2;
unsigned long diff;
int counter = 0;
bool flag = false;
float adc_voltage;                               // The LTC1867 voltage

void loop() {
  //process any serial input
  
//  uint8_t user_command;
//  uint8_t adc_command;                             // The LTC1867 command byte
//  uint16_t adc_code = 0;                           // The LTC1867 code
//
//  
//  pinMode(LTC1867L_SS, OUTPUT);
//  SPI.begin();
//  quikeval_SPI_init();
//  
//  LTC1867_read(LTC1867L_SS, adc_command, &adc_code); // Throws out last reading
//  LTC1867_read(LTC1867L_SS, adc_command, &adc_code);
//  Serial.print(F("Received Code: 0x"));
//  Serial.println(adc_code, HEX);
//
//  if (uni_bi_polar == LTC1867_UNIPOLAR_MODE)
//    adc_voltage = LTC1867_unipolar_code_to_voltage(adc_code, LTC1867_lsb, LTC1867_offset_unipolar_code)/1.6384;
//
//    
  if (counter == 1)
  {
    //Serial.println(time1);
    //Serial.println(time2);
    //Serial.println(time2 - time1);
    //flag = false;
  }
  time1 = micros();
  if (Serial.available() > 0)
  {
    char input = Serial.read();
    if (input == 'r') //d=dump all register values
      radio.readAllRegs();
    if (input == 'E') //E=enable encryption
      radio.encrypt(ENCRYPTKEY);
    if (input == 'e') //e=disable encryption
      radio.encrypt(null);
    if (input == 'p')
    {
      promiscuousMode = !promiscuousMode;
      radio.promiscuous(promiscuousMode);
      Serial.print("Promiscuous mode ");Serial.println(promiscuousMode ? "on" : "off");
    }
    
    if (input == 'd') //d=dump flash area
    {
      Serial.println("Flash content:");
      int counter = 0;

      while(counter<=256){
        Serial.print(flash.readByte(counter++), HEX);
        Serial.print('.');
      }
      while(flash.busy());
      Serial.println();
    }
    if (input == 'D')
    {
      Serial.print("Deleting Flash chip ... ");
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
    if (input == 't')
    {
      byte temperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
      byte fTemp = 1.8 * temperature + 32; // 9/5=1.8
      Serial.print( "Radio Temp is ");
      Serial.print(temperature);
      Serial.print("C, ");
      Serial.print(fTemp); //converting to F loses some resolution, obvious when C is on edge between 2 values (ie 26C=78F, 27C=80F)
      Serial.println('F');
    }
    if (input == 'a')
    {
      //read adc values
      //todo: enable, get cpp and h files:LT_SPI
      
      menu_1_read_single_ended();
      Serial.print("DONE");
    }
    if (input == 's')
    {
      Serial.print("Set pin 7");
      //set pin 7
      pinMode(LTC1867L_SS, OUTPUT);
        digitalWrite(LTC1867L_SS, HIGH);
    }
    if (input == 'u')
    {
      Serial.print("Un-set pin 7");
      //un-set pin 7
      pinMode(LTC1867L_SS, OUTPUT);
      digitalWrite(LTC1867L_SS, LOW);
    }

    
    if(input == 'g') {
        // turn GPRS off
        if (!fona.enableGPRS(false))
          Serial.println(F("Failed to turn off"));
      }
    if(input == 'G') {
        // turn GPRS on
        if (!fona.enableGPRS(true))
          Serial.println(F("Failed to turn on"));
      }
    if(input == 'l') {
        // check for GSMLOC (requires GPRS)
        uint16_t returncode;

        if (!fona.getGSMLoc(&returncode, replybuffer, 250))
          Serial.println(F("Failed!"));
        if (returncode == 0) {
          Serial.println(replybuffer);
        } else {
          Serial.print(F("Fail code #")); Serial.println(returncode);
        }

      }
    if(input == 'w') {
        // read website URL
        uint16_t statuscode;
        int16_t length;
        char url[80];

        flushSerial();
        Serial.println(F("NOTE: in beta! Use small webpages to read!"));
        Serial.println(F("URL to read (e.g. www.adafruit.com/testwifi/index.html):"));
        Serial.print(F("http://")); readline(url, 79);
        Serial.println(url);

        Serial.println(F("****"));
        if (!fona.HTTP_GET_start(url, &statuscode, (uint16_t *)&length)) {
          Serial.println("Failed!");
        }
        while (length > 0) {
          while (fona.available()) {
            char c = fona.read();

            // Serial.write is too slow, we'll write directly to Serial register!
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
            loop_until_bit_is_set(UCSR0A, UDRE0); /* Wait until data register empty. */
            UDR0 = c;
#else
            Serial.write(c);
#endif
            length--;
            
          }
        }
        Serial.println(F("\n****"));
        fona.HTTP_GET_end();
      }

    if(input == 'W') {
        // Post data to website
        uint16_t statuscode;
        int16_t length;
        char url[80];
        char data[80];

        flushSerial();
        Serial.println(F("NOTE: in beta! Use simple websites to post!"));
        Serial.println(F("URL to post (e.g. httpbin.org/post):"));
        Serial.print(F("http://")); readline(url, 79);
        Serial.println(url);
        Serial.println(F("Data to post (e.g. \"foo\" or \"{\"simple\":\"json\"}\"):"));
        readline(data, 79);
        Serial.println(data);

        Serial.println(F("****"));
        if (!fona.HTTP_POST_start(url, F("text/plain"), (uint8_t *) data, strlen(data), &statuscode, (uint16_t *)&length)) {
          Serial.println("Failed!");
        }
        while (length > 0) {
          while (fona.available()) {
            char c = fona.read();

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
            loop_until_bit_is_set(UCSR0A, UDRE0); /* Wait until data register empty. */
            UDR0 = c;
#else
            Serial.write(c);
#endif

            length--;
            if (! length) break;
          }
        }
        Serial.println(F("\n****"));
        fona.HTTP_POST_end();
      }
    /*****************************************/

  }

  if (radio.receiveDone())
  {
    Serial.print("#[");
    Serial.print(++packetCount);
    Serial.print(']');
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
    if (promiscuousMode)
    {
      Serial.print("to [");Serial.print(radio.TARGETID, DEC);Serial.print("] ");
    }
    for (byte i = 0; i < radio.DATALEN; i++)
    
      Serial.print((char)radio.DATA[i]);
      //Serial.print(" ");
      //Serial.print((char)radio.DATA[i]);
    

    //for (byte i = 0; i < radio.DATALEN; i++)
    //Serial.print((char)radio.DATA[i]);
    //Serial.print("   [RX_RSSI:");Serial.print(radio.RSSI);Serial.print("]");
    
    if (radio.ACKRequested())
    {
      flag = true;
      byte theNodeID = radio.SENDERID;
      radio.sendACK();
      Serial.print(" - ACK sent.");

      // When a node requests an ACK, respond to the ACK
      // and also send a packet requesting an ACK (every 3rd one only)
      // This way both TX/RX NODE functions are tested on 1 end at the GATEWAY
      if (ackCount++%3==0)
      {
        Serial.print(" Pinging node ");
        Serial.print(theNodeID);
        Serial.print(" - ACK...");
        delay(3); //need this when sending right after reception .. ?
        if (radio.sendWithRetry(theNodeID, "ACK TEST", 8, 0))  // 0 = only 1 attempt, no retries
          Serial.print("ok!");
        else Serial.print("nothing");
      }
    }
    Serial.println();
    Blink(LED,3);
  }
  counter ++;
  time2 = micros();
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

