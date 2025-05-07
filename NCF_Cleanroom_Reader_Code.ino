//#include <avr/io.h>
//#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>
#include <LiquidCrystal.h>

// Computer module code
#define MAX_NAME_LEN 16
#define MAX_PROFILES 4

#define MESSAGE_LENGTH 39
uint8_t messageBuffer[MESSAGE_LENGTH];  // Global buffer to store the message

// Communication Commands
uint8_t R; // Status (0-4; off, Ready, User&Profiles, Login, Logout)
uint8_t U; // Number of user's profiles (0-n)
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

uint32_t card_number = 0;

// Define the Profile struct
typedef struct {
    String profileName;
    uint8_t profileIndex; // To keep track of how many profiles are used
    bool logged = 0;
} Profile;

// Define the User struct
typedef struct {
    String userName;
    uint32_t userID;
    Profile profiles[MAX_PROFILES];
    size_t profCount = 0;
    bool logged = 0;
} User;

// Array of users
#define MAX_USERS 4
User users[MAX_USERS];

// User info
User* foundUser = 0;
uint32_t ID;
uint8_t profIndex;

// Create user, set name and ID, save to users array
void CreateNewUser(const String name, uint32_t id) {
    static int userIndex = 0;
    if (userIndex >= MAX_USERS) return; // Prevent overflow

    users[userIndex].userName = name.substring(0, MAX_NAME_LEN-1); // Ensure length limit
    users[userIndex].userID = id;
    users[userIndex].logged = false;
    users[userIndex].profCount = 0;

    // Clear all profiles
    for (size_t i = 0; i < MAX_PROFILES; i++) {
        users[userIndex].profiles[i].profileName = "";
        users[userIndex].profiles[i].profileIndex = i;
        users[userIndex].profiles[i].logged = false;
    }

    userIndex++;
}

// Add profile to user
void AddProfileToUser(uint32_t userID, const char* profileName) {
    for (size_t i = 0; i < MAX_USERS; i++) {
        if (users[i].userID == userID) {
            // Add profile to the next available slot
            for (size_t j = 0; j < MAX_PROFILES; j++) {
                if (users[i].profiles[j].profileName[0] == '\0') {
                    users[i].profiles[j].profileName = profileName;
                    users[i].profiles[j].profileIndex = j;  // Profile index
                    users[i].profCount++;  // Increment profile count
                    return;
                }
            }
        }
    }
}

// Parse userID and profileIndex
void ParseUserData(uint32_t &ID, uint8_t &profIndex) {
  // Extract 3 bytes from indices 3, 4, 5 in little-endian order
  ID = ((uint32_t)messageBuffer[3] * 10000) + ((uint32_t)messageBuffer[4] * 100) + (uint32_t)messageBuffer[5];
  // ID =  (uint32_t)messageBuffer[5];
  // ID |= (uint32_t)messageBuffer[4] << 8;
  // ID |= (uint32_t)messageBuffer[3] << 16;

  // Extract index from index 6
  profIndex = messageBuffer[6]; 
}

// Look for user using userID
User* FindUserByID(uint32_t targetID) {
  for (size_t i = 0; i < MAX_USERS; i++) {
    if (users[i].userID == targetID) {
      return (users + i);  // Return pointer to matching user
    }
  }
  return 0;  // Not found
}

