//#include <avr/io.h>
//#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>
//#include <LiquidCrystal.h>

#include "Wire.h"
#include "Adafruit_LiquidCrystal.h"

// Pin Assignment:
const int LED = A0; // TODO
const int buzzer = A1; // TODO
const int motion = A2; // TODO
const int SHD = A3;       // SHD control pin
const int mod = A4;
const int rdyclk  = A5;
// serial commands on pin 0 and 1, LCD on pins 2 to 5
const int relay = 6;       // Relay control pin TODO
//const int button1 = 7;  // scroll button :D I did it
const int fskSignalPin = 8;  // Pin connected to the FSK signal (ICU pin)
//const int button2 = 9;  // select button TODO
// display enable on pin 10 and reset on pin 11
const int doorSensor = 12; // TODO
const int motionserialin = 13; // TODO
uint8_t choice = 1;

// Communication Commands
uint8_t R; // Status (0-4; off, Ready, User&Profiles, Login, Logout)
uint8_t U; // Number of user's profiles (0-4;255 User not found)
bool Y; // Door Relay OFF/ON
bool S; // SHD OFF/ON
bool D; // R/G LEDs RED/GREEN
bool B; // Buzzer OFF/ON
bool BACKLIGHT_SETTING = true; //Backlight setting to update.
bool BACKLIGHT_STATUS = true;  //Backlight current state.
String message1 = "";
String message2 = "";
String message3 = "";
String message4 = "";

//A timer to wait until we can disable the LCD.
volatile uint32_t display_off_time_ticks = 0; 
volatile uint8_t overflow_count = 0;

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
//const int rs = 11, en = 10, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
// LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
//LiquidCrystal lcd(11, 10, 5, 4, 3, 2);
const int LCD_latch = 11, LCD_data = 10, LCD_clk = 9; 
Adafruit_LiquidCrystal lcd(LCD_data, LCD_clk, LCD_latch);


#define ___ARR_SIZE___ 100 //captures bit data in 200 bit chunks to capture at least 1 complete read

//Uncomment this line to enable a verbose output mode.
//The state machine is disabled, and the decoder does not run.
//#define ___DEBUG___

// Global Variables:
volatile bool FirstMeas = 1;
volatile uint16_t T1OVF_Counter = 0; //overflow counter
const unsigned long CLK_IO = 16000000; //clock speed of arduino UNO

volatile unsigned long T1, T2 = 0; //used to capture period of card frequency, which holds binary info
volatile uint8_t Low_Pulses = 0;  // FSK 0
volatile uint8_t High_Pulses = 0; // FSK 1

volatile uint8_t data_index = 0;
volatile char data_array[___ARR_SIZE___] = {0};

volatile uint8_t STATE = 0;

volatile uint8_t COMMAND_FLAG = 0;

//defines expected period of FSK modulation on iCards
const unsigned long Low_Period_min = 960 / 8;  // 60us * 16,000,000Hz
const unsigned long Low_Period_max = 1288 / 8;  // 80.5us * 16,000,000Hz
const unsigned long High_Period_min = 1296 / 8; // 81us * 16,000,000Hz
const unsigned long High_Period_max = 1600 / 8; // 100us * 16,000,000Hz

//variables for handling output to LCD
const unsigned long message1_TIMEOUT = 5;  // ms timeout for message1 completion
#define MAX_BUFFER_SIZE 75//150           // Max number of bytes we can buffer
//volatile uint8_t buffer[MAX_BUFFER_SIZE];
//size_t serialBufferIndex = 0;

// boolean to check if a card was scanned
// bool cardScanned = false;

// array that holds byte data
uint8_t bytes[39];

uint32_t card_number = 0;

/*
struct HID_CARD_DATA{
    volatile uint64_t Raw_Decoded_Data;
    volatile uint16_t Card_Format = 0;
    volatile uint16_t Facility_Code = 0;
    volatile uint32_t Card_Number = 0;
};

struct HID_CARD_DATA Card_Data;
*/

///!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//This is some hardware timer code that I had ChatGPT write up to allow us to start a timer in the event the computer system wants to only keep the display online for a few moments.
///!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


