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
#define NODEID        02   //must be unique for each node on same network (range up to 254, 255 is used for broadcast)
#define NETWORKID     100  //the same on all nodes that talk to each other (range up to 255)
#define GATEWAYID     01
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

#define SERIAL_BAUD     9600

#define SAMPLE_FREQ     10 //(in milliseconds) max of about 1 second
#define SAMPLE_TIME     15 //(in seconds)
#define NUM_SAMPLES     ( (1000 / SAMPLE_FREQ) * SAMPLE_TIME )
int samples_taken = 0;
uint32_t timer_sub = round(SAMPLE_FREQ * 62.5) - 1;

int TRANSMITPERIOD = 10; //transmit a packet to gateway so often (in ms)
char payload[] = "1234";
char buff[20];
byte sendSize = 0;
boolean requestACK = false;
SPIFlash flash(FLASH_SS, 0xEF40); //EF40 for 8mbit  Windbond chip (W25X40CL)
bool promiscuousMode = false; //set to 'true' to sniff all packets on the same network
uint32_t addr = 0000;
char HUB_ID[] = "00";

unsigned long now;
unsigned long time1;
unsigned long time2;
signed long diff;
int switch_flag = 0;
int timer_rdy = 0;

unsigned long timer2_counter = 0;
static unsigned long timer2_fract = 0;
bool timer2_counter_flag = false;

int retry_count = 0;

uint8_t _TCCR1A;
uint8_t _TCCR1B;
uint8_t _ICR1;
uint8_t _TIMSK1;

uint8_t _TCCR2A;
uint8_t _TCCR2B;
uint8_t _TIMSK2;

//Address of flash blocks
const uint32_t BLOCKS[16] = {0x000000, 0x010000, 0x020000, 0x030000,
                             0x040000, 0x050000, 0x060000, 0x070000,
                             0x080000, 0x090000, 0x0A0000, 0x0B0000,
                             0x0C0000, 0x0D0000, 0x0E0000, 0x0F0000
                            };

const uint16_t ChNStartPos[8] = {0x0000, 0x2000, 0x4000, 0x6000,
                                 0x8000, 0xA000, 0xC000, 0xE000
                                };

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
uint16_t ADCcode[8] = {0, 0, 0, 0, 0, 0, 0, 0};

//! Lookup table to build the command for single-ended mode, input with respect to GND
const uint8_t BUILD_COMMAND_SINGLE_ENDED[8] = {LTC1867_CH0, LTC1867_CH1, LTC1867_CH2, LTC1867_CH3,
                                               LTC1867_CH4, LTC1867_CH5, LTC1867_CH6, LTC1867_CH7
                                              }; //!< Builds the command for single-ended mode, input with respect to GND

void setup() {

  radio.initialize(FREQUENCY, NODEID, NETWORKID);
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

  if (flash.initialize())
  {
    flash.readUniqueId();
  }

  quikeval_SPI_init();
  DDRC = B11111111;
  DDRB |= 1 << 1;
  setTimer2();
}

long lastPeriod = 0;
int voltage = 0;
float adc_voltage;
uint8_t user_command;
uint8_t adc_command;                             // The LTC1867 command byte
uint16_t adc_code = 0;                           // The LTC1867 code
int i, j, count, counter = 0;
char input;
int hello = 0;
int cmd;

//For conversion
char BTC_array[3];
byte CTB;
char zero[] = "0";

