#include <EasyTransfer.h>
//#include <SoftEasyTransfer.h>
#include <SoftwareSerial.h>
#include <Servo.h>
#include <SPI.h>
#include <ServoEaser.h>
#include <Metro.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

// debug level for output verbosity
#define DEBUG                   0

// STATUE_ID: unique ID of the unit
//    choose a statue id from 1-5, corresponding to A-E in the script
// statueX: x-position of base of the unit
// statueY: y-position of base of the unit

// Statue position A / 1
#define STATUE_ID     1
#define statueX      40
#define statueY      84
#define offset       45.0

//// Statue position B / 2
//#define STATUE_ID     2
//#define statueX      80
//#define statueY     143
//#define offset       45.0

//// Statue position C / 3
//#define STATUE_ID     3
//#define statueX     134
//#define statueY     177
//#define offset       45.0

//// Statue position D / 4
//#define STATUE_ID     4
//#define statueX     202
//#define statueY     183
//#define offset       45.0

//// Statue position E / 5
//#define STATUE_ID     5
//#define statueX     265
//#define statueY      95
//#define offset      -45.0




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



int trackActive = 1;
int currentAngle = 0;
int destinationAngle = 0;
int trackDestinationAngle = 0;
int moveDestinationAngle = 0;
// Used to smooth the motion between positions. The closer to 
// 1, the faster the motion converges to its destination.
float smooth = 0.1;

Servo statueServo;
ServoEaser servoEaser;
int servoFrameMillis = 20;  // minimum time between servo updates
const int servSpeed = 60.0; // Degrees per second
// Initial movement duration
int duration = 1000; 
// The upper and lower bound for how long a movement should take
int durationLower;
int durationUpper;

//////// A bunch of easing functions //////

// from Easing::easeInOutQuad()
float easeInOutQuad (float t, float b, float c, float d) {
  if ((t/=d/2) < 1) return c/2*t*t + b;
  return -c/2 * ((--t)*(t-2) - 1) + b;
}
// from Easing::easeInOutCubic()
float easeInOutCubic (float t, float b, float c, float d) {
  if ((t/=d/2) < 1) return c/2*t*t*t + b;
  return c/2*((t-=2)*t*t + 2) + b;
}
// from Easing::easeInOutQuart()
float easeInOutQuart (float t, float b, float c, float d) {
  if ((t/=d/2) < 1) return c/2*t*t*t*t + b;
  return -c/2 * ((t-=2)*t*t*t - 2) + b;
}


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

String audiofilename;
char filename_char[20];
char scriptOrder[] = "pzafec";

// Set up easy transfer
EasyTransfer ETin;
//SoftEasyTransfer ETin;
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

  if (DEBUG) {
    Serial.println("Initializing SD card...");
  }
  SD.begin(CARDCS);    // initialise the SD card
  if (DEBUG) {
    Serial.println("SD card initialized.");
  }
  
//  XBee.begin(9600);
//  ETin.begin(details(receiveData), &XBee);
  ETin.begin(details(receiveData), &Serial);
  // ETout.begin(details(sendData), &Serial);

  statueServo.attach(9);
  servoEaser.begin( statueServo, servoFrameMillis);
  servoEaser.useMicroseconds(true);

  if (STATUE_ID == 1 || STATUE_ID == 2) {
    servoEaser.setEasingFunc(easeInOutQuad);
    durationLower = 500;
    durationUpper = 1200;
  } else if (STATUE_ID == 3) {
    servoEaser.setEasingFunc(easeInOutCubic);
    durationLower = 800;
    durationUpper = 1500;
  } else {
    servoEaser.setEasingFunc(easeInOutQuart);
    durationLower = 1000;
    durationUpper = 2000;
  }
  

  servoEaser.easeTo( 90, 1000);

  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }

  musicPlayer.setVolume(10,10);
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
  if (DEBUG) {
    Serial.println(F("Playing test file..."));
    musicPlayer.playFullFile("left.mp3");
  }
  
  
  

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
  deg = constrain(deg - offset, 0, 180);
  // Serial.println(deg);
  return deg;
}


void loop() {
  if (DEBUG > 4) {
    Serial.println("...");
  }

  // If we receive data, do something with it.
  if (ETin.receiveData()) {
    if (DEBUG > 4) {
      Serial.println("---------------");
      Serial.print("Statue: ");
      Serial.println(receiveData.unitId);
      Serial.print("Command: ");
      Serial.println(receiveData.commandType);
      Serial.print("X: ");
      Serial.print(receiveData.xPos);
      Serial.print(", Y: ");
      Serial.println(receiveData.yPos);
      Serial.print("Track Active: ");
      Serial.println(receiveData.trackActive);
      Serial.print("ScriptId: ");
      Serial.println(receiveData.scriptId);
      Serial.print("AudioId: ");
      Serial.println(receiveData.audioId);
    }

    // only handle commands directed to this statue
    if (receiveData.unitId == STATUE_ID || receiveData.unitId == STATUE_ID_ALL) {
      // handle each command type
      switch (receiveData.commandType) {
        case CMD_PLAY:
          if (DEBUG > 1) {
            Serial.println("HANDLING PLAY COMMAND");
          }

          musicPlayer.stopPlaying();

          audiofilename = "";
          audiofilename.concat(scriptOrder[receiveData.scriptId]);
          audiofilename.concat("/");
          audiofilename.concat(receiveData.audioId);
          audiofilename.concat(".mp3");

          audiofilename.toCharArray(filename_char, 20);
          
          musicPlayer.startPlayingFile(filename_char);

          break;

        case CMD_MOVE:
          if (DEBUG > 1) {
            Serial.println("HANDLING MOVE COMMAND");
          }

          // set tracking state
          trackActive = receiveData.trackActive;
          
          // Convert the x,y coordinate to an angle
          moveDestinationAngle = motorPositionDeg(receiveData.xPos, receiveData.yPos, statueX, statueY);

          break;

        case CMD_TRACK:
          // Convert the x,y coordinate to an angle
          trackDestinationAngle = motorPositionDeg(receiveData.xPos, receiveData.yPos, statueX, statueY);
          
          break;

        case CMD_STOP:

          musicPlayer.stopPlaying();
          trackActive = TRACK_ACTIVE;

          break;
      }
    }
    

    
  }


  // Moved the servo a step toward its destination using the specified
  // easing function
  servoEaser.update();

  // Only reverse direction if the servo has already reached its initial destination
  if (servoEaser.hasArrived()) {

    if (trackActive == TRACK_ACTIVE) {
      // tracking move
      destinationAngle = trackDestinationAngle;
    }
    else if (trackActive == TRACK_INACTIVE) {
      // fixed mode
      destinationAngle = moveDestinationAngle;
    }

    // Find out where the servo is currently pointed
    int currPos = servoEaser.getCurrPos();

    // Provides a variable rotation duration so that long distances aren't covered
    // in the same amount of time as short distances
    int duration = int(float(abs(destinationAngle - currPos)) / servSpeed * 1000);

    // Constrains the duration to reasonable bounds
    duration = constrain(duration, durationLower, durationUpper);
    Serial.println(duration);
    
    servoEaser.easeTo(destinationAngle, duration);
  }

}

// END PROGRAM