/*
ISR(TIMER2_OVF_vect)
 {

    // overflow_count++;
    // if (overflow_count >= 61)
    // {
    //     // 1 second has passed
    //     overflow_count = 0;
    // }//End if

    if (display_off_time_ticks == 0)
    {
      //lcd.noDisplay();
      lcd.setBacklight(LOW);
      timer2_disable();
    }//End if
    else
      --display_off_time_ticks;
}//End Timer2 Overflow Vector

void timer2_enable() 
{
    TCCR2A = 0;                // Normal mode
    TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20); 
                               // Prescaler = 1024
    TIMSK2 = (1 << TOIE2);     // Enable Timer2 overflow interrupt
    TCNT2 = 0;                 // Reset counter
}//End timer2_enable

void timer2_disable() 
{
    TCCR2B = 0;                      // Stop the clock (no prescaler)
    TIMSK2 &= ~(1 << OCIE2A);        // Disable compare match interrupt
    TCNT2 = 0;                       // Reset counter (optional)
}//End timer2_disable
*/
///!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
///!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

/*
 *  parity_check_binary
 *  0 = even parity
 *  1 = odd parity
 *
*/
char parity_check_binary(uint64_t data)
{
  uint8_t ones_count = 0;

  while(data)
  {
    if (data & 0x01)
      ++ones_count;

    data >>= 1;
  }//End while loop

  if (ones_count % 2)
    return 1;
  else
    return 0;
}//End parity_check_binary


char decode_HID_35_bit_125KHz_Card_Data (char* input, uint8_t in_size, char* output, uint8_t* out_size)
{
  //TODO: Update this for performance purposes by making everything simplified to a single 64-bit bit-wise operation.

  bool parity_bit[3] = {0};
  uint16_t facility_code = 0;
  uint32_t card_number = 0;

  //00011101 - preamble
  //Remaining 88 bits are manchester encoded:
  
  if(manchester_decoding(input, in_size, output, out_size) <= 0)
    return -1;

  //The next 9 bits are card format/length information:
  //Normally, this is 000000101
  //Bits 10, 11, and 44 are parity bits:
  // 10 -> Odd parity
  // 11 -> Even parity
  // 44 -> Odd parity
  parity_bit[0] = output[9] - '0';
  parity_bit[1] = output[10] - '0';
  parity_bit[2] = output[43] -'0';



}//End decode_HID_35_bit_125KHz_Card_Data

char decode_HID_35_bit_125KHz_Card_Data_Binary (char* input, uint8_t in_size, uint64_t* output)
{
  //bool parity_bit[3] = {0};
  uint16_t card_format = 0;
  uint16_t facility_code = 0;
  card_number = 0;

  //00011101 - preamble
  //Remaining 88 bits are manchester encoded:
  if(manchester_decoding_binary(input, in_size, output) <= 0)
    return -1;

  //The next 9 bits are card format/length information:
  //Normally, this is 000000101
  //Bits 10, 11, and 44 are parity bits:
  // 10 -> Odd parity 1
  // 11 -> Even parity 2
  // 44 -> Odd parity 3
  //parity_bit[0] = (0x000400000000 & (*output)) >> 34;
  //parity_bit[1] = (0x000200000000 & (*output)) >> 33;
  card_format   = (0x0FF800000000 & (*output)) >> 35;
  facility_code = (0x0001FFE00000 & (*output)) >> 21;
  card_number   = (0x0000001FFFFE & (*output)) >>  1;
  //parity_bit[2] =  0x000000000001 & (*output);

  // // Serial.print("Raw Data: ");
  // // Serial.print((uint32_t)((*output) >> 32), BIN);
  // // Serial.println((uint32_t)((*output) & 0x00000000FFFFFFFF), BIN);

  // // Serial.print("Card Format: ");
  // // Serial.println(card_format, BIN);
  // // Serial.println(card_format, DEC);

  // // Serial.print("Facility Code: ");
  // // Serial.println(facility_code, BIN);
  // // Serial.println(facility_code, DEC); 

  // // Serial.print("Card Number: ");
  // // Serial.println(card_number, BIN);
  // // Serial.println(card_number, DEC);

//0011.1011.0110.1101.1011.0110.1101.1011.0110
  //Check the even parity first:
  if(parity_check_binary(0x0003B6DB6DB6 & (*output)) == 1) //?????
  {
    // Serial.println("First even parity check failed!");
    return -2; //even parity failed.
  }//End if
  //Check the odd parity second:
  if(parity_check_binary(0x00036DB6DB6D & (*output)) == 0)
  {
    // Serial.println("Second odd parity check failed!");
    return -3;  //odd parity failed.
  }//End if
  if(parity_check_binary(0x0007FFFFFFFF & (*output)) == 0)
  {
    // // Serial.println("Third odd parity check failed!");
    return -4;  //odd parity failed.
  }//End if

  return 0;
}//End decode_HID_35_bit_125KHz_Card_Data_Binary


