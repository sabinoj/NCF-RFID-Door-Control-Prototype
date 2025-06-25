//#include <avr/io.h>
//#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>
#include <LiquidCrystal.h>

// Pin Assignment:
const int LED = A0; // TODO
const int buzzer = A1; // TODO
const int motion = A2; // TODO
const int SHD = A3;       // SHD control pin
const int mod = A4;
const int rdyclk  = A5;
// serial commands on pin 0 and 1, LCD on pins 2 to 5
const int relay = 6;       // Relay control pin TODO
const int button1 = 7;  // scroll button :D I did it
const int fskSignalPin = 8;  // Pin connected to the FSK signal (ICU pin)
const int button2 = 9;  // select button TODO
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
String userName = "";
String profile1 = "";
String profile2 = "";
String profile3 = "";
String profile4 = "";
String* profilePtr[] = { &profile1, &profile2, &profile3, &profile4 };


// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 11, en = 10, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
// LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
LiquidCrystal lcd(11, 10, 5, 4, 3, 2);

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

//defines expected period of FSK modulation on iCards
const unsigned long Low_Period_min = 960 / 8;  // 60us * 16,000,000Hz
const unsigned long Low_Period_max = 1288 / 8;  // 80.5us * 16,000,000Hz
const unsigned long High_Period_min = 1296 / 8; // 81us * 16,000,000Hz
const unsigned long High_Period_max = 1600 / 8; // 100us * 16,000,000Hz

//variables for handling output to LCD
const unsigned long message1_TIMEOUT = 5;  // ms timeout for message1 completion
#define MAX_BUFFER_SIZE 100           // Max number of bytes we can buffer
volatile uint8_t buffer[MAX_BUFFER_SIZE];
size_t serialBufferIndex = 0;


// boolean to check if screen should be on or off
bool screenOff;

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

  // Serial.print("Raw Data: ");
  // Serial.print((uint32_t)((*output) >> 32), BIN);
  // Serial.println((uint32_t)((*output) & 0x00000000FFFFFFFF), BIN);

  // Serial.print("Card Format: ");
  // Serial.println(card_format, BIN);
  // Serial.println(card_format, DEC);

  // Serial.print("Facility Code: ");
  // Serial.println(facility_code, BIN);
  // Serial.println(facility_code, DEC); 

  // Serial.print("Card Number: ");
  // Serial.println(card_number, BIN);
  // Serial.println(card_number, DEC);

