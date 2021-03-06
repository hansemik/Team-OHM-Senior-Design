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

#define SERIAL_BAUD   9600

#define AUTOMATED 1

#ifdef __AVR_ATmega1284P__
  #define LED           15 // Moteino MEGAs have LEDs on D15
  #define FLASH_SS      23 // and FLASH SS on D23
#else
  #define LED           9 // Moteinos have LEDs on D9
  #define FLASH_SS      8 // and FLASH SS on D8
  #define FONA_RX       7
  #define FONA_TX       6
  #define FONA_RST      4
#endif

#define SAMPLE_FREQ     10 //(in milliseconds) max of about 1.04 seconds
#define SAMPLE_TIME     1.5 //(in seconds)
#define NUM_SAMPLES     ( (1000 / SAMPLE_FREQ) * SAMPLE_TIME )
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

uint8_t Node_IDs[16] = {02,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00};
int num_nodes = 1;
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

int i,j = 0;
bool bit;
byte reg;

#include <SoftwareSerial.h>
SoftwareSerial fonaSS(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;

void setup() {
  int counter = 0;
  delay(10);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);
  radio.promiscuous(promiscuousMode);
  //radio.setFrequency(919000000); //set frequency to some custom frequency
  char buffer[50];
  if (flash.initialize())
  {
    flash.readUniqueId();
  }
  else
  /////something needs to go here instead of print
    
#ifdef ENABLE_ATC

#endif

  fonaSerial->begin(SERIAL_BAUD);

  DDRD |= 1 << 4;
  PORTD |= 1 << 4;
  delay(10);
  PORTD ^= 1 << 4;
  delay(100);
  PORTD |= 1 << 4;
  delay(100);
  
  sprintf(buffer, "AT\r\n%c", 26);
  for(i = 0; i < 30; i++){
    if(fonaSS.available()){
      fonaSS.flush();
      break;
    }
    fonaSS.write(buffer);
    delay(100);
  }
  fonaSS.flush();
  delay(10);
  sprintf(buffer, "AT\r\n%c", 26);
  fonaSS.write(buffer);
  while(counter < 9){
    if(fonaSS.available()){
      Serial.write(fonaSS.read());
      fonaSS.flush();
      counter++;
    }
  }
  delay(10);
  sprintf(buffer, "ATE0\r\n%c", 26);
  fonaSS.write(buffer);
  counter = 0;
  while(counter < 6){
    if(fonaSS.available()){
      fonaSS.flush();
      counter++;
    }
  }
  delay(100);
  sprintf(buffer, "AT+CVHU=0\r\n%c", 26);
  fonaSS.write(buffer);
  delay(100);
  counter = 0;
  while(counter < 6){
    if(fonaSS.available()){
      fonaSS.flush();
      counter++;
    }
  }
  
  delay(10);

  fonaSS.begin(SERIAL_BAUD);
  delay(10);


setTimer1();
}

char zero[] = "0";

byte ackCount=0;
uint32_t packetCount = 0;

//For conversion
char BTC_array[3];
byte CTB;

uint32_t tenMinCounter = 54000;
uint16_t oneMinCounter = 0;
bool waitOneMin = false;
bool send3G = false;

char input;