char manchester_decoding(char* input, uint8_t in_size, char* output, uint8_t* out_size)
{
    //Null pointers and empty array checks:
    if(input == 0 || in_size == 0 || out_size == 0 || output == 0) 
      return -1;
    //If there isn't an even number of characters disregard the data.
    else if (in_size % 2)
      return -2;

    *out_size = 0;

    //Manchester encoding is as follows:
    //  01 => 0
    //  10 => 1
    // Example: 01010110  => 01 01 01 10 => 0 0 0 1
    // Example: 10100110  => 10 10 01 10 => 1 1 0 1          
    for(int i = 0; i < in_size / 2; i += 2)
    {
      if (input[i] == '0' && input[i + 1] == '1')     //Manchester Logic 0
        output[(*out_size)++] = '0';
      else if (input[i] == '1' && input[i + 1] == '0')  //Manchester Logic 1
        output[(*out_size)++] = '1';
      else                                              //Unknown Data proceed anyways...
        output[(*out_size)++] = '*';
    }//End for loop

    return *out_size;
}//End manchester_decoding

/*
 *  manchester_decoding_binary
 *  Same as manchester_decoding but it uses a bitwise method to store the data in a 64-bit integer to save on processing time and space. 
 * Returns the number of decoded bits.
 *  
*/
char manchester_decoding_binary(char* input, uint8_t in_size, uint64_t* output)
{
    //Null pointers and empty array checks:
    if(input == 0 || in_size == 0) 
      return -1;
    //If there isn't an even number of characters disregard the data.
    else if (in_size % 2)
      return -2;

    uint8_t out_size = 0;
    (*output) = 0;

    //Manchester encoding is as follows:
    //  01 => 0
    //  10 => 1
    // Example: 01010110  => 01 01 01 10 => 0 0 0 1
    // Example: 10100110  => 10 10 01 10 => 1 1 0 1          
    for(int i = 8; i < 96; i += 2)    //There are a total of 96 bits of data that we read, but the first 8 bits are preamble, so ignore them and decode the remaining 88.
    {
      (*output) <<= 1;
      if (input[i] == '0' && input[i + 1] == '1')     //Manchester Logic 0
        (*output) |= 0;
      else if (input[i] == '1' && input[i + 1] == '0')  //Manchester Logic 1
        (*output) |= 1;
      else                                              //Unknown Data ERROR
        return -1;
      ++out_size;   
    }//End for loop

    return out_size;
}//End manchester_decoding_binary


char state_machine(char input) //state machine to check for preamble
{
#ifndef ___DEBUG___                             //     0         1      1      2        6
  if ((STATE >= 0 && STATE <= 2) || STATE == 6) //Start State - '0' - '00' - '000' - '0001110'
    if (input == '0')
    {
      STATE++;
      insert_character(input);
    }  
    else
    {
      STATE = 0;
      data_index = 0;
    }
  else if (STATE == 3)  //States: '0001'
  {
    if (input == '1')
    {
      STATE++;
      insert_character(input);
    }
    else if (input != '0')
    {
      STATE = 0;
      data_index = 0;      
    }    
  }//End else if (STATE == 3)
  else if (STATE == 4 || STATE == 5 || STATE == 7) //States: '0001' - '00011' - '000111' - '00011101'
    if (input == '1')
    {
      STATE++;
      insert_character(input);
    } 
    else
    {
      STATE = 0;
      data_index = 0;
    }

  else  //Read data state:
  {
    insert_character(input);
  }
#else
  insert_character(input);
#endif

    return 0; //Future error reporting.
}//End state_machine