//0011.1011.0110.1101.1011.0110.1101.1011.0110
  //Check the even parity first:
  if(parity_check_binary(0x0003B6DB6DB6 & (*output)) == 1) //?????
  {
    Serial.println("First even parity check failed!");
    return -2; //even parity failed.
  }//End if
  //Check the odd parity second:
  if(parity_check_binary(0x00036DB6DB6D & (*output)) == 0)
  {
    Serial.println("Second odd parity check failed!");
    return -3;  //odd parity failed.
  }//End if
  if(parity_check_binary(0x0007FFFFFFFF & (*output)) == 0)
  {
    Serial.println("Third odd parity check failed!");
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
      cli();
      //Shut down the timer1 unit.
      TCCR1A = 0;          
      TCCR1B = 0;          
      TIMSK1 = 0;
      T1OVF_Counter = 0;
      sei();
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
          //Serial.print("X");
          //insert_character('X');
          state_machine('X');
          //digitalWrite(13, HIGH);
        }
        High_Pulses = 0;
      }

      if (Low_Pulses == 6)
      {
        Low_Pulses = 0;
        //Serial.print("0");
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
        //Serial.print("X");
          Low_Pulses = 0;
      }

      if (High_Pulses == 5)
      {
        High_Pulses = 0;
        //Serial.print("1");
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
      //Serial.print("\n\rInvalid period! ");
      //Serial.println(Period, DEC);
      //Serial.print(" ticks.\n\r");
      //Serial.print("-");
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
void WriteCereal(uint32_t cardnumenc, int j) {
  uint8_t Arr[3];
    for(int i = 0; i < 3; i++){  //for loop to capture 2 digit integers of card number
      Arr[i] = cardnumenc%(100); //% sign for byte capture
        cardnumenc /= 100;
    }
    Serial.write(1); //...
    Serial.write(9); //...
    Serial.write(12); //Preamble
    Serial.write(Arr[2]); //...
    Serial.write(Arr[1]); //...
    Serial.write(Arr[0]); //Card number
    Serial.write(j); //Account number
    Serial.write(0); //...
    Serial.write(0); //...
    Serial.write(0); //buffer 0's
    Serial.write(9 ^ 12 ^ Arr[2] ^ Arr[1] ^ Arr[0] ^ j); //checksum
    // delay(50);
 }

// Decode, update pins and store names
void SerialReadCommand() {
  static bool waitingForCommand = false;
  static uint8_t currentInstruction = 0;
  static String* currentProfilePtr = nullptr;
  static bool readingString = false;
  static uint8_t profileIndex = 0;

  while (Serial.available() > 0) {
    uint8_t incoming = Serial.read();

    // Check if this is the start of a new command
    if (incoming == 254) {
      waitingForCommand = true;
      readingString = false;
      currentProfilePtr = nullptr;
      continue;  // Go to next byte (the instruction)
    }

    if (waitingForCommand) {
      currentInstruction = incoming;
      waitingForCommand = false;

      // Prepare for string if needed
      if (currentInstruction == 'X') {
        userName = "";
        readingString = true;
      } else if (currentInstruction == 'P') {
        if (profileIndex < 4) {
          if (profileIndex == 0) currentProfilePtr = &profile1;
          else if (profileIndex == 1) currentProfilePtr = &profile2;
          else if (profileIndex == 2) currentProfilePtr = &profile3;
          else if (profileIndex == 3) currentProfilePtr = &profile4;
          if (currentProfilePtr) *currentProfilePtr = "";
          readingString = true;
        }
      }
      continue;  // Go to next byte (either data or string content)
    }

    // Handle string reading
    if (readingString) {
      if (incoming == 254) {
        // We saw a new command marker in the middle of string reading
        waitingForCommand = true;
        readingString = false;
        currentProfilePtr = nullptr;
        continue;  // Next loop will handle new command
      }

      // Append to the correct string
      if (currentInstruction == 'X') {
        userName += (char)incoming;
      } else if (currentInstruction == 'P') {
        if (currentProfilePtr) {
          *currentProfilePtr += (char)incoming;
        }
      }
      continue;
    }

    // Handle single-byte commands
    if (currentInstruction == 'R') {
      R = incoming;
    } else if (currentInstruction == 'U') {
      U = incoming;
    } else if (currentInstruction == 'Y') {
      Y = incoming;
    } else if (currentInstruction == 'S') {
      S = incoming;
    } else if (currentInstruction == 'D') {
      D = incoming;
    } else if (currentInstruction == 'B') {
      B = incoming;
    }
    // Reset state after handling
    currentInstruction = 0;
  }
}

void cardRead()
{
  if (data_index > ___ARR_SIZE___ - 1)
  {
    //digitalWrite(13, HIGH);   //Give an indication to the user that the system is in transmission mode.
    uint64_t raw_data = 0;
    decode_HID_35_bit_125KHz_Card_Data_Binary (data_array, data_index, &raw_data);
    //Serial.print("Binary Data: ");
    //Serial.print((uint32_t)(raw_data >> 32), BIN);
    //Serial.print("|");
    //Serial.println((uint32_t)(raw_data & 0x00000000FFFFFFFF), BIN);

    data_index = 0;
    //Re-enable the system.
    TCCR1A = 0;           // Initialize Timer1A
    TCCR1B = 0;           // Initialize Timer1B
    TCCR1B |= B00000010;  // Internal Clock, Prescaler = 8, ICU Filter DIS, ICU Pin RISING
    TIMSK1 |= B00100001;  // Enable Timer1 Overflow and Capture Interrupts
    //digitalWrite(13, LOW); //Give an indication to the user that the system has exited transmission mode.
    digitalWrite(SHD, LOW);
  }
}//End cardRead

// Digital write pins at once
void UpdateOutputs () {
  digitalWrite(LED, D);
  digitalWrite(relay, Y);
  digitalWrite(SHD, S);
  digitalWrite(buzzer, B);
}

// LCD display (line1, line2)
void display(const String line1, const String line2) {
  lcd.display();
  lcd.clear();                // Clear previous display
  lcd.setCursor(0, 0);        // Line 1, column 0
  lcd.print(line1);           // Print first line
  lcd.setCursor(0, 1);        // Line 2, column 0
  lcd.print(line2);           // Print second line
}

void setup()
{
  pinMode(button1, INPUT_PULLUP);  // Internal pull-up resistor
  pinMode(button2, INPUT_PULLUP);  // Internal pull-up resistor
  pinMode(doorSensor, INPUT);
  pinMode(LED, OUTPUT);  // Set analog pin A0 as digital output
  pinMode(buzzer, OUTPUT);
  pinMode(relay, OUTPUT);
  Serial.begin(9600);

  // Initialize serial communication
  Serial.println("FSK Decoder ONLINE:\n\r");

  digitalWrite(relay, LOW);      // Start with relay OFF (door locked)

  // Pin Setup
  pinMode(fskSignalPin, INPUT);  // Set the FSK signal pin as input, pin assignment defined at top of code
  pinMode(SHD, OUTPUT);  // SHD control pin for EM4095 HIGH => SLEEP, LOW => ONLINE
  digitalWrite(SHD, LOW);


  // Timer1 Setup for FSK signal detection
  TCCR1A = 0;           // Initialize Timer1A
  TCCR1B = 0;           // Initialize Timer1B
  TCCR1B |= B00000010;  // Internal Clock, Prescaler = 1, ICU Filter EN, ICU Pin FALLING
  TIMSK1 |= B00100001;  // Enable Timer1 Overflow and Capture Interrupts
  
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2); //16 columns, 2 rows
  
  // Print a message to the LCD.
  lcd.noDisplay(); //turns display off
  screenOff = true;
  R = 1;
  
}//End setup