void loop()
{
  //check for any received packets
  if (radio.receiveDone())
  {
    if (radio.ACKRequested())
    {
      radio.sendACK();
    }

    int id = 6;
    id = idParser();
    if (id == 99 || id == NODEID)
    {

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
          strcpy(sending, zero);
          strcat(sending, ID);
          strcat(sending, cmd_char);
        }
        else if (strlen(ID) == 2)
        {
          strcpy(sending, ID);
          strcat(sending, cmd_char);
        }
        strcat(sending, " DELAY RECEIVED");
        radio.send(GATEWAYID, sending, strlen(sending));
      }

      if (cmd == 2)
      {
        char cmd_char[10];
        itoa(cmd, cmd_char, 10);
        char sending[50] = " ";
        char ID[2] = {' ', ' '};
        itoa(NODEID, ID, 10);
        int retry_count;

        char space[2] = {' ', '\0'};
        char code[] = " ";
        uint32_t p;
        uint32_t p1;
        byte d;
        char b;
        char b1[3] = {' ', ' ', '\0'};


        for (i = 0; i < 1; i++)
        {
          //Loop through all channels (0-7)
          currBlockMax = ChNCurrPos[i];
          ChNCurrPos[i] = 0;
          while (currBlockMax > ChNCurrPos[i])
          {
            counter++;
            if((ChNCurrPos[i] % 10) == 0){
              PORTB ^= 1 << 1;
            }
            if (strlen(ID) == 1)
            {
              strcpy(sending, zero);
              strcat(sending, ID);
              strcat(sending, cmd_char);
            }
            else if (strlen(ID) == 2)
            {
              strcpy(sending, ID);
              strcat(sending, cmd_char);
            }

            for (j = 0; j < 8; j++)
            {
              if (currBlockMax <= ChNCurrPos[i])
                break;
              //Send 8 readings
              strcat(sending, " ");

              p = BLOCKS[0] + ChNStartPos[i] + ChNCurrPos[i];
              d = flash.readByte(p);
              byteToChar(d);
              ChNCurrPos[i] = ChNCurrPos[i] + 1;
              strcat(sending, BTC_array);

              p = BLOCKS[0] + ChNStartPos[i] + ChNCurrPos[i];
              d = flash.readByte(p);
              byteToChar(d);
              ChNCurrPos[i] = ChNCurrPos[i] + 1;
              strcat(sending, BTC_array);
            }

            //wait_ms(10);
            //radio.send(GATEWAYID, sending, strlen(sending));
            retry_count = 0;
            while(!radio.sendWithRetry(GATEWAYID, sending, strlen(sending), 8, 100))
            {
              retry_count++;
              if (retry_count > 5)
              {
                break;
              }
            }
            PORTB &= ~(1 << 1);
          }

          //Send change sensor CMD
          //Wait for response
          //Then continue
          if (strlen(ID) == 1)
          {
            strcpy(sending, zero);
            strcat(sending, ID);
            strcat(sending, cmd_char);
          }
          else if (strlen(ID) == 2)
          {
            strcpy(sending, ID);
            strcat(sending, cmd_char);
          }
          strcat(sending, " CHG SENSOR");

          wait_ms(15);
          radio.send(GATEWAYID, sending, strlen(sending));


          start_timer2();
          while (1)
          {
            time1 = get_timer2();
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
                break;
              }
            }
            
            if ( time1 > 500)
            {
              radio.send(GATEWAYID, sending, strlen(sending));
              now = micros();
            }
          }
          end_timer2();
        }

        if (strlen(ID) == 1)
        {
          strcpy(sending, zero);
          strcat(sending, ID);
          strcat(sending, cmd_char);
        }
        else if (strlen(ID) == 2)
        {
          strcpy(sending, ID);
          strcat(sending, cmd_char);
        }

        strcat(sending, " DONE");
        wait_ms(15);
        radio.send(GATEWAYID, sending, strlen(sending));
        //radio.send(GATEWAYID, sending, strlen(sending)); // Need second DONE command?????


        start_timer2();
        while (1)
        {
          time1 = get_timer2();
          if (radio.receiveDone())
          {

            if ((char)radio.DATA[radio.DATALEN - 13] == 'D' &&
                (char)radio.DATA[radio.DATALEN - 12] == 'O' &&
                (char)radio.DATA[radio.DATALEN - 11] == 'N' &&
                (char)radio.DATA[radio.DATALEN - 10] == 'E' &&
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
              break;
            }
          }
          if ( time1 > 500)
          {
            radio.send(GATEWAYID, sending, strlen(sending));
            now = micros();
          }
        }
        end_timer2();
      }

      if (cmd == 3)
      {
        
        PORTC |= 1 << 2;
        PORTB |= 1 << 1;
        
        flash.chipErase();
        while (flash.busy());
        //cmd3 Start data capture
        setTimer1();
        resetFlashAddr();
        do
        {
          if (timer_rdy == 1)
          {
            timer_rdy = 0;
            readADC();
            readADC();
            writeADCtoFlash();
            samples_taken ++;
          }
          //samples_taken = samples_taken;
        } while (samples_taken < NUM_SAMPLES);
        unsetTimer1();
        samples_taken = 0;
        PORTC ^= 1 << 2;
        PORTB ^= 1 << 1;
      }
    }
    hello = 1;
  }
}

