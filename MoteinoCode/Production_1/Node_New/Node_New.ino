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
#define NODEID        02    //must be unique for each node on same network (range up to 254, 255 is used for broadcast)
#define NETWORKID     100  //the same on all nodes that talk to each other (range up to 255)
#define GATEWAYID     00
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
  #define LTC1867L_SS   A3 // Pin for chip select of ADC
#endif

#define SERIAL_BAUD     115200

#define SAMPLE_FREQ     10 //(in milliseconds) max of about 1 second
#define SAMPLE_TIME     0.1 //(in seconds)
#define NUM_SAMPLES     ( (1000 / SAMPLE_FREQ) * SAMPLE_TIME )
int samples_taken = 0;
uint32_t timer_sub = round(SAMPLE_FREQ * 62.5) - 1;

int TRANSMITPERIOD = 10; //transmit a packet to gateway so often (in ms)
char payload[] = "1234";
char buff[20];
byte sendSize=0;
boolean requestACK = false;
SPIFlash flash(FLASH_SS, 0xEF40); //EF40 for 8mbit  Windbond chip (W25X40CL)
bool promiscuousMode = true; //set to 'true' to sniff all packets on the same network
uint32_t addr = 0000;
char HUB_ID[] = "00";

unsigned long now;
unsigned long time1;
unsigned long time2;
signed long diff;
int switch_flag = 0;
int timer_rdy = 0;

uint8_t _TCCR1A;
uint8_t _TCCR1B;
uint8_t _ICR1;
uint8_t _TIMSK1;

////FLASH Blocks
//uint32_t BLOCK0 = 0x000000;
//uint32_t BLOCK1 = 0x010000;
//uint32_t BLOCK2 = 0x020000;
//uint32_t BLOCK3 = 0x030000;
//uint32_t BLOCK4 = 0x040000;
//uint32_t BLOCK5 = 0x050000;
//uint32_t BLOCK6 = 0x060000;
//uint32_t BLOCK7 = 0x070000;
//uint32_t BLOCK8 = 0x080000;
//uint32_t BLOCK9 = 0x090000;
//uint32_t BLOCK10 = 0x0A0000;
//uint32_t BLOCK11 = 0x0B0000;
//uint32_t BLOCK12 = 0x0C0000;
//uint32_t BLOCK13 = 0x0D0000;
//uint32_t BLOCK14 = 0x0E0000;
//uint32_t BLOCK15 = 0x0F0000;
//
//uint32_t *BLOCKS[16] = {&BLOCK0,&BLOCK1,&BLOCK2,&BLOCK3,
//                        &BLOCK4,&BLOCK5,&BLOCK6,&BLOCK7,
//                        &BLOCK8,&BLOCK9,&BLOCK10,&BLOCK11,
//                        &BLOCK12,&BLOCK13,&BLOCK14,&BLOCK15};
//
//uint16_t block0pos = 0;
//uint16_t block1pos = 0;
//uint16_t block2pos = 0;
//uint16_t block3pos = 0;
//uint16_t block4pos = 0;
//uint16_t block5pos = 0;
//uint16_t block6pos = 0;
//uint16_t block7pos = 0;
//uint16_t block8pos = 0;
//uint16_t block9pos = 0;
//uint16_t block10pos = 0;
//uint16_t block11pos = 0;
//uint16_t block12pos = 0;
//uint16_t block13pos = 0;
//uint16_t block14pos = 0;
//uint16_t block15pos = 0;
//
////Pos 0 in array is 0th block pos
//uint16_t blockPos[16] = {&block0pos,&block1pos,&block2pos,&block3pos,
//                          &block4pos,&block5pos,&block6pos,&block7pos,
//                          &block8pos,&block9pos,&block10pos,&block11pos,
//                          &block12pos,&block13pos,&block14pos,&block15pos};
//

//Address of flash blocks
const uint32_t BLOCKS[16] = {0x000000, 0x010000, 0x020000, 0x030000,
                             0x040000, 0x050000, 0x060000, 0x070000,
                             0x080000, 0x090000, 0x0A0000, 0x0B0000,
                             0x0C0000, 0x0D0000, 0x0E0000, 0x0F0000};

const uint16_t ChNStartPos[8] = {0x0000, 0x2000, 0x4000, 0x6000,
                                 0x8000, 0xA000, 0xC000, 0xE000};