// millis variables
static unsigned long lastReadyTime = 0;
static unsigned long stateStartTime = 0;
static unsigned long buttonDebounceTime = 0;
static unsigned long lastSerialReadTime = 0;

const unsigned long debounceDuration = 300;     // 300ms debounce
const unsigned long stateWaitDuration = 10000;  // 10 seconds
const unsigned long serialReadInterval = 2000;  // 1 second

  bool isCardRead = false;

void loop() {
  // Read from serial only once per second
  // if (millis() - lastSerialReadTime >= serialReadInterval) {
  //   SerialReadCommand(); // This updates global R
  //   lastSerialReadTime = millis();
  // }
  // Reset variables
  // foundUser = 0;
  // card_number = 0;
  choice = 1;
  // while (Serial.available()) { // Flush the buffer
  //   uint8_t ser = Serial.read();
  //   Serial.print(ser);
  // }
  // memset((void*)buffer, 0, MAX_BUFFER_SIZE); // Reset buffer
  // if (Serial.available() == 0) {
  //   Serial.print("!");
  // } else {
  //   Serial.print("!!");
  // }
  // Serial.print(Serial.available());
  // char b;
  // while (Serial.available() == 0) { // Wait for command
  //   delay(500);
    // while (Serial.available() != 0) { // Clear buffer
    // b = Serial.read();
    // Serial.print(b);
    // }
    // Serial.println();
    // while (R != 1) {
    SerialReadCommand();
    Serial.println(R, DEC);
    Serial.println(U, DEC);
    Serial.println(userName);
    Serial.println(profile1);
    Serial.println(profile2);
    Serial.println(profile3);
    Serial.println(profile4);
    // }
  // }

  Serial.print("RR");
  Serial.println(R, DEC);

  if (R == 0) { // OFF
    display("RFID TRS-off", "");
    UpdateOutputs();
    // delay(3000); // 3 seconds fixed for this message
    // lcd.clear();
    // lcd.noDisplay();
    // stateStartTime = millis();
  }
  else if (R == 1) { // READY
    card_number = 0;
    // if (millis() - lastReadyTime > stateWaitDuration) {
    //   // SerialWriteCommand(1); // re-send READY every 10s
      display("RFID TRS Ready", "Scan your card");
      UpdateOutputs();
    //   lastReadyTime = millis();
    // }
    memset((void*)data_array, 0, ___ARR_SIZE___); // Reset data_array to avoid double tap
    while (card_number == 0) {
      // if ((millis() - lastReadyTime) > 500) {
        cardRead();
      //   lastReadyTime = millis();
      // }
    }
    Serial.println(card_number, DEC);
    delay(500);
    if (card_number != 0) {
      WriteCereal(card_number, 0); // Send card with default profile
      stateStartTime = millis();
      // card_number = 0; // ❗ CRITICAL: Reset to avoid spamming
    }
  }
  else if (R == 2) { // HANDLE USER
    UpdateOutputs();
    if (U == 255) { // User not found
      display(String(card_number), "Inactive User!");
      Serial.println("Inactive User!");
      delay(2000);
    } else {
      bool selected = false;
      unsigned long selectionStart = millis();
      
      if (U > 1) { // Only show selection if multiple profiles
        while (!selected && (millis() - selectionStart < stateWaitDuration)) {
          display(userName, *profilePtr[choice - 1]);
    
          if (buttonPushed(button1) && (millis() - buttonDebounceTime >= debounceDuration)) {
            selectionStart = millis();
            buttonDebounceTime = millis();
            choice = (choice % U) + 1; // Better cycling through profiles
          }
          if (buttonPushed(button2) && (millis() - buttonDebounceTime >= debounceDuration)) {
            buttonDebounceTime = millis();
            selected = true;
            WriteCereal(card_number, choice);
          }
          // delay(50);
        }
        if (!selected) { // Timeout occurred
          display("RFID TRS Ready", "Scan your card");
        }
      } else { // Single profile - auto-login
        WriteCereal(card_number, choice); // Send card with the first profile
      }
    }
    R = 1;
  }
  else if (R == 3) { // LOGIN
    display(userName, "Login");
    UpdateOutputs();
    delay(2000);
    R = 1;
  }
  else if (R == 4) { // LOGOUT
    display(userName, "Logout");
    UpdateOutputs();
    delay(2000);
    R = 1;
  }
  // R = 1;
}