void insert_character(char input) //fills buffer with valid bit data
{
  data_array[data_index++] = input;
  
  if (data_index > ___ARR_SIZE___ - 1)
  {
      digitalWrite(SHD, HIGH);
      //cli();
      //Shut down the timer1 unit.
      TCCR1A = 0;          
      TCCR1B = 0;          
      TIMSK1 = 0;
      T1OVF_Counter = 0;
      //sei();
      FirstMeas = 1;
      Low_Pulses = 0;
      High_Pulses = 0;  
      STATE = 0;         
  }//End if
}//End insert_character

// Timer1 Overflow Interrupt
ISR(TIMER1_OVF_vect)
{
  T1OVF_Counter = 1;
}//End Timer1 Overflow Interrupt

// Timer1 Input Capture Interrupt (to measure period of FSK signal)
ISR(TIMER1_CAPT_vect)
{
  //digitalWrite(13, LOW);

  uint32_t Period = 0;

  if (FirstMeas)
  {
    T1 = (unsigned long)ICR1;
    FirstMeas = 0;
    T1OVF_Counter = 0;
  }
  else
  {
    T2 = (unsigned long)ICR1;
    
    Period = ((T2 + (65535 * T1OVF_Counter)) - T1);
    T1 = T2;

    // Low Period Detected:
    if (Period >= Low_Period_min && Period <= High_Period_min)
    {
      ++Low_Pulses;
      if (High_Pulses)
      {
        if (High_Pulses != 1)
        {        
          //// Serial.print("X");
          //insert_character('X');
          state_machine('X');
          //digitalWrite(13, HIGH);
        }
        High_Pulses = 0;
      }

      if (Low_Pulses == 6)
      {
        Low_Pulses = 0;
        //// Serial.print("0");
        //digitalWrite(12, LOW);
        //insert_character('0');
        state_machine('0');
      }
    }
    // High Period Detected:
    else if (Period >= High_Period_min && Period <= High_Period_max)
    {
      ++High_Pulses;
      if (Low_Pulses)
      {
        if (Low_Pulses != 1)
        {
          //insert_character('X');
          state_machine('X');
          //digitalWrite(13, HIGH); //pin 12 vs 13 for logic analyzer debugging  
        }
        //// Serial.print("X");
          Low_Pulses = 0;
      }

      if (High_Pulses == 5)
      {
        High_Pulses = 0;
        //// Serial.print("1");
        //digitalWrite(12, HIGH);
        //insert_character('1');
        state_machine('1');
      }
    }
    //Fall Through:
    else
    {
      //Error, invalid capture.
      Low_Pulses = 0;
      High_Pulses = 0;
      //// Serial.print("\n\rInvalid period! ");
      //// Serial.println(Period, DEC);
      //// Serial.print(" ticks.\n\r");
      //// Serial.print("-");
      //insert_character('-');
      state_machine('-');
    }
  }
     T1OVF_Counter = 0;
}//End Timer1 Input Capture Interrupt


// BUTTON METHOD
bool buttonPushed(int button){
  return digitalRead(button) == HIGH;  // assuming button is active LOW
}


// Talk to the comuter module (Send bytes including card serial number and account number
void WriteCereal(uint32_t cardnumenc, int j) 
{
  digitalWrite(SHD, HIGH);
  //Serial.end();
  Serial.begin(9600);
  uint8_t Arr[3];
    for(int i = 0; i < 3; i++){  //for loop to capture 2 digit integers of card number
      Arr[i] = cardnumenc%(100); //% sign for byte capture
        cardnumenc /= 100;
    }
    //Mimic the RI-STU-MRD1 Micro-reader (2000 series reader system).
    Serial.write(1); //             Start Mark
    Serial.write(9); //             Length
    Serial.write(12); //Preamble    Status
    Serial.write(Arr[2]); //        Data field (1) [LSB]
    Serial.write(Arr[1]); //        Data field (2)
    Serial.write(Arr[0]); //Card number   Data field (3)
    Serial.write(j); //Account number     Data field (4)
    Serial.write(0); //                   Data field (5)
    Serial.write(0); //                   Data field (6)
    Serial.write(0); //                   Data field (7)
    Serial.write(0); //buffer 0's         Data field (8) [MSB]
    Serial.write(9 ^ 12 ^ Arr[2] ^ Arr[1] ^ Arr[0] ^ j); //checksum
    // delay(50);
    Serial.end();
    Serial.begin(19200);
    // card_number = 0;
    //S = 0;
    //digitalWrite(SHD, HIGH);
    delay(1000);

data_index = 0;
//Re-enable the system.
TCCR1A = 0;           // Initialize Timer1A
TCCR1B = 0;           // Initialize Timer1B
TCCR1B |= B00000010;  // Internal Clock, Prescaler = 8, ICU Filter DIS, ICU Pin RISING
TIMSK1 |= B00100001;  // Enable Timer1 Overflow and Capture Interrupts

    digitalWrite(SHD, LOW);
 }//End WriteCereal

