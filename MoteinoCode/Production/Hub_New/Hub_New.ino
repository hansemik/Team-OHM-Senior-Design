// Sample RFM69 receiver/gateway sketch, with ACK and optional encryption, and Automatic Transmission Control
// Passes through any wireless received messages to the serial port & responds to ACKs
// It also looks for an onboard FLASH chip, if present
// RFM69 library and sample code by Felix Rusu - http://LowPowerLab.com/contact
// Copyright Felix Rusu (2015)

#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
//#include <RFM69_ATC.h>//get it here: https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>      //comes with Arduino IDE (www.arduino.cc)
#include <SPIFlash.h> //get it here: https://www.github.com/lowpowerlab/spiflash

//*********************************************************************************************
//************ IMPORTANT SETTINGS - YOU MUST CHANGE/CONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
#define NODEID        01    //unique for each node on same network
#define NETWORKID     100  //the same on all nodes that talk to each other
#define GATEWAYID     00
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
#endif

#define SAMPLE_FREQ     1000 //(in milliseconds) max of about 1 second
#define SAMPLE_TIME     5 //(in seconds)
#define NUM_SAMPLES     ( 1000 / SAMPLE_FREQ * SAMPLE_TIME )
int samples_taken = 0;
uint32_t timer_sub = round(SAMPLE_FREQ * 62.5) - 1;

#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif

unsigned long now = 0;
unsigned long time1 = 0;
unsigned long time2 = 0;
unsigned long time3 = 0;
signed long diff;
int switch_flag = 0;
int timer_rdy = 0;

uint8_t _TCCR1A;
uint8_t _TCCR1B;
uint8_t _ICR1;
uint8_t _TIMSK1;

//Node ID array Node_IDs[0] = 02;
uint8_t Node_IDs[16] = {02,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00};
int num_nodes = 1;
uint8_t commDelay[16] = {00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00};




                        //Address of flash blocks
const uint32_t BLOCKS[16] = {0x000000, 0x010000, 0x020000, 0x030000,
                             0x040000, 0x050000, 0x060000, 0x070000,
                             0x080000, 0x090000, 0x0A0000, 0x0B0000,
                             0x0C0000, 0x0D0000, 0x0E0000, 0x0F0000};



const uint16_t ChNStartPos[8] = {0x0000, 0x2000, 0x4000, 0x6000,
                                 0x8000, 0xA000, 0xC000, 0xE000};

uint16_t ChNCurrPos[8] = {0, 0, 0, 0, 0, 0, 0, 0};

uint16_t currBlockMax = 0;
                                  
SPIFlash flash(FLASH_SS, 0xEF40); //EF30 for 8mbit  Windbond chip (W25X40CL)
bool promiscuousMode = true; //set to 'true' to sniff all packets on the same network

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
  //sprintf(buff, "\nListening at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println("Hub radio init");
  //Serial.println(buff);
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


setTimer1();
}

char zero[] = "0";
int i,j = 0;
byte ackCount=0;
uint32_t packetCount = 0;

//For conversion
char BTC_array[3];
byte CTB;

char input;