// Decode and directly send the command over Serial with names strings
void WriteCommand(uint8_t &R) {
  if (R == 0) { // off
    Y = 0;  S = 1;  D = 0;  B = 0; // Set pins
  }
  else if (R == 1) { // Ready
    Y = 0;  S = 0;  D = 0;  B = 0; // Set pins
  }
  else if (R == 2) { // User
    // Look for user
     foundUser = FindUserByID(card_number);
    if (foundUser != 0) { // If user was found
      if (!foundUser->logged) { // If user is not logged
        if (foundUser->profCount > 1) { // If user has profiles
          // Set number of users
          U = foundUser->profCount;
          userName = foundUser->userName;
          // Set user profile names        
          for (size_t i = 0; i < foundUser->profCount; i++) {
            *profilePtr[i] = foundUser->profiles[i].profileName;
          }
        }
        else { // login directly
          R = 3;
          WriteCommand(R); // Login
          return; // Exit loop
        }
      } else { // User is already loggedin
        R = 4;
        WriteCommand(R); // Logout
        return; // Exit loop
      }
    } else { // User not found
      // Set 255 to indicate User not available
      
      U = 255; // 255 means user not found
    }
    Y = 0;  S = 1;  D = 0;  B = 0; // Set pins
  }
  else if (R == 3) { // Login
    // Login user and its profile
    if (!foundUser->logged) { // If user is not logged
      foundUser->logged = true; // Login user
      if (foundUser->profCount > 1) { // If there is multiple frofiles
        // Serial.print("!");
        // Serial.print(profIndex, DEC);
        foundUser->profiles[profIndex - 1].logged = true; // Login profile
        // Serial.print(profIndex - 1, DEC);
        
        // Set profile name for confirmation
        userName = foundUser->profiles[profIndex - 1].profileName;
      }
      else { // No profiles or one profile exists
        if (foundUser->profCount == 1) {
          // Set profile name
          userName = foundUser->profiles[0].profileName;
        }
        else {
          // Set user name
          userName = foundUser->userName;
        }
      }
    } else { // User is already loggedin
        R = 4;
        WriteCommand(R); // Logout
        return; // Exit loop
      }
    Y = 1;  S = 1;  D = 1;  B = 1; // Set pins
  }
  else if (R == 4) { // Logout
    foundUser->logged = false;
    if (foundUser->profCount > 0) { // If there is multiple frofiles
      // Logoff all profiles
      for (uint8_t i = 0; i < foundUser->profCount; i++) {
        if (foundUser->profiles[i].logged) { // Find the logged profile
          foundUser->profiles[i].logged = false; // Log off
          
          // Set profile name for confirmation
          userName = foundUser->profiles[i].profileName;
        }
      }
    }
    else { // No profiles exists
      
      // Set user name
      userName = foundUser->userName;
    }
    Y = 0;  S = 1;  D = 1;  B = 0; // Set pins
  }
  else return; // Invalid status (Do not write)
}

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
uint8_t c = 1;


// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 11, en = 10, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
// LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
LiquidCrystal lcd(11, 10, 5, 4, 3, 2);

#define ___ARR_SIZE___ 100 //captures bit data in 100 bit chunks to capture at least 1 complete read

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
int statmac = 0; //sets current state of flow diagram

//defines expected period of FSK modulation on iCards
const unsigned long Low_Period_min = 960 / 8;  // 60us * 16,000,000Hz
const unsigned long Low_Period_max = 1288 / 8;  // 80.5us * 16,000,000Hz
const unsigned long High_Period_min = 1296 / 8; // 81us * 16,000,000Hz
const unsigned long High_Period_max = 1600 / 8; // 100us * 16,000,000Hz


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
    /*NOTE: THIS METHOD GOES UNUSED IN THE CODE AND CAN BE IGNORED
    PLEASE SEE THE METHOD "decode_HID_35_bit_125KHz_Card_Data_Binary" INSTEAD*/
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

  Serial.print("Raw Data: ");
  Serial.print((uint32_t)((*output) >> 32), BIN);
  Serial.println((uint32_t)((*output) & 0x00000000FFFFFFFF), BIN);

  Serial.print("Card Format: ");
  Serial.println(card_format, BIN);
  Serial.println(card_format, DEC);

  Serial.print("Facility Code: ");
  Serial.println(facility_code, BIN);
  Serial.println(facility_code, DEC); 

  Serial.print("Card Number: ");
  Serial.println(card_number, BIN);
  Serial.println(card_number, DEC);

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

    // memset((void*)data_array, 0, ___ARR_SIZE___); // Reset data_array
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
  // digitalWrite(buzzer, B);
  if (B == 1) {
    tone(buzzer, 1000);
  } else {
    noTone(buzzer);
  }
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
  // John
  CreateNewUser(">_> uh oh", 199440);
  AddProfileToUser(199440, "Are you sure?");
  AddProfileToUser(199440, "No you're not");

  pinMode(button1, INPUT_PULLUP);  // Internal pull-up resistor
  pinMode(button2, INPUT_PULLUP);  // Internal pull-up resistor
  pinMode(doorSensor, INPUT);
  pinMode(LED, OUTPUT);  // Set analog pin A0 as digital output
  pinMode(buzzer, OUTPUT);
  pinMode(relay, OUTPUT);
  Serial.begin(9600);

  // Initialize serial communication
  Serial.println("FSK Decoder ONLINE:\n\r");

