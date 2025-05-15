#include <stdio.h>
#include <string.h>

// Global Variables
#define MAX_NAME_LEN 16
#define MAX_PROFILES 4

#define MESSAGE_LENGTH 39
uint8_t messageBuffer[MESSAGE_LENGTH];  // Global buffer to store the message

// Communication Commands
uint8_t R; // Status (0-4; off, Ready, User, Login, Logout)
uint8_t U; // Number of user's profiles (0-n)
uint8_t P; // Profile number (n)
bool Y; // Door Relay OFF/ON
bool S; // SHD OFF/ON
bool D; // R/G LEDs RED/GREEN
bool B; // Buzzer OFF/ON

// Define the Profile struct
typedef struct {
    char profileName[MAX_NAME_LEN];
    uint8_t profileIndex; // To keep track of how many profiles are used
    bool logged = 0;
} Profile;

// Define the User struct
typedef struct {
    char userName[MAX_NAME_LEN];
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
void CreateNewUser(const char* name, uint32_t id) {
    static int userIndex = 0;  // Keeps track of how many users have been added

    // Initialize the new user
    strncpy(users[userIndex].userName, name, MAX_NAME_LEN);
    users[userIndex].userName[MAX_NAME_LEN - 1] = '\0';  // Ensure null-termination

    users[userIndex].userID = id;
    users[userIndex].logged = false;

    // Initialize their profiles
    for (size_t i = 0; i < MAX_PROFILES; i++) {
        users[userIndex].profiles[i].profileName[0] = '\0';  // Empty string
        users[userIndex].profiles[i].profileIndex = 0;
    }

    userIndex++;  // Move to the next available slot for future users
}

// Add profile to user
void AddProfileToUser(uint32_t userID, const char* profileName) {
    for (size_t i = 0; i < MAX_USERS; i++) {
        if (users[i].userID == userID) {
            // Add profile to the next available slot
            for (size_t j = 0; j < MAX_PROFILES; j++) {
                if (users[i].profiles[j].profileName[0] == '\0') {
                    strncpy(users[i].profiles[j].profileName, profileName, MAX_NAME_LEN);
                    users[i].profiles[j].profileName[MAX_NAME_LEN - 1] = '\0';  // Ensure null-terminated
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
void SerialWriteCommand(uint8_t &R) {
  // Serial.println(R, DEC);
  if (R == 0) { // off
    Y = 0;  S = 1;  D = 0;  B = 0; // Set pins
  }
  else if (R == 1) { // Ready
    Y = 0;  S = 0;  D = 0;  B = 0; // Set pins
  }
  else if (R == 2) { // User
    // Look for user
    // foundUser = FindUserByID(ID); // Handled in loop()
    if (foundUser != 0) { // If user was found
      if (!foundUser->logged) { // If user is not logged
        if (foundUser->profCount > 1) { // If user has profiles
          // Send number of users
          Serial.write(254);
          Serial.write('U');
          Serial.write(foundUser->profCount);
          
          Serial.write(254);
          Serial.write('X');
          // Send user name
          for (size_t i = 0; foundUser->userName[i] != '\0'; i++) {
            Serial.write((uint8_t)foundUser->userName[i]);
          }
          // Send user profile names        
          for (size_t i = 0; i < foundUser->profCount; i++) {
            Serial.write(254);
            Serial.write('P');
            for (size_t j = 0; foundUser->profiles[i].profileName[j] != '\0'; j++) {
              Serial.write((uint8_t)foundUser->profiles[i].profileName[j]);
            }
          }
        }
          else { // login directly
          R = 3;
          SerialWriteCommand(R); // Login
          return; // Exit loop
        }
      } else { // User is already loggedin
        R = 4;
        SerialWriteCommand(R); // Logout
        return; // Exit loop
      }
    } else { // User not found
      // Send 255 to indicate User not available
      Serial.write(254);
      Serial.write('U');
      Serial.write(255); // User does not exists
    }
    Y = 0;  S = 1;  D = 0;  B = 0; // Set pins
  }
  else if (R == 3) { // Login
    // Login user and its profile
    if (!foundUser->logged) { // If user is not logged
      foundUser->logged = true; // Login user
      if (foundUser->profCount > 1) { // If there is multiple profiles
        // Serial.print("!");
        // Serial.print(profIndex, DEC);
        foundUser->profiles[profIndex - 1].logged = true; // Login profile
        // Serial.print(profIndex - 1, DEC);
        Serial.write(254);
        Serial.write('X');
        // Send profile name for confirmation
        // Serial.print("!!");
        // Serial.print(foundUser->profiles[profIndex - 1].profileName);
        for (size_t j = 0; foundUser->profiles[profIndex - 1].profileName[j] != '\0'; j++) {
            Serial.write((uint8_t)foundUser->profiles[profIndex - 1].profileName[j]);
        }
      }
      else { // No profiles or one profile exists
        Serial.write(254);
        Serial.write('X');
        if (foundUser->profCount == 1) {
          foundUser->profiles[0].logged = true; // Login profile
          // Send profile name
          for (size_t i = 0; foundUser->profiles[0].profileName[i] != '\0'; i++) {
            Serial.write((uint8_t)foundUser->profiles[0].profileName[i]);
          }
        }
        else {
          // Send user name
          for (size_t i = 0; foundUser->userName[i] != '\0'; i++) {
            Serial.write((uint8_t)foundUser->userName[i]);
          }
        }
      }
    } else { // User is already loggedin
        R = 4;
        SerialWriteCommand(R); // Logout
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
          Serial.write(254);
          Serial.write('X');
          // Send profile name for confirmation
          // Serial.print("!!");
          // Serial.print(foundUser->profiles[i].profileName);
          for (size_t j = 0; foundUser->profiles[i].profileName[j] != '\0'; j++) {
            Serial.write((uint8_t)foundUser->profiles[i].profileName[j]);
          }
        }
      }
    }
    else { // No profiles exists
      Serial.write(254);
      Serial.write('X');
      // Send user name
      for (size_t i = 0; foundUser->userName[i] != '\0'; i++) {
        Serial.write((uint8_t)foundUser->userName[i]);
      }
    }
    Y = 0;  S = 1;  D = 0;  B = 0; // Set pins
  }
  else return; // Invalid status (Do not write)

  // R
  Serial.write(254);
  Serial.write('R');
  Serial.write(R);

  // Y
  Serial.write(254);
  Serial.write('Y');
  Serial.write(Y);

  // S
  Serial.write(254);
  Serial.write('S');
  Serial.write(S);

  // D
  Serial.write(254);
  Serial.write('D');
  Serial.write(D);

  // B
  Serial.write(254);
  Serial.write('B');
  Serial.write(B);

  // Serial.print('R');
  // Serial.print(R, DEC);
  // Serial.println("\n");
  // Serial.print('U');
  // Serial.print(foundUser->profCount, DEC);
  // Serial.print('X');
  // Serial.print(foundUser->userName);
  // Serial.print('P');
  // Serial.print(foundUser->profiles[0].profileName);
  // Serial.print('P');
  // Serial.print(foundUser->profiles[1].profileName);
  // Serial.print('P');
  // Serial.print(foundUser->profiles[2].profileName);
  // Serial.print('P');
  // Serial.print(foundUser->profiles[3].profileName);
  // Serial.print('Y');
  // Serial.print(Y, DEC);
  // Serial.print('S');
  // Serial.print(S, DEC);
  // Serial.print('D');
  // Serial.print(D, DEC);
  // Serial.print('B');
  // Serial.println(B, DEC);
}

// Read 12 bytes message and store it into an array of chars
void ReadSerialBytes() {
  static uint8_t index = 0;
  static unsigned long lastByteTime = 0;
  const unsigned long TIMEOUT = 5;  // ms

  // Reset user's variables
  ID = 0;
  profIndex = 0;

  // Read incoming bytes
  while (Serial.available() > 0) {
    uint8_t incoming = Serial.read();

    // Ensure first three bytes are correct
    if (index == 0 && incoming != 0x01) continue;
    if (index == 1 && incoming != 0x09) {
      index = 0;
      continue;
    }
    if (index == 2 && incoming != 0x0C) {
      index = 0;
      continue;
    }

    // Store byte if valid
    if (index < MESSAGE_LENGTH) {
      messageBuffer[index++] = incoming;
      lastByteTime = millis();
    }

    // Prevent reading extra bytes
    if (index >= MESSAGE_LENGTH) break;
  }

  // Process if full message received and timeout passed
  if (index == MESSAGE_LENGTH && millis() - lastByteTime > TIMEOUT) {
    // Serial.println("Valid 12-byte message received:");
    // for (size_t i = 0; i < MESSAGE_LENGTH; i++) {
    //   if (messageBuffer[i] < 0x10) // Serial.print("0");
    //   Serial.print(messageBuffer[i], DEC);
    //   Serial.print(" ");
    // }
    Serial.println();

    ParseUserData(ID, profIndex); // Update user's variables
    index = 0;  // Reset for next message
  }
}

// Clear any byte array
// void ClearByteArray(uint8_t* array, size_t length) {
//   for (size_t i = 0; i < length; i++) {
//     array[i] = 0;
//   }
// }

// Write message stored in an array of bytes
// void SendMessage(const uint8_t* data, size_t length) {
//   for (size_t i = 0; i < length; i++) {
//     Serial.write(data[i]);
//   }
// }

void setup() {
  Serial.begin(9600);

  CreateNewUser("NCF", 197319);
  CreateNewUser("NCE", 201260);
  // AddProfileToUser(197319, "396");
  // AddProfileToUser(197319, "397");
  // AddProfileToUser(197319, "398");
  // AddProfileToUser(197319, "399");
  //AddProfileToUser(201266, "400");
  // foundUser = FindUserByID(201266);
  // Serial.print("1."); 
  // SerialWriteCommand(2); // Login 396
  // foundUser = FindUserByID(201266);
  // // Serial.print("2.");
  // SerialWriteCommand(2); // Show NCF
  // profIndex = 5;
  // // Serial.print("3.");
  // SerialWriteCommand(3); // Login 397
  // foundUser = FindUserByID(201260);
  // // Serial.print("4."); 
  // SerialWriteCommand(2); // Logout NCE
  // foundUser = FindUserByID(201266);
  // // Serial.print("5."); 
  // SerialWriteCommand(2); // Logout 397
  // foundUser = FindUserByID(197319);
  // profIndex = 3;
  // R = 3;
  // SerialWriteCommand(R); // State 1: TRS Ready
  // // R = 4;
  // SerialWriteCommand(R);
}

// millis variables
static unsigned long lastReadyTime = 0;
unsigned long waitStartTime;
static const unsigned long TIMEOUT = 1000000; // 60 seconds
static bool waitingForData = true;

void loop() {
  unsigned long currentTime = millis();

  // If we’re waiting for data and timeout has passed, send READY state again
  // if (waitingForData && (currentTime - lastReadyTime >= TIMEOUT)) {
  //   R = 1;
  //   SerialWriteCommand(R); // State 1: TRS Ready
  //   lastReadyTime = currentTime;
  // }

  // Wait for new incoming serial data a maximum of 10 seconds
  waitStartTime = millis();
  bool dataReceived = false;

  while ((millis() - waitStartTime < TIMEOUT)) {
    ReadSerialBytes();  // This will only act if Serial.available() > 0

    if (ID != 0) {  // As soon as we get valid data, stop waiting
      dataReceived = true;
      break;
    }

    // delay(10);  // avoid tight-looping
  }

  if (dataReceived) {
    // Serial.println(millis());
    foundUser = FindUserByID(ID);
    // delay(1000);
    if (profIndex == 0) {
      R = 2;
      SerialWriteCommand(R); // State 2: Handle User
    } else if (profIndex > 0 && profIndex < 5) {
      R = 3;
      SerialWriteCommand(R); // State 3: Login
    }
    // Serial.println(millis());

    // Reset ID to 0 so we don’t process the same message again
    ID = 0;
    waitingForData = true;
    lastReadyTime = millis();
  }
  R = 1;
}