void loop() {
  //process any serial input


  if (timer_rdy == 1)
  {
    timer_rdy = 0;
    //Serial.println(diff);
  }
  if (Serial.available() > 0)
  {
    input = Serial.read();
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
      //Serial.print("0-255: ");
      for (i = 0; i < 8; i++)
      {
        counter = 0;
        Serial.println("Next sector");
        while(counter<=600){
          Serial.print(flash.readByte(ChNStartPos[i] + counter), HEX);
          Serial.print('.');
          counter++;
        }
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


    if (input == '0')
    {
      //Get delay
      char cmd[] = "0";
      
      char ID[2] = {' ', ' '};
      itoa(Node_IDs[0], ID, 10);
      Serial.print("Node ID:");
      Serial.println(ID);
      Serial.print("cmd:");
      Serial.println(cmd);
      Serial.println(strlen(ID));
      char sending[50] = " ";
      if (strlen(ID) == 1)
      {
        //Serial.println("here");
        strcpy(sending,zero);
        strcat(sending,ID);
        strcat(sending,cmd);
      }
      else if (strlen(ID) == 2)
      {
        strcpy(sending,ID);
        strcat(sending,cmd);
      }
      strcat(sending, " DELAY");
      Serial.println(sending);
      Serial.println(strlen(sending));
      radio.send(Node_IDs[0], sending, strlen(sending));

      now = micros();
      while(1)
      {
        time3 = micros();
        if (radio.receiveDone())
        {
          time3 = micros();
          if ((char)radio.DATA[radio.DATALEN - 14] == 'D' && 
              (char)radio.DATA[radio.DATALEN - 13] == 'E' &&
              (char)radio.DATA[radio.DATALEN - 12] == 'L' && 
              (char)radio.DATA[radio.DATALEN - 11] == 'A' && 
              (char)radio.DATA[radio.DATALEN - 10] == 'Y' &&
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
            commDelay[0] = time3 - now;
            Serial.println(commDelay[0]);
            Serial.println("Received delay response");
            break;      
          }
        }
        if ( time3 - now > 500000)
        {
          //didnt receive, so resend
          radio.send(Node_IDs[0], sending, strlen(sending));
          Serial.println("Resent CHG SENSOR");
          now = micros();
        }
      }
    }
    if (input == '1')
    {
      resetFlashAddr();

      //Transmit command with ID of 02
      char cmd[] = "1";
      
      char ID[2] = {' ', ' '};
      itoa(Node_IDs[0], ID, 10);
      Serial.print("Node ID:");
      Serial.println(ID);
      Serial.print("cmd:");
      Serial.println(cmd);
      Serial.println(strlen(ID));
      char sending[50] = " ";
      if (strlen(ID) == 1)
      {
        Serial.println("here");
        strcpy(sending,zero);
        strcat(sending,ID);
        strcat(sending,cmd);
      }
      else if (strlen(ID) == 2)
      {
        strcpy(sending,ID);
        strcat(sending,cmd);
      }
      Serial.println(sending);
      Serial.println(strlen(sending));
      radio.send(GATEWAYID, sending, strlen(sending), 0);
      
    }
    

    if (input == '2')
    {
      //cmd2 Get Data
      resetFlashAddr();

      //Transmit command with ID of 02
      char cmd[2] = {'2', '\0'};
      char ID[2] = {' ', ' '};
      itoa(Node_IDs[0], ID, 10);
      Serial.print("Node ID:");
      Serial.println(ID);
      Serial.print("cmd:");
      Serial.println(cmd);
      Serial.println(strlen(ID));
      char sending[50] = " ";
      if (strlen(ID) == 1)
      {
        Serial.println("here");
        strcpy(sending,zero);
        strcat(sending,ID);
        strcat(sending,cmd);
      }
      else if (strlen(ID) == 2)
      {
        strcpy(sending,ID);
        strcat(sending,cmd);
      }
      Serial.println(sending);
      Serial.println(strlen(sending));
      int retry_count = 0;

      //Send Command
//      while(!radio.sendWithRetry(2, sending, strlen(sending), 8, 100))
//      {
//        retry_count++;
//        if (retry_count > 3)
//        {
//          Serial.println("Error: Can't communicate(Send 2)");
//          break;
//        }
//      }

      radio.send(2, sending, strlen(sending));
      //now = micros();
      

      char data[50];
      char write_data[50];
      int wrong_cmd = 0;
      int CH = 0;
      byte b;
      //char done[4];

      //Receive command and data until done
      while(1)
      {
        //if ( (micros() - now) > 100000) //100ms
        //{
          //Havn't seen a command in a while
          
        //}
        
        //Serial.println("Hello?");
        if (radio.receiveDone())
        {
          Serial.print("Data: ");
          for ( int z = 0; z < radio.DATALEN; z++)
            Serial.print((char)radio.DATA[z]);

          Serial.println();
          now = micros();
          if (radio.ACKRequested())
          {
            byte theNodeID = radio.SENDERID;
            Serial.println(theNodeID);
            radio.sendACK();
            Serial.println(" - ACK sent.");
          }

          if (radio.DATALEN == 0)
          {
            Serial.println("DATALEN = 0");
            continue; // ACK packet
          }
         
          if ((char)radio.DATA[radio.DATALEN - 4] == 'D' && 
              (char)radio.DATA[radio.DATALEN - 3] == 'O' && 
              (char)radio.DATA[radio.DATALEN - 2] == 'N' &&
              (char)radio.DATA[radio.DATALEN - 1] == 'E')
          {
            Serial.println("DONE Received");
            break;
          }

          if ((char)radio.DATA[radio.DATALEN - 10] == 'C' && 
              (char)radio.DATA[radio.DATALEN - 9 ] == 'H' && 
              (char)radio.DATA[radio.DATALEN - 8 ] == 'G' &&
              (char)radio.DATA[radio.DATALEN - 7 ] == ' ' &&
              (char)radio.DATA[radio.DATALEN - 6 ] == 'S' && 
              (char)radio.DATA[radio.DATALEN - 5 ] == 'E' && 
              (char)radio.DATA[radio.DATALEN - 4 ] == 'N' &&
              (char)radio.DATA[radio.DATALEN - 3 ] == 'S' &&
              (char)radio.DATA[radio.DATALEN - 2 ] == 'O' && 
              (char)radio.DATA[radio.DATALEN - 1 ] == 'R')
          {
            Serial.println("CHG SENSOR Received");

              
            if (strlen(ID) == 1)
            {
              Serial.println("here");
              strcpy(sending,zero);
              strcat(sending,ID);
              strcat(sending,cmd);
              Serial.println(cmd);
            }
            else if (strlen(ID) == 2)
            {
              Serial.println("there");
              strcpy(sending,ID);
              strcat(sending,cmd);
            }
            Serial.print("cmd: ");
            Serial.println(strlen(cmd));
            strcat(sending, " CHG RECEIVED");
            radio.send(idParser(), sending, strlen(sending));
            Serial.println("CHG RECEIVED SENT");
            Serial.println(sending);
            Serial.flush();

            Serial.print("cmd: ");
            Serial.println(strlen(cmd));
            if (ChNCurrPos[CH] == 0)
            {
              Serial.println("Duplicate cmd");
              Serial.print("ChNCurrPos[CH]: ");
              Serial.println(ChNCurrPos[CH]);
              
              Serial.flush();
              continue;
            }
            Serial.print("cmd: ");
            Serial.println(strlen(cmd));
            CH++;
            Serial.print("cmd: ");
            Serial.println(strlen(cmd));
            Serial.print("ID: ");
            Serial.println(ID);
            //Serial.print("zero: ");
            //Serial.println(zero);
            Serial.println(freeRam());
            Serial.flush();
            continue;
          }

          //Serial.print("Radio datalen - 4: ");
          //Serial.println((char)radio.DATA[radio.DATALEN - 4]);
          
          for (i = 0; i < 50; i++)
          {
            data[i] = '\0';
          }
          Serial.println(idParser());
          Serial.println(atoi(ID));
          Serial.println(cmdParser());
          Serial.println(atoi(cmd));
          if (atoi(ID) == idParser() && atoi(cmd) == cmdParser())
          {
            //Store data
            //Todo: create parser based on spaces, then write to flash
            //int space_pos = valParser(space_pos, radio.DATA); // ID(0), ID(1), CMD(2), Space(3), Val(4)

            //sprintf(data, "%x", (char)radio.DATA);
            //Serial.print("Data: ");
            //Serial.println(data);
            //Serial.println(radio.DATALEN);
            for (j = 0; j < (radio.DATALEN) ; j++)
            {
              data[j] = (char)radio.DATA[j];
            }
            //Serial.print("Data: ");
            //Serial.println(data);
//            for (j = 0; j < strlen(radio.DATA); j++)
//            {
//              Serial.print(radio.DATA[j]);
//            }
            Serial.println();

            j = 0;
            for (i = 4; i < strlen(data); i++)
            {

              if (data[i] == ' ')
              {
                //write_data[i] = data[i];
                continue; 
              }
              else
              {
                //Serial.print("i = ");
                //Serial.println(i);
                //Serial.print("Data[i] = ");
                //Serial.println(data[i]);
                //Serial.print("Absolute Pos: ");
                //Serial.println(BLOCKS[0] + ChNStartPos[0] + ChNCurrPos[0]);
                //Serial.print("ChNCurrPos: ");
                //Serial.println(ChNCurrPos[0]);
                charToByte(data[i], data[i+1]);
                flash.writeByte(BLOCKS[0] + ChNStartPos[CH] + ChNCurrPos[CH], CTB);
                ChNCurrPos[CH] = ChNCurrPos[CH] + 1;
                i++;//Need to increment because two characters get converted into one byte (i.e. "FF" -> 0xFF)
                //write_data[0] = write_data[1] = write_data[2] = write_data[3] = '\0';  
              }
            }
            Serial.print("ChNCurrPos: ");
            Serial.println(ChNCurrPos[CH]);
            //break;
          }
          else
          {
            //Not right command
            Serial.print("WRONG Data: ");
            for ( int z = 0; z < radio.DATALEN; z++)
              Serial.print((char)radio.DATA[z]);

            Serial.println();

            wrong_cmd++;
            if (wrong_cmd > 5)
            {
              Serial.println("Wrong cmd");
              break;//Too many wrong commands
            }
            //break;
          }
          Serial.print("Processing Time: ");
          Serial.println(micros() - now);
        }
      }
    }
    
    if (input == '3')
    {
      //Transmit command with ID of 02
      char cmd[] = "3";
      
      char ID[2] = {' ', ' '};
      itoa(Node_IDs[0], ID, 10);
      Serial.print("Node ID:");
      Serial.println(ID);
      Serial.print("cmd:");
      Serial.println(cmd);
      Serial.println(strlen(ID));
      char sending[50] = " ";
      if (strlen(ID) == 1)
      {
        Serial.println("here");
        strcpy(sending,zero);
        strcat(sending,ID);
        strcat(sending,cmd);
        //sending = '0' + *ID + cmd;
      }
      else if (strlen(ID) == 2)
      {
        //sending = sending + *ID + cmd;
      }
      //char val[5];
      //sprintf(val, "%x", adc_code);
      //char sending[] = {*ID, cmd};
      Serial.println(sending);
    
      //Serial.println(ID);
      //Serial.println(hi,HEX);
      //Serial.println(lo,HEX);
      //Serial.println(val);
      //Serial.println(ADCcode);
      Serial.println(strlen(sending));

      int retry_count = 0;
      while(!radio.sendWithRetry(2, sending, strlen(sending), 8, 100))
      {
        retry_count++;
        if (retry_count > 3)
        {
          Serial.println("Error: Can't communicate");
          break;
        }
      }

    }
    
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
    Serial.print("   [RX_RSSI:");Serial.print(radio.RSSI);Serial.print("]");
    
    if (radio.ACKRequested())
    {
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
}

void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
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
//  for (byte i = 0; i < radio.DATALEN; i++)
//    Serial.print((char)radio.DATA[i]);
//  Serial.print("   [RX_RSSI:");Serial.print(radio.RSSI);Serial.print("]");

  //ID is first two bytes in DATA array
  char ID[] = {(char)radio.DATA[0],(char)radio.DATA[1]};
  return atoi(ID);
}

int cmdParser()
{
  char cmd[] = {(char)radio.DATA[2]};
  return atoi(cmd);
}

int valParser(int start, char* str)
{
  for (int a = start; a < strlen(str); a++)
  {
    if(str[a] == ' ')
    {
      //found space, return pos of char after space
      return a;
    }
    else 
    {
      //Did not find space, so nothing more to parse
      return -1;
    }
  }
}

void resetFlashAddr()
{

  for (int i = 0; i < 7; i++)
    ChNCurrPos[i] = 0;
}

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
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

  CTB = (CTB_array[0] << 4) | (CTB_array[1]);
  Serial.print("CTB");
  Serial.println(CTB,HEX);
}


