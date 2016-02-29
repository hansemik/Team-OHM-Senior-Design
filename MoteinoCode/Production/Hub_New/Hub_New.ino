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
#define NODEID        00    //unique for each node on same network
#define NETWORKID     100  //the same on all nodes that talk to each other
#define GATEWAYID     1
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

#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif

unsigned long time1;
unsigned long time2;
signed long diff;
int switch_flag = 0;
int timer_rdy = 0;

uint8_t _TCCR1A;
uint8_t _TCCR1B;
uint8_t _ICR1;
uint8_t _TIMSK1;

//Node ID array
int Node_IDs[99];
int num_nodes = 1;


//FLASH Blocks
uint32_t BLOCK0 = 0x000000;
uint32_t BLOCK1 = 0x010000;
uint32_t BLOCK2 = 0x020000;
uint32_t BLOCK3 = 0x030000;
uint32_t BLOCK4 = 0x040000;
uint32_t BLOCK5 = 0x050000;
uint32_t BLOCK6 = 0x060000;
uint32_t BLOCK7 = 0x070000;
uint32_t BLOCK8 = 0x080000;
uint32_t BLOCK9 = 0x090000;
uint32_t BLOCK10 = 0x0A0000;
uint32_t BLOCK11 = 0x0B0000;
uint32_t BLOCK12 = 0x0C0000;
uint32_t BLOCK13 = 0x0D0000;
uint32_t BLOCK14 = 0x0E0000;
uint32_t BLOCK15 = 0x0F0000;

uint32_t *BLOCKS[16] = {&BLOCK0,&BLOCK1,&BLOCK2,&BLOCK3,
                        &BLOCK4,&BLOCK5,&BLOCK6,&BLOCK7,
                        &BLOCK8,&BLOCK9,&BLOCK10,&BLOCK11,
                        &BLOCK12,&BLOCK13,&BLOCK14,&BLOCK15};

uint16_t block0pos = 0;
uint16_t block1pos = 0;
uint16_t block2pos = 0;
uint16_t block3pos = 0;
uint16_t block4pos = 0;
uint16_t block5pos = 0;
uint16_t block6pos = 0;
uint16_t block7pos = 0;
uint16_t block8pos = 0;
uint16_t block9pos = 0;
uint16_t block10pos = 0;
uint16_t block11pos = 0;
uint16_t block12pos = 0;
uint16_t block13pos = 0;
uint16_t block14pos = 0;
uint16_t block15pos = 0;


uint16_t *blockPos[16] = {&block0pos,&block1pos,&block2pos,&block3pos,
                          &block4pos,&block5pos,&block6pos,&block7pos,
                          &block8pos,&block9pos,&block10pos,&block11pos,
                          &block12pos,&block13pos,&block14pos,&block15pos};

uint16_t currBlockMax = 0;


//Each Sector is 4KB (Each channel gets 8KB)
// 8KB / 2B per ADC read = 4000 reads possible
uint16_t CH0_pos = 0x0000; //Sector 0 & 1
uint16_t CH1_pos = 0x2000; //Sector 2 & 3
uint16_t CH2_pos = 0x4000; //Sector 4 & 5
uint16_t CH3_pos = 0x6000; //Sector 6 & 7
uint16_t CH4_pos = 0x8000; //Sector 8 & 9
uint16_t CH5_pos = 0xA000; //Sector 10 & 11
uint16_t CH6_pos = 0xC000; //Sector 12 & 13
uint16_t CH7_pos = 0xE000; //Sector 14 & 15

uint16_t *CHNpos[16] = {&CH0_pos,&CH1_pos,&CH2_pos,&CH3_pos,
                        &CH4_pos,&CH5_pos,&CH6_pos,&CH7_pos};

SPIFlash flash(FLASH_SS, 0xEF40); //EF30 for 8mbit  Windbond chip (W25X40CL)
bool promiscuousMode = false; //set to 'true' to sniff all packets on the same network

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

Node_IDs[0] = 02;
}


int i,j = 0;
byte ackCount=0;
uint32_t packetCount = 0;
void loop() {
  //process any serial input
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
      Serial.print("0-6000: ");
      while(counter<=6000){
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


    if (input == '0')
    {
      
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
      char sending[] = " ";
      if (strlen(ID) == 1)
      {
        Serial.println("here");
        char zero[] = "0";
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
      char cmd[] = "2";
      
      char ID[2] = {' ', ' '};
      itoa(Node_IDs[0], ID, 10);
      Serial.print("Node ID:");
      Serial.println(ID);
      Serial.print("cmd:");
      Serial.println(cmd);
      Serial.println(strlen(ID));
      char sending[] = " ";
      if (strlen(ID) == 1)
      {
        Serial.println("here");
        char zero[] = "0";
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

      while(1)
      {
        Serial.println("Hello?");
        if (radio.receiveDone())
        {
          Serial.println(idParser());
          Serial.println(atoi(ID));
          Serial.println(cmdParser());
          Serial.println(atoi(cmd));
          if (atoi(ID) == idParser() && atoi(cmd) == cmdParser())
          {
            //Store data
            //Todo: create parser based on spaces, then write to flash
            //int space_pos = valParser(space_pos, radio.DATA); // ID(0), ID(1), CMD(2), Space(3), Val(4)
            char data[radio.DATALEN];
            //sprintf(data, "%x", (char)radio.DATA);
            Serial.print("Data: ");
            Serial.println(data);
            for (j = 0; j < radio.DATALEN; j++)
            {
              data[j] = (char)radio.DATA[j];
            }
            Serial.print("Data: ");
            Serial.println(data);
//            for (j = 0; j < strlen(radio.DATA); j++)
//            {
//              Serial.print(radio.DATA[j]);
//            }
            Serial.println();
            char write_data[4];
            j = 0;
            for (i = 4; i < strlen(data); i++)
            {
              if (radio.DATA[i] != ' ')
              {
                write_data[j] = data[i];
                j++; 
              }
              else
              {
                j = 0;
                flash.writeBytes(*BLOCKS[0] + CH0_pos, write_data, strlen(write_data));
                CH0_pos = CH0_pos + strlen(write_data);
                write_data[0] = write_data[1] = write_data[2] = write_data[3] = '\0';                
              }
            }
            break;
          }
          else
          {
            //Not right command
            break;
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
      Serial.print("Node ID:");
      Serial.println(ID);
      Serial.print("cmd:");
      Serial.println(cmd);
      Serial.println(strlen(ID));
      char sending[] = " ";
      if (strlen(ID) == 1)
      {
        Serial.println("here");
        char zero[] = "0";
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
      radio.send(GATEWAYID, sending, strlen(sending), 0);

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
  TCNT1 = 64911;
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
  TCNT1 = 64911;

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
  block0pos = 0;
  block1pos = 0;
  block2pos = 0;
  block3pos = 0;
  block4pos = 0;
  block5pos = 0;
  block6pos = 0;
  block7pos = 0;
  block8pos = 0;
  block9pos = 0;
  block10pos = 0;
  block11pos = 0;
  block12pos = 0;
  block13pos = 0;
  block14pos = 0;
  block15pos = 0;
}