uint16_t ChNCurrPos[8] = {0, 0, 0, 0, 0, 0, 0, 0};

uint16_t currBlockMax = 0;

#define CH0  0
//#define CH1  1
//#define CH2  2
//#define CH3  3
//#define CH4  4
//#define CH5  5
//#define CH6  6
//#define CH7  7


#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif

static uint8_t uni_bi_polar = LTC1867_UNIPOLAR_MODE;    //!< The LTC1867 unipolar/bipolar mode selection
static float LTC1867_lsb = 6.25009537E-5;               //!< Ideal LSB voltage for a perfect part
static int32_t LTC1867_offset_unipolar_code = 0;        //!< Ideal unipolar offset for a perfect part
static int32_t LTC1867_offset_bipolar_code = 0;         //!< Ideal bipolar offset for a perfect part
uint16_t ADCcode[8] = {0,0,0,0,0,0,0,0};

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
  radio.promiscuous(promiscuousMode);
  //radio.setFrequency(919000000); //set frequency to some custom frequency
  
//Auto Transmission Control - dials down transmit power to save battery (-100 is the noise floor, -90 is still pretty good)
//For indoor nodes that are pretty static and at pretty stable temperatures (like a MotionMote) -90dBm is quite safe
//For more variable nodes that can expect to move or experience larger temp drifts a lower margin like -70 to -80 would probably be better
//Always test your ATC mote in the edge cases in your own environment to ensure ATC will perform as you expect
#ifdef ENABLE_ATC
  radio.enableAutoPower(-70);
#endif
  
  //char buff[50];
  //sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  //Serial.println(buff);
  Serial.println("Node radio init");
  
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

  quikeval_SPI_init();
  pinMode(LTC1867L_SS, OUTPUT);
}

long lastPeriod = 0;
int voltage = 0;
float adc_voltage;
uint8_t user_command;
uint8_t adc_command;                             // The LTC1867 command byte
uint16_t adc_code = 0;                           // The LTC1867 code
int i,j,count,counter = 0;
char input;
int hello = 0;
int cmd;

//For conversion
char BTC_array[3];
byte CTB;
char zero[] = "0";

