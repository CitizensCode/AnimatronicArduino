#include <SoftEasyTransfer.h>
#include <SoftwareSerial.h>
#include <Servo.h>
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

// unique ID of the unit: choose a statue id from 1-5, corresponding to A-E in the script
#define STATUE_ID     1

// Statue position
#define statueX 90.0
#define statueY 164.0

// debug level for output verbosity
#define DEBUG             10
#define SIMULATE_AUDIO          0
#define SIMULATE_MOTOR          1

// control constants
#define   CMD_PLAY            0
#define   CMD_MOVE            1
#define   CMD_TRACK           2
#define   CMD_STOP            3

#define   TRACK_INACTIVE      0
#define   TRACK_ACTIVE        1

#define   STATUE_ID_INVALID  -1
#define   STATUE_ID_ALL       0

// pins for audio board and SD card
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)
#define CARDCS 4     // Card chip select pin
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin



int currentAngle = 0;
int destinationAngle = 0;
// Used to smooth the motion between positions. The closer to 
// 1, the faster the motion converges to its destination.
float smooth = 0.1;

Servo statueServo;

SoftwareSerial XBee =  SoftwareSerial(2, 3);

// Set up the audio card
// #define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
// #define SHIELD_CS     7      // VS1053 chip select pin (output)
// #define SHIELD_DCS    6      // VS1053 Data/command select pin (output)

// These are common pins between breakout and shield
// #define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
// #define DREQ 3       // VS1053 Data request, ideally an Interrupt pin

 Adafruit_VS1053_FilePlayer musicPlayer = 
   Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

// Set up easy transfer
// EasyTransfer ETin, ETout;
SoftEasyTransfer ETin;
struct RECEIVE_DATA_STRUCTURE{
  int unitId; // statue id
  int commandType; // command type...eg move, play, track, etc.
  int trackActive; // set the tracking state for the statue...if 0, then don't track. if 1, then auto track
  int xPos; // x-position
  int yPos; // y-position
  int scriptId; // numeric id of the script currently being played
  int audioId; // numeric id of the audio file to be played from the selected script
};
RECEIVE_DATA_STRUCTURE receiveData;

void setup() {
  Serial.begin(9600);

  Serial.println("Initializing SD card...");
  SD.begin(CARDCS);    // initialise the SD card
  Serial.println("SD card initialized.");
  
  XBee.begin(9600);
  ETin.begin(details(receiveData), &XBee);
  // ETout.begin(details(sendData), &Serial);

  statueServo.attach(9);

  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }

  musicPlayer.setVolume(10,10);
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
  Serial.println(F("Playing test file..."));
  musicPlayer.playFullFile("left.mp3");
  
  

  // If DREQ is on an interrupt pin (on uno, #2 or #3) we can do background
  // audio playing
  // musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int

  // musicPlayer.playFullFile("Left.mp3");
}


// Calculates where to position the motor to point at the object detected.
float motorPositionDeg(float xCoord, float yCoord, float motorX, float motorY) {
  float rad, deg;
  // atan 2 takes the arctan and finds which quadrant it is in
  // based on the x and y values provided
  rad = atan2((yCoord - motorY), (xCoord - motorX));
  // Return 'geometric' angle in degrees
  deg = rad * -180 / M_PI;
  // Because servos measure angle backwards for some reason, we have to
  // map the geometric angle to a servo angle
  deg = map(deg, 0, 180, 180, 0);
  // Serial.println(deg);
  return deg;
}


void loop() {
  // If we receive data, do something with it.
  Serial.println("...");
  if (ETin.receiveData()) {
   Serial.println("---------------");
   Serial.print("Statue: ");
   Serial.println(receiveData.unitId);
   Serial.print("Command: ");
   Serial.println(receiveData.commandType);
   Serial.print("X: ");
   Serial.print(receiveData.xPos);
   Serial.print(", Y: ");
   Serial.println(receiveData.yPos);

  // Convert the x,y coordinate to an angle
  destinationAngle = motorPositionDeg(receiveData.xPos, receiveData.yPos, statueX, statueY);

  // sendData.blink = 1;
  // ETout.sendData();

  // fileNumber = receiveData.audio;

  // if (fileNumber < 10) {
  //   filename = "0";
  //   filename.concat(fileNumber);
  //   filename.concat(".mp3");
  // } else {
  //   filename = "";
  //   filename.concat(fileNumber);
  //   filename.concat(".mp3");
  // }

  // Serial.println(filename);
  // filename.toCharArray(filename_char, 10);
  // // Serial.println(filename_char);
  // musicPlayer.startPlayingFile(filename_char);
  }

  // Gradually move the motor to its destination
  currentAngle = currentAngle * (1.0 - smooth) + destinationAngle * smooth;
  statueServo.write(currentAngle);
  delay(1000);
}