//  digitalWrite(relay, LOW);      // Start with relay OFF (door locked)

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
  
  // User data
  // Tony
  CreateNewUser("Tony Weidel", 201266);
  AddProfileToUser(201266, "Personal");
  AddProfileToUser(201266, "Work");
  AddProfileToUser(201266, "Other");
  AddProfileToUser(201266, "The other Tony");
  // Moody
  CreateNewUser("Moody Ekladios", 197319);
  // Juliana
  CreateNewUser("Juliana Batista", 201399);
  AddProfileToUser(201399, "Personal");
  AddProfileToUser(201399, "Ivan");
  AddProfileToUser(201399, "Bandit");
  // Julian
  CreateNewUser("Julain Porro", 104167);
  AddProfileToUser(104167, "JOOLIAN");

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

// uint32_t temp_card_number;
//bool selected = false;


void loop() {
  Serial.print('R');
  Serial.println(R, DEC);

  bool sensor = digitalRead(doorSensor);

  // Trigger only when state goes from LOW to HIGH
  if (sensor == 1) {
    display("Door is open", "Close the door");
    tone(buzzer, 1000);
    delay(1000);
  }
  else { // Operate the system
    WriteCommand(R);
    if (R == 1) { // READY
      display("RFID TRS Ready", "");
      UpdateOutputs();

      // Reset variables
      foundUser = 0;
      card_number = 0;
      c = 1;
      memset((void*)data_array, 0, ___ARR_SIZE___); // Reset data_array
      data_index = 0;
      while (card_number == 0) {
        // if (sensor == 1) {
        //  display("Door is open", "Close the door");
        //   tone(buzzer, 1000);
        //   delay(1000);
        // }
       cardRead();
      }
      Serial.println(card_number, DEC);
      
      if (card_number != 0) {
        // temp_card_number = card_number; // Store the card number before reset
        foundUser = FindUserByID(card_number); // Look up user immediately
        R = 2;
      }
    }
    else if (R == 2) { // HANDLE USER
      delay(1000);
      UpdateOutputs();
      
      if (foundUser == 0) {
        display(String(card_number), "Inactive User!");
        Serial.println("Inactive User!");
        delay(2000);
        R = 1;
        // Reset variables when returning to ready
        // foundUser = 0;
        // card_number = 0;
      } else {
        bool selected = false;
        unsigned long selectionStart = millis();
        const unsigned long maxSelectionDuration = 10000; // 10 seconds timeout
        
        if (U > 1) { // Only show selection if multiple profiles
          while (!selected && (millis() - selectionStart < maxSelectionDuration)) {
            display(userName, *profilePtr[c - 1]);
      
            if (buttonPushed(button1) && (millis() - buttonDebounceTime >= debounceDuration)) {
              selectionStart = millis();
              buttonDebounceTime = millis();
              c = (c % U) + 1; // Better cycling through profiles
            }
            if (buttonPushed(button2) && (millis() - buttonDebounceTime >= debounceDuration)) {
              buttonDebounceTime = millis();
              profIndex = c;
              selected = true;
              R = 3;
            }
            delay(50);
          }
          
          if (!selected) { // Timeout occurred
            R = 1;
          }
        } else { // Single profile - auto-login
          profIndex = 0;
          R = 3;
        }
      }
    }
    else if (R == 3) { // LOGIN
      display(userName, "Login");
      UpdateOutputs();
      delay(3000);
      R = 1; // Return to READY
    }
    else if (R == 4) { // LOGOUT
      display(userName, "Logout");
      UpdateOutputs();
      delay(3000);
      R = 1; // Return to READY
    }
  }
}