void loop() {
  //process any serial input
  input = '\0';
  //Serial.print("d=");
  //Serial.println(diff);
  if (send3G)
  {
    input = '4';
  }

  if(fonaSS.available()){
    fonaSS.flush();
  }

  if (timer_rdy == 1)
  {
    timer_rdy = 0;
   }

#ifdef AUTOMATED
  if (timer_rdy == 1 && send3G == false)
  {
    timer_rdy = 0;

    tenMinCounter++;
    if (tenMinCounter >= 90000)  //Timer set to 10ms, so 60000 is 10 minutes
    {
      input = '3'; //Start data capture
      tenMinCounter = 0;
      waitOneMin = true;
    }

    if(oneMinCounter >= 6000)
    {
      input = '2'; //Start data collection
      oneMinCounter = 0;
      waitOneMin = false;
      send3G = true;
    }
    
  }
#endif

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
    }
    
    if (input == 'd') //d=dump flash area
    {
      int counter = 0;
       for (i = 0; i < 8; i++)
      {
        counter = 0;
        while(counter<=(NUM_SAMPLES*2)){
          counter++;
        }
      }
      while(flash.busy());
    }
    if (input == 'D')
    {
      flash.chipErase();
      while(flash.busy());
    }
    if (input == 'i')
    {
      word jedecid = flash.readDeviceId();
    }
    if (input == 't')
    {
      byte temperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
      byte fTemp = 1.8 * temperature + 32; // 9/5=1.8
    }
   
    if (input == '2')
    {
      //cmd2 Get Data
      resetFlashAddr();

      flash.chipErase();
      while(flash.busy());
      
      //Transmit command with ID of 02
      char cmd[2] = {'2', '\0'};
      char ID[2] = {' ', ' '};
      itoa(Node_IDs[0], ID, 10);
      char sending[50] = " ";
      if (strlen(ID) == 1)
      {
        strcpy(sending,zero);
        strcat(sending,ID);
        strcat(sending,cmd);
      }
      else if (strlen(ID) == 2)
      {
        strcpy(sending,ID);
        strcat(sending,cmd);
      }

      int retry_count = 0;

      radio.send(2, sending, strlen(sending));
      //now = micros();
    
      char data[50];
      char write_data[50];
      int wrong_cmd = 0;
      int CH = 0;
      byte b;
  
      //Receive command and data until done
      while(1)
      {
        
        //Serial.println("Hello?");
        if (radio.receiveDone())
        {
          now = micros();
          if (radio.ACKRequested())
          {
            byte theNodeID = radio.SENDERID;
            radio.sendACK();
          }

          if (radio.DATALEN == 0)
          {
            continue; // ACK packet
          }
         
          if ((char)radio.DATA[radio.DATALEN - 4] == 'D' && 
              (char)radio.DATA[radio.DATALEN - 3] == 'O' && 
              (char)radio.DATA[radio.DATALEN - 2] == 'N' &&
              (char)radio.DATA[radio.DATALEN - 1] == 'E')
          {
            if (strlen(ID) == 1)
            {
              strcpy(sending,zero);
              strcat(sending,ID);
              strcat(sending,cmd);
            }
            else if (strlen(ID) == 2)
            {
              strcpy(sending,ID);
              strcat(sending,cmd);
            }
            strcat(sending, " DONE RECEIVED");
            radio.send(idParser(), sending, strlen(sending));
            
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
              
            if (strlen(ID) == 1)
            {
              strcpy(sending,zero);
              strcat(sending,ID);
              strcat(sending,cmd);
            }
            else if (strlen(ID) == 2)
            {
              strcpy(sending,ID);
              strcat(sending,cmd);
            }
            strcat(sending, " CHG RECEIVED");
            radio.send(idParser(), sending, strlen(sending));

            if (ChNCurrPos[CH] == 0)
            {
              continue;
            }
            CH++;
            continue;
          }
       
          for (i = 0; i < 50; i++)
          {
            data[i] = '\0';
          }
          if (atoi(ID) == idParser() && atoi(cmd) == cmdParser())
          {
            //Store data
            //Todo: create parser based on spaces, then write to flash
 
            for (j = 0; j < (radio.DATALEN) ; j++)
            {
              data[j] = (char)radio.DATA[j];
            }

            j = 0;
            for (i = 4; i < strlen(data); i++)
            {

              if (data[i] == ' ')
              {
                  continue; 
              }
              else
              {

                charToByte(data[i], data[i+1]);
                flash.writeByte(BLOCKS[0] + ChNStartPos[CH] + ChNCurrPos[CH], CTB);
                ChNCurrPos[CH] = ChNCurrPos[CH] + 1;
                i++;//Need to increment because two characters get converted into one byte (i.e. "FF" -> 0xFF)
               }
            }
          }
          else
          {
            wrong_cmd++;
            if (wrong_cmd > 5)
            {
              break;//Too many wrong commands
            }
          }
        }
      }
    }
    
    if (input == '3')
    {
      //Transmit command with ID of 02
      char cmd[] = "3";
      char ID[2] = {' ', ' '};
      itoa(Node_IDs[0], ID, 10);
      char sending[50] = " ";
      
      if (strlen(ID) == 1)
      {
        strcpy(sending,zero);
        strcat(sending,ID);
        strcat(sending,cmd);
        //sending = '0' + *ID + cmd;
      }
      else if (strlen(ID) == 2)
      {
        //sending = sending + *ID + cmd;
      }

      int retry_count = 0;
      while(!radio.sendWithRetry(2, sending, strlen(sending), 8, 100))
      {
        retry_count++;
        if (retry_count > 3)
        {
          break;
        }
      }

    }


    if(input == '4')
    {
      char buffer[760];
      int channel;
      int sample_period = 0;
      uint32_t p;
      byte d;
      char term[2] = {26,'\0'};
      BTC_array[2] = '\0';
      int counter = 0;
      int count = 0;
      int partition = 0;
      sample_period++;
      
      for(i = 0; i < 8; i++){
        channel = i + 1;
        currBlockMax = ChNCurrPos[i];
        ChNCurrPos[i] = 0;
        partition = 0;       
 
        while(ChNCurrPos[i] < currBlockMax)
        {
          //Send data from flash to server
          sprintf(buffer, "AT\r\n%c", 26);
          fonaSS.write(buffer);
          j = 2500;
          while(j > 0){
              if(fonaSS.available()){
                fonaSS.flush();
                j = j+10;
              }
              delay(1);
              j--;
          }
  
          sprintf(buffer, "");
          sprintf(buffer, "AT+CHTTPACT=\"data.sparkfun.com\",80\r\n%c", 26);
          fonaSS.write(buffer);
          count = 0;
          while(count < 22){
            if(fonaSS.available()){
              fonaSS.flush();
              count++;
            }
          }
          fonaSS.flush();
          partition++;
          sprintf(buffer, "GET /input/0lox2pWYros5qzrnXqAA?private_key=D6wpbj7Wdwh5Yvn4yYXX&netid=%d&nodeid=%d&chan=%d&samp=%d&part=%d&val=",NETWORKID, NODEID, channel, sample_period, partition);
          for(j = 0; j < 100; j++){
            p = BLOCKS[0] + ChNStartPos[i]+ ChNCurrPos[i]; 
            d = flash.readByte(p);
            byteToChar(d);
            ChNCurrPos[i] = ChNCurrPos[i] + 1;
            strcat(buffer,BTC_array);
            
            p = BLOCKS[0] + ChNStartPos[i]+ ChNCurrPos[i];
            d = flash.readByte(p);
            byteToChar(d);
            ChNCurrPos[i] = ChNCurrPos[i] + 1;
          
            strcat(buffer,BTC_array);
            counter++;
            strcat(buffer, ".");

            if(ChNCurrPos[i] >= currBlockMax){
              break;
            }
          }
          strcat(buffer, " HTTP/1.1\r\nHost: data.sparkfun.com\r\n\r\n");
          strcat(buffer, term);      
          fonaSS.write(buffer);
          j = 15000;
          while(j > 0){
            if(fonaSS.available()){
              fonaSS.flush();
              j = j + 10;
            }
            delay(1);
            j--;
          }
        }
      }
      send3G = false;
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


#ifdef AUTOMATED
  tenMinCounter++;

  if(waitOneMin)
  {
    oneMinCounter++;
  }
#endif
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

  for (int i = 0; i < 8; i++)
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
        break;
    }
  }

  CTB = (CTB_array[0] << 4) | (CTB_array[1]);

}