const unsigned long MESSAGE_TIMEOUT = 5;  // ms timeout for message completion
bool secondMessage = false;

int SerialReadCommand() 
{
  uint8_t cmd_buffer[MAX_BUFFER_SIZE];
  int index = 0;
  unsigned long lastByteTime = 0;

  if (Serial.available())
  {
    index = Serial.readBytes(cmd_buffer, MAX_BUFFER_SIZE);
  }

  // If timeout passed and buffer has data, process it
  if (index > 0) // && millis() - lastByteTime > MESSAGE_TIMEOUT) 
  {
    // Serial.print("index = ");
    // Serial.print(index);
    message1 = "";
    message2 = "";
    int i = 0;
    bool justWroteChar = false;
    secondMessage = false;

    while (i < index) {
      if (cmd_buffer[i] == 254) // Start of instruction 
      {  
        i++;
        if (i >= index) break;
        uint8_t instruction = cmd_buffer[i++];

        // Handle known instruction sequences
        if (instruction == 70) //Display off
        {
            //lcd.noDisplay();  //Turns off the LCD display, without losing the text currently shown on it.
//lcd.setBacklight(LOW);
BACKLIGHT_SETTING = false;
        }//End if (70) Display off
        
        else if (instruction == 71)  //Set Cursor Position [column] [row]
        {
          //The original unit goes from 1 to 20 in columns, and 1 to 2 for rows. The Arduino library goes from 0 to 15 and 0 to 1.
          uint8_t column = (cmd_buffer[i++]) - 1;
          uint8_t row = (cmd_buffer[i++]) - 1;

          if (column > 15)
            column = 15;

//lcd.setCursor(column, row);

          //i += 2;  // Skip 2 extra bytes
          secondMessage = true;
        }//End else if (71) Set Cursor Position

        else if (instruction == 86 || instruction == 87) 
        {
          if (i >= index) break;
          uint8_t value = cmd_buffer[i++];

          if (instruction == 86) //Turn GPIO off ('V')
          {        
            if (value == 1) S = false;
            else if (value == 2) D = false;
            else if (value == 4) Y = false;
            else if (value == 5) B = false;
          }//End if (86) GPIO OFF

          else if (instruction == 87) //Turn GPIO on ('W')
          {   
            if (value == 1) S = true; 
            else if (value == 2) D = true;
            else if (value == 4) Y = true;
            else if (value == 5) B = true;
          }//End if (87)  GPIO ON
        }//End else if (86/87)

        else if (instruction == 88) //Clear Display
        {
//lcd.clear(); //Clears the LCD screen and positions the cursor in the upper-left corner
        }//End else if (88) Clear Display

        else if (instruction == 37) //Keypad or GPO mode [mode] 0 -> keypad, 1 -> GPO
        {
          i += 1;  // Skip 1 extra byte
        }//End else if Keypad or GPO mode

        else if (instruction == 66) //Display On [minutes] (R)
        {
          //This command turns on the display on for a time of [minutes] minutes. If [minutes] is zero (0), the display will remain on indefinitely.
            //uint8_t minutes = cmd_buffer[i++];
            i++;
            BACKLIGHT_SETTING = true;
            //lcd.display();
//lcd.setBacklight(HIGH);
            /* We will not need the timer, because the PC always disables the display for us. And this interferes with the buzzer.
            if (minutes != 0)   //if it is 0 then leave it on indefinitely.
            {
              //60 seconds per minute.
              //61 ticks per second.
              display_off_time_ticks = minutes * 60 *  61;
              timer2_enable();
            }//End if
            */
        }//End else if (66) Dispaly On
///!*!*!*!*!*!*!*!*!*!*
/// REVIEW THIS. I'M NOT SURE EXACTLY WHAT IT DOES.
///!*!*!*!*!*!*!*!*!*!*        
        // Add space if a char was just written
        if (justWroteChar) 
        {
          if (!secondMessage)       message1 += ' ';
          else                      message2 += ' ';
          justWroteChar = false;
        }//End if (justWroteChar)

      }//End if (254) COMMAND 
      
      else // Regular character
      {  
        if (isPrintable(cmd_buffer[i])) 
        {
          if (!secondMessage)
            message1 += (char)cmd_buffer[i];
          else
            message2 += (char)cmd_buffer[i];
          justWroteChar = true;
        }//End if (Character to display)
        i++;
        if (message1[0] != "") 
        {
        message3 = message1;
        message4 = message2;
        }//End if
      }//End else Regular Character
    }//End while loop

    // Serial.println();
    // Debug: Output control variables
    // Serial.print("Y="); // Serial.print(Y);
    // Serial.print(" S="); // Serial.print(S);
    // Serial.print(" D="); // Serial.print(D);
    // Serial.print(" B="); // Serial.println(B);

    // Output filtered messages
    // Serial.println("Filtered ASCII Output:");
    // Serial.println(message1);
    if (secondMessage) {
      // Serial.println(message2);
    }

    // Update LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    for (char c : message1) {
      lcd.print(c);
    }
    if (secondMessage) {
      lcd.setCursor(0, 1);
      for (char c : message2) {
        lcd.print(c);
      }
    }

    index = 0;  // Reset buffer
  }
}//End SerialReadCommand