ISR (TIMER1_OVF_vect)
{
  TCNT1 = 65535 - timer_sub;
  timer_rdy = 1;
}

ISR (TIMER2_OVF_vect)
{
  unsigned long m = timer2_counter;
  unsigned long f = timer2_fract;
  bool b = timer2_counter_flag;
  
  if(b == true){
    m += 1;
    f += 3;
    if (f >= 125) {
      f -= 125;
      m += 1;
    }

    timer2_fract = f;
    timer2_counter = m;
    timer2_counter_flag = b;
  }
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
  byte lo, hi;

  for (i = 0; i < 8; i++)
  {
    lo = ADCcode[i] & 0xFF;
    val[1] = lo;
    hi = ADCcode[i] >> 8;
    val[0] = hi;

    flash.writeByte(ChNStartPos[i] + ChNCurrPos[i], hi);
    ChNCurrPos[i] = ChNCurrPos[i] + 1;
    flash.writeByte(ChNStartPos[i] + ChNCurrPos[i], lo);
    ChNCurrPos[i] = ChNCurrPos[i] + 1;
  }
}


void resetFlashAddr()
{
  for (int i = 0; i < 8; i++)
    ChNCurrPos[i] = 0;
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

void setTimer2()
{
  cli();         // disable global interrupts

  _TCCR2A = TCCR2A;
  _TCCR2B = TCCR2B;
  _TIMSK2 = TIMSK2;
  
  TCCR2A = 0;    // set entire TCCR1A register to 0
  TCCR2B = 0;    // set entire TCCR1B register to 0 
  TCNT2= 255 - 251;

  TIMSK2 |= (1 << TOIE2);
  //Set interrupt on compare match

  TCCR2B |= (1 << CS22);// set prescaler to 64 and starts the timer

  sei(); // enable interrupts
}

void unsetTimer1()
{
  TCCR1A = _TCCR1A;
  TCCR1B = _TCCR1B;
  ICR1 = _ICR1;
  TIMSK1 = _TIMSK1;
}

void unsetTimer2()
{
  TCCR2A = _TCCR2A;
  TCCR2B = _TCCR2B;
  TIMSK2 = _TIMSK2;
}

//Returns value of ID in command string
int idParser()
{
  //ID is first two bytes in DATA array
  char ID[2] = {(char)radio.DATA[0], (char)radio.DATA[1]};
  return atoi(ID);
}

int cmdParser()
{
  //CMD is third byte in DATA array
  char cmd[] = {(char)radio.DATA[2]};
  return atoi(cmd);
}

void start_timer2(){
  unsigned long m = timer2_counter;
  unsigned long f = timer2_fract;
  bool b = timer2_counter_flag;
  
  m = 0;
  f = 0;
  b = true;

  timer2_fract = f;
  timer2_counter = m;
  timer2_counter_flag = b;
}

int get_timer2(){
  unsigned long m;
  uint8_t oldSREG = SREG;

  cli();
  m = timer2_counter;
  SREG = oldSREG;
  return m;
}

int end_timer2(){
  unsigned long m = timer2_counter;
  unsigned long f = timer2_fract;
  bool b = timer2_counter_flag;
  
  b = false;
  timer2_counter_flag = b;
  return m;
}

void wait_ms(int ms){
  unsigned long m = 0;
  start_timer2();
  while(m < ms){
    m = get_timer2();
  }
  end_timer2();
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
  byte lo, hi;
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

  CTB = (CTB_array[0] << 4) || (CTB_array[1]);
}

int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}