void loop() 
{

  //Blink(LED,3);  

//  time1 = micros();
//  time2 = micros();
//  Serial.println(time1);
//  Serial.println(time2);

  //if (timer_rdy = 1)
//  if (count = 1)
//  {
//    count = 0;
//    timer_rdy = 0;
//    for (i = 0; i <= 7; i++)
//    {
//    adc_command = BUILD_COMMAND_SINGLE_ENDED[0] | uni_bi_polar;   // Build ADC command for channel 0
//    LTC1867_read(LTC1867L_SS, adc_command, &ADCcode[i]);             // Throws out last reading
//    LTC1867_read(LTC1867L_SS, adc_command, &ADCcode[i]);             // Takes reading
//    //read ADC
//    }
//    char val[4];
//    sprintf(val, "%x", ADCcode);
//    Serial.println(val);
//    flash.writeBytes(BLOCK0, val, strlen(val));
//  }
  if (Serial.available() > 0)
  {
    input = Serial.read();
    if (input == 'f')
    {
      time1 = micros();
      count = 0;
      timer_rdy = 0;
      for (i = 0; i <= 7; i++)
      {
        adc_command = BUILD_COMMAND_SINGLE_ENDED[0] | uni_bi_polar;   // Build ADC command for channel 0
        LTC1867_read(LTC1867L_SS, adc_command, &ADCcode[i]);             // Throws out last reading
        LTC1867_read(LTC1867L_SS, adc_command, &ADCcode[i]);             // Takes reading
        //read ADC
      }
    
      writeADCtoFlash();
      time2 = micros();
      Serial.print("delay = ");
      Serial.println(time2 - time1);
    }
    
    if (input == 'd') //d=dump flash area
    {
      Serial.println("Flash content:");
      int counter = 0;
      //Serial.print("0-255: ");
      for (i = 0; i < 8; i++)
      {
        counter = 0;
        Serial.println();
        Serial.print("Next sector: ");
        Serial.println(ChNStartPos[i]);
        while(counter<=((NUM_SAMPLES*2))){
          Serial.print(flash.readByte(ChNStartPos[i] + counter), HEX);
          Serial.print('.');
          counter++;
        }
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

    if (input == 'a')
    {
      readADC();
      writeADCtoFlash();
      uint16_t code1;
      for (i = 0; i < 8; i++)
      {
        code1 = ADCcode[i];
        Serial.print("ADC CH");
        Serial.print(i);
        Serial.println(code1);
      }
    }
  }


  
  //check for any received packets
  if (radio.receiveDone())
  {
    Serial.println("Packet");
    Serial.println(idParser());
    Serial.println(cmdParser());
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
    for (byte i = 0; i < radio.DATALEN; i++)
      Serial.print((char)radio.DATA[i]);
    Serial.print("   [RX_RSSI:");Serial.print(radio.RSSI);Serial.print("]");

    Serial.println();

    if (radio.ACKRequested())
    {
      //byte theNodeID = radio.SENDERID;
      radio.sendACK();
      Serial.println(" CMD 2 - ACK sent.");
    }

    int id = idParser();
    if (id == 99 || id == NODEID)
    {
      Serial.println("Initiate cmd");

      cmd = cmdParser();
      if (cmd == 0)
      {
        char cmd_char[10];
        itoa(cmd, cmd_char, 10);
        char sending[50] = " ";
        char ID[2] = {' ', ' '};
        itoa(NODEID, ID, 10);
        
        //cmd 0 Send back packet immediately (Delay computation)
        if (strlen(ID) == 1)
        {
          //Serial.println("here");
          strcpy(sending,zero);
          strcat(sending,ID);
          strcat(sending,cmd_char);
          Serial.println(cmd_char);
        }
        else if (strlen(ID) == 2)
        {
          strcpy(sending,ID);
          strcat(sending,cmd_char);
        }
        strcat(sending," DELAY RECEIVED");
        Serial.println(sending);
        Serial.println(strlen(sending));
        radio.send(GATEWAYID, sending, strlen(sending));
      }

      if (cmd == 1)
      {
        //block0pos = 47288;
        //cmd 1 Send readystatus
        Serial.println(ChNCurrPos[0]);
        ChNCurrPos[0] = ChNCurrPos[0] + 1;

        Serial.println(ChNCurrPos[0]);
        ChNCurrPos[0] = ChNCurrPos[0] + 1;
      }

      if (cmd == 2)
      {
        //currBlockMax = ChNCurrPos[0];
        //block0pos = 0;
        //cmd 2 Send data
        //resetFlashAddr();
        char cmd_char[10];
        itoa(cmd, cmd_char, 10);
        char sending[50] = " ";
        char ID[2] = {' ', ' '};
        itoa(NODEID, ID, 10);

        char space[2] = {' ', '\0'};
        char code[] = " ";
        uint32_t p;
        uint32_t p1;
        byte d;
        char b;
        char b1[3] = {' ', ' ', '\0'};

        
        for (i = 0; i < 8; i++)
        {
          //Loop through all channels (0-7)
          currBlockMax = ChNCurrPos[i];
          ChNCurrPos[i] = 0;
          Serial.print("CurrCH: ");
          Serial.println(i);
          Serial.print("CurrBlockMax: ");
          Serial.println(currBlockMax);
          while (currBlockMax > ChNCurrPos[i])
          {
            Serial.println();
            Serial.println();
            counter++;
            Serial.print("Counter: ");
            Serial.println(counter);
            if (strlen(ID) == 1)
            {
              Serial.println("here");
              strcpy(sending,zero);
              strcat(sending,ID);
              strcat(sending,cmd_char);
              Serial.println(cmd_char);
            }
            else if (strlen(ID) == 2)
            {
              strcpy(sending,ID);
              strcat(sending,cmd_char);
            }

            Serial.print("Sending size: ");
            Serial.println(strlen(sending));
            

            for (j = 0; j < 8; j++)
            {
              if (currBlockMax <= ChNCurrPos[i])
                break;
              //Send 8 readings
              strcat(sending," ");
              
              //Serial.print("Sending size: ");
              //Serial.println(strlen(sending));
              
              //Serial.print("Pos within Block: ");
              //Serial.println(ChNCurrPos[0]);
              
              p = BLOCKS[0] + ChNStartPos[i]+ ChNCurrPos[i];
              d = flash.readByte(p);
              byteToChar(d);
              ChNCurrPos[i] = ChNCurrPos[i] + 1;
              strcat(sending,BTC_array);
              
              p = BLOCKS[0] + ChNStartPos[i]+ ChNCurrPos[i];
              d = flash.readByte(p);
              byteToChar(d);
              ChNCurrPos[i] = ChNCurrPos[i] + 1;
              strcat(sending,BTC_array);
              
              Serial.print("Read flash: ");
              Serial.println(BTC_array);

              Serial.print("Pos: ");
              Serial.println(p);
              Serial.print("Block Start Pos: ");
              Serial.println(ChNStartPos[i]);
              Serial.print("Pos within Block: ");
              Serial.println(ChNCurrPos[i]);

              

  
              //Serial.print("Sending size: ");
              //Serial.println(strlen(sending));
            }
    
            //Serial.flush();
            Serial.print("Sending: ");
            Serial.println(sending);
            Serial.print("Sending size: ");
            Serial.println(strlen(sending));
            Serial.print("ChNCurrPos: ");
            Serial.println(ChNCurrPos[i]);
            Serial.flush();
    
            int retry_count = 0;

            delay(10);
            radio.send(GATEWAYID, sending, strlen(sending));
    
    //        while(!radio.sendWithRetry(GATEWAYID, sending, strlen(sending), 5, 100))
    //        {
    //          retry_count++;
    //          if (retry_count > 3)
    //          {
    //            Serial.println("Error: Can't communicate(Send DATA)");
    //            break;
    //          }
    //        }
    
    //        while(1)
    //        {
    //          //Wait for next cmd
    //          if (radio.receiveDone());
    //        }
          }
  
          //Send change sensor CMD
          //Wait for response
          //Then continue
          if (strlen(ID) == 1)
          {
            //Serial.println("here");
            //char zero[] = "0";
            strcpy(sending,zero);
            strcat(sending,ID);
            strcat(sending,cmd_char);
            Serial.println(cmd_char);
          }
          else if (strlen(ID) == 2)
          {
            strcpy(sending,ID);
            strcat(sending,cmd_char);
          }
          strcat(sending," CHG SENSOR");
  
          delay(10);
          radio.send(GATEWAYID, sending, strlen(sending));
          //radio.send(GATEWAYID, sending, strlen(sending)); // Need second DONE command?????
          Serial.print("Sent CHG SENSOR:");
          Serial.println(sending);
          Serial.println(strlen(sending));

          now = micros();
          while(1)
          {
            time1 = micros();
            if (radio.receiveDone())
            {

              if ((char)radio.DATA[radio.DATALEN - 12] == 'C' && 
                  (char)radio.DATA[radio.DATALEN - 11] == 'H' && 
                  (char)radio.DATA[radio.DATALEN - 10] == 'G' &&
                  (char)radio.DATA[radio.DATALEN - 9 ] == ' ' &&
                  (char)radio.DATA[radio.DATALEN - 8 ] == 'R' && 
                  (char)radio.DATA[radio.DATALEN - 7 ] == 'E' && 
                  (char)radio.DATA[radio.DATALEN - 6 ] == 'C' &&
                  (char)radio.DATA[radio.DATALEN - 5 ] == 'E' &&
                  (char)radio.DATA[radio.DATALEN - 4 ] == 'I' && 
                  (char)radio.DATA[radio.DATALEN - 3 ] == 'V' &&
                  (char)radio.DATA[radio.DATALEN - 2 ] == 'E' &&
                  (char)radio.DATA[radio.DATALEN - 1 ] == 'D')
              {
                Serial.println("Received chg done");
                break;      
              }

            }
            if ( time1 - now > 500000)
            {
              radio.send(GATEWAYID, sending, strlen(sending));
              Serial.println("Resent CHG SENSOR");
              now = micros();
            }
          }
          
        }
        
        //Done with all data, send completion command

        if (strlen(ID) == 1)
        {
          //Serial.println("here");
          //char zero[] = "0";
          strcpy(sending,zero);
          strcat(sending,ID);
          strcat(sending,cmd_char);
          Serial.println(cmd_char);
        }
        else if (strlen(ID) == 2)
        {
          strcpy(sending,ID);
          strcat(sending,cmd_char);
        }

        strcat(sending," DONE");
        delay(10);
        radio.send(GATEWAYID, sending, strlen(sending));
        radio.send(GATEWAYID, sending, strlen(sending)); // Need second DONE command?????
        Serial.print("Sent DONE:");
        Serial.println(sending);
        Serial.println(strlen(sending));
      }

      Serial.println("After 2");


      if (cmd == 3)
      {
          
        Serial.print("Erasing Flash chip ... ");
        flash.chipErase();
        while(flash.busy());
        Serial.println("DONE");
        
        //cmd3 Start data capture
        setTimer1();
        resetFlashAddr();
        Serial.print("NUM_SAMPLES: ");
        Serial.println(NUM_SAMPLES);
        while (samples_taken < NUM_SAMPLES)
        {
          if (timer_rdy)
          {
            Serial.print("time diff: ");
            Serial.println(diff);
            
            Serial.print("samples_taken: ");
            Serial.println(samples_taken);
            timer_rdy = 0;     
               
            readADC();
            //adc_command, &ADCcode[i]
            Serial.println("Read Complete");
            writeADCtoFlash();
            Serial.println("Write Complete");
            Serial.flush();
            samples_taken ++;
          }
          Serial.println("Im stuck");
        }
        Serial.println("Unset timer");
        unsetTimer1();
        samples_taken = 0;        
      }
      Serial.println("End Receive");
      Serial.flush();
      
    }

//    if (radio.ACKRequested())
//    {
//      radio.sendACK();
//      Serial.print(" - ACK sent");
//    }
//    Blink(LED,3);
//    Serial.println();
    Serial.println("Crash before here?");
    Serial.flush();
    hello = 1;
  }
  //Serial.println("loop");
  //Serial.flush();

  if (hello == 1)
  {
    Serial.println("Outside Receive");
    Serial.flush();
    hello = 0;
  }


}

ISR (TIMER1_OVF_vect)
{
  TCNT1 = 65535 - timer_sub;
  timer_rdy = 1;
  // action to be done every 10ms
  if (switch_flag == 1)
  {
    time1 = micros();
    diff = time1 - time2;
    switch_flag = 0;
  }
  else if (switch_flag == 0)
  {
    time2 = micros();
    diff = time2 - time1;
    switch_flag = 1;
  }
  //Serial.println(diff);
}


void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}

void readADC()
{
  for (i = 0; i < 8; i++)
  {
    adc_command = BUILD_COMMAND_SINGLE_ENDED[i] | uni_bi_polar;   // Build ADC command for channel i
    LTC1867_read(LTC1867L_SS, adc_command, &ADCcode[i]);             // Throws out last reading
    LTC1867_read(LTC1867L_SS, adc_command, &ADCcode[i]);             // Takes reading
  }
}

void writeADCtoFlash()
{
  char val[3] = {' ', ' ', '\0'};
  byte lo,hi;

  for (i = 0; i < 8; i++)
  {

    lo = ADCcode[i] & 0xFF;
    val[1] = lo;
    hi = ADCcode[i] >> 8;
    val[0] = hi;
    Serial.print("i: ");
    Serial.println(i);

    Serial.print("ADCcode[i]: ");
    Serial.println(ADCcode[i]);

    flash.writeByte(ChNStartPos[i] + ChNCurrPos[i], hi);
    ChNCurrPos[i] = ChNCurrPos[i] + 1;
    flash.writeByte(ChNStartPos[i] + ChNCurrPos[i], lo);   
    ChNCurrPos[i] = ChNCurrPos[i] + 1;
    Serial.print("Pos: ");
    Serial.println(ChNStartPos[i] + ChNCurrPos[i]);
    //block0pos = block0pos + strlen(val);
    //ChNCurrPos[i] = ChNCurrPos[i] + strlen(val);
    //Serial.print("ADCcode: ");
    //Serial.println(ADCcode[i]);
    //Serial.println(block0pos);
    Serial.print("Hig: ");
    Serial.println(hi);
    Serial.print("Low: ");
    Serial.println(lo);
    //Serial.println(ADCcode[CH0]);
  }
}


void resetFlashAddr()
{
  for (int i = 0; i < 7; i++)
    ChNCurrPos[i] = 0;
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

void setTimer1()
{
  cli();         // disable global interrupts

  _TCCR1A = TCCR1A;
  _TCCR1B = TCCR1B;
  _ICR1 = ICR1;
  _TIMSK1 = TIMSK1;
  
  TCCR1A = 0;    // set entire TCCR1A register to 0
  TCCR1B = 0;    // set entire TCCR1B register to 0 
  TCNT1 = 65535 - timer_sub;

  TIMSK1 |= (1 << TOIE1);
  //Set interrupt on compare match

  TCCR1B |= (1 << CS12);// set prescaler to 256 and starts the timer

  sei(); // enable interrupts
}

void unsetTimer1()
{
  TCCR1A = _TCCR1A;
  TCCR1B = _TCCR1B;
  ICR1 = _ICR1;
  TIMSK1 = _TIMSK1;
}

//Returns value of ID in command string
int idParser()
{
  //ID is first two bytes in DATA array
  char ID[] = {(char)radio.DATA[0],(char)radio.DATA[1]};
  return atoi(ID);
}

int cmdParser()
{
  //CMD is third byte in DATA array
  char cmd[] = {(char)radio.DATA[2]};
  return atoi(cmd);
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

void byteToChar(byte b)
{
  uint8_t nibbles[2];
  nibbles[0] = b >> 4; //high nibble
  nibbles[1] = b & 0xF; //low nibble

  for (int btc = 0; btc < 2; btc++)
  {
    switch (nibbles[btc])
    {
      case 0x0:
        BTC_array[btc] = '0';
        break;

      case 0x1:
        BTC_array[btc] = '1';
        break;    
        
      case 0x2:
        BTC_array[btc] = '2';
        break;

      case 0x3:
        BTC_array[btc] = '3';
        break;
        
      case 0x4:
        BTC_array[btc] = '4';
        break;

      case 0x5:
        BTC_array[btc] = '5';
        break;

      case 0x6:
        BTC_array[btc] = '6';
        break;

      case 0x7:
        BTC_array[btc] = '7';
        break;

       case 0x8:
        BTC_array[btc] = '8';
        break;

      case 0x9:
        BTC_array[btc] = '9';
        break;      
        
      case 0xA:
        BTC_array[btc] = 'A';
        break;

      case 0xB:
        BTC_array[btc] = 'B';
        break;
        
      case 0xC:
        BTC_array[btc] = 'C';
        break;

      case 0xD:
        BTC_array[btc] = 'D';
        break;

      case 0xE:
        BTC_array[btc] = 'E';
        break;

      case 0xF:
        BTC_array[btc] = 'F';
        break;

      default:
        Serial.println("BTC ERROR");
        break;
    }
  }
  
  BTC_array[2] = '\0';
}

void charToByte(char c, char c1)
{
  char array[2] = {c, c1};
  byte lo,hi;
  byte CTB_array[2];

  for (int ctb = 0; ctb < 2; ctb++)
  {
    switch (array[ctb])
    {
      case '0':
        CTB_array[ctb] = 0x0;
        break;

      case '1':
        CTB_array[ctb] = 0x1;
        break; 
        
      case '2':
        CTB_array[ctb] = 0x2;
        break;

      case '3':
        CTB_array[ctb] = 0x3;
        break;
          
      case '4':
        CTB_array[ctb] = 0x4;
        break;

      case '5':
        CTB_array[ctb] = 0x5;
        break; 
         
      case '6':
        CTB_array[ctb] = 0x6;
        break;

      case '7':
        CTB_array[ctb] = 0x7;
        break;  
        
      case '8':
        CTB_array[ctb] = 0x8;
        break;
        
      case '9':
        CTB_array[ctb] = 0x9;
        break; 
         
      case 'A':
        CTB_array[ctb] = 0xA;
        break;

      case 'B':
        CTB_array[ctb] = 0xB;
        break; 
         
      case 'C':
        CTB_array[ctb] = 0xC;
        break;

      case 'D':
        CTB_array[ctb] = 0xD;
        break; 
        
      case 'E':
        CTB_array[ctb] = 0xE;
        break;

      case 'F':
        CTB_array[ctb] = 0xF;
        break;
              
      default:
        Serial.println("BTC ERROR");
        break;
    }
  }

  CTB = (CTB_array[0] << 4) || (CTB_array[1]);
}

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}