void cardRead()
{

  if (data_index > ___ARR_SIZE___ - 1)
  {
    //digitalWrite(13, HIGH);   //Give an indication to the user that the system is in transmission mode.
    uint64_t raw_data = 0;
    decode_HID_35_bit_125KHz_Card_Data_Binary (data_array, data_index, &raw_data);
    //// Serial.print("Binary Data: ");
    //// Serial.print((uint32_t)(raw_data >> 32), BIN);
    //// Serial.print("|");
    //// Serial.println((uint32_t)(raw_data & 0x00000000FFFFFFFF), BIN);

    data_index = 0;
    //Re-enable the system.
    TCCR1A = 0;           // Initialize Timer1A
    TCCR1B = 0;           // Initialize Timer1B
    TCCR1B |= B00000010;  // Internal Clock, Prescaler = 8, ICU Filter DIS, ICU Pin RISING
    TIMSK1 |= B00100001;  // Enable Timer1 Overflow and Capture Interrupts
    //digitalWrite(13, LOW); //Give an indication to the user that the system has exited transmission mode.
    digitalWrite(SHD, HIGH);
    //digitalWrite(SHD, LOW);
  }
}//End cardRead

// Digital write pins at once
void UpdateOutputs () 
{
  digitalWrite(LED, D);
  digitalWrite(relay, !Y);
  //if (S == 1) digitalWrite(SHD, LOW);
  //else        digitalWrite(SHD, HIGH);
  digitalWrite(SHD, !S);
  //digitalWrite(buzzer, B);
  if (B == 1)     tone(buzzer, 2300);
  else            noTone(buzzer);

  /*
  if (BACKLIGHT_SETTING != BACKLIGHT_STATUS)
  {
    lcd.setBacklight(BACKLIGHT_SETTING);
    BACKLIGHT_STATUS = BACKLIGHT_SETTING;
  }//End backlight if
  */
}//End UpdateOutputs

// LCD display (line1, line2)
// void display(const String line1, const String line2) {
//   lcd.display();
//   // lcd.clear();                // Clear previous display
//   lcd.setCursor(0, 0);        // Line 1, column 0
//   lcd.print(line1);           // Print first line
//   lcd.setCursor(0, 1);        // Line 2, column 0
//   lcd.print(line2);           // Print second line
//   delay(100);
// }

void setup()
{
  pinMode(mod, OUTPUT);
  digitalWrite(mod, LOW);
  //pinMode(button1, INPUT_PULLUP);  // Internal pull-up resistor
  //pinMode(button2, INPUT_PULLUP);  // Internal pull-up resistor
  pinMode(doorSensor, INPUT);
  pinMode(LED, OUTPUT);  // Set analog pin A0 as digital output
  pinMode(buzzer, OUTPUT);
  pinMode(relay, OUTPUT);
  Serial.begin(19200);
  //Serial.setTimeout(500); //Set a timeout of 500 millisecond when reading data.
  Serial.setTimeout(1000);
  // Initialize serial communication
  // // Serial.println("FSK Decoder ONLINE:\n\r");


  //digitalWrite(relay, LOW);      // Start with relay OFF (door locked)
  digitalWrite(relay, HIGH);      //the relay needs to be turned on now to lock the door.




  // Pin Setup
  pinMode(fskSignalPin, INPUT);  // Set the FSK signal pin as input, pin assignment defined at top of code
  pinMode(SHD, OUTPUT);  // SHD control pin for EM4095 HIGH => SLEEP, LOW => ONLINE
  digitalWrite(SHD,  HIGH);
  delay(100);
  digitalWrite(SHD, LOW);
  //digitalWrite(mod, LOW);




  // Timer1 Setup for FSK signal detection
  TCCR1A = 0;           // Initialize Timer1A
  TCCR1B = 0;           // Initialize Timer1B
  TCCR1B |= B00000010;  // Internal Clock, Prescaler = 1, ICU Filter EN, ICU Pin FALLING
  TIMSK1 |= B00100001;  // Enable Timer1 Overflow and Capture Interrupts
  
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2); //16 columns, 2 rows
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  //lcd.setBacklight(0);    //This causes a lock-up on the card reader.

  S = true;
  
}//End setup

// millis variables
static unsigned long lastReadyTime = 0;
static unsigned long stateStartTime = 0;
static unsigned long buttonDebounceTime = 0;
static unsigned long lastSerialReadTime = 0;

const unsigned long debounceDuration = 300;     // 300ms debounce
const unsigned long stateWaitDuration = 10000;  // 10 seconds
const unsigned long serialReadInterval = 2000;  // 1 second

bool door = false;

void loop() {
  //Check if the door has been opened:
  bool sensor = digitalRead(doorSensor);

  // Trigger only when state goes from LOW to HIGH
  if (sensor == 1) 
  {
///!*!*!*!*!*!*!*!*!*!*
///THIS MAY BE THE SOURCE OF THE DISPLAY ERROR!!!
///!*!*!*!*!*!*!*!*!*!*
    // Update LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    for (char c : "Door is open") 
    {
      if (c != 0)
        lcd.print(c);
    }//End for loop
    lcd.setCursor(0, 1);
    for (char c : "Close the door") 
    {
      if (c != 0)
        lcd.print(c);
    }//End for loop
///!*!*!*!*!*!*!*!*!*!*
///!*!*!*!*!*!*!*!*!*!*

    tone(buzzer, 2300);
    delay(100);
    door = true;

    while(digitalRead(doorSensor)); //Wait until the door is closed.
  }//End if (sensor == 1)
  else if (door == true) 
  {
    // Update LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    for (char c : message1) {
      lcd.print(c);
    }
    if (secondMessage) {
      lcd.setCursor(0, 1);
      for (char c : message2) {
        lcd.print(c);
      }
    }
    door = false;
  }//End else if (door == true)
  else 
  {
    SerialReadCommand();
    // display(message1, message2);
    UpdateOutputs();

    // if (S == 0) { // READY
    // memset((void*)data_array, 0, ___ARR_SIZE___); // Reset data_array to avoid double tap
    if (card_number == 0) 
    {
      cardRead();
    }//End if (card_number == 0)
    //   delay(500);
    //Successful card read:
    //if (card_number != 0) 
    else
    {
      tone(buzzer, 2300);
      delay(100);
      noTone(buzzer);
      // Serial.println(card_number, DEC);
      WriteCereal(card_number, 0); // Send card with default profile
      card_number = 0;
      //delay(1000);
      //stateStartTime = millis();
    }//End if (card_number != 0)
  }//End else
}//End void loop

