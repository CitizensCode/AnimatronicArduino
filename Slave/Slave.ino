#include <EasyTransfer.h>
// #include <SoftwareSerial.h>
#include <Servo.h>
#include <SPI.h>
#include <ServoEaser.h>
#include <Metro.h>
// #include <Adafruit_VS1053.h>
// #include <SD.h>

// The unique ID of the unit
int statueId = 1;
// Statue position
#define statueX 50.0
#define statueY 160.0
#define offset 45.0

int currentAngle = 0;
int destinationAngle = 0;

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


// SoftwareSerial XBee =  SoftwareSerial(2, 3);

// Set up the audio card
// #define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
// #define SHIELD_CS     7      // VS1053 chip select pin (output)
// #define SHIELD_DCS    6      // VS1053 Data/command select pin (output)

// These are common pins between breakout and shield
// #define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
// #define DREQ 3       // VS1053 Data request, ideally an Interrupt pin

// Adafruit_VS1053_FilePlayer musicPlayer = 
//   // create breakout-example object!
//   //Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, CARDCS);
//   // create shield-example object!
//   Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

// Set up easy transfer
// EasyTransfer ETin, ETout;
EasyTransfer ETin;
struct RECEIVE_DATA_STRUCTURE{
  int unitId;
  int commandType;  
  int xPos;
  int yPos;
  // int audio;
};
RECEIVE_DATA_STRUCTURE receiveData;

// How often to receive data
Metro getData = Metro(10);


// struct SEND_DATA_STRUCTURE{
//   int blink;
// };
// SEND_DATA_STRUCTURE sendData;

void setup() {
  Serial.begin(9600);
  // XBee.begin(9600);
  ETin.begin(details(receiveData), &Serial);
  // ETout.begin(details(sendData), &Serial);

  statueServo.attach(9);
  servoEaser.begin( statueServo, servoFrameMillis);
  servoEaser.useMicroseconds(true);

  if (statueId == 1 || statueId == 2) {
    servoEaser.setEasingFunc(easeInOutQuad);
    durationLower = 500;
    durationUpper = 1200;
  } else if (statueId == 3) {
    servoEaser.setEasingFunc(easeInOutCubic);
    durationLower = 800;
    durationUpper = 1500;
  } else {
    servoEaser.setEasingFunc(easeInOutQuart);
    durationLower = 1000;
    durationUpper = 2000;
  }
  

  servoEaser.easeTo( 90, 1000);

  // if (! musicPlayer.begin()) { // initialise the music player
  //    // Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
  //    while (1);
  // }
  
  // SD.begin(CARDCS);    // initialise the SD card

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
  // If we receive data, do something with it.
  if (ETin.receiveData() && getData.check() == 1) {
   // Serial.println("---------------");
   // Serial.print("Statue: ");
   // Serial.println(receiveData.unitId);
   // Serial.print("Command: ");
   // Serial.println(receiveData.commandType);
   // Serial.print("X: ");
   // Serial.print(receiveData.xPos);
   // Serial.print(", Y: ");
   // Serial.println(receiveData.yPos);

  // Convert the x,y coordinate to an angle
  destinationAngle = motorPositionDeg(receiveData.xPos, receiveData.yPos, statueX, statueY);
  Serial.println(destinationAngle);

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

  // Moved the servo a step toward its destination using the specified
  // easing function
  servoEaser.update();

  // Only reverse direction if the servo has already reached its initial destination
  if( servoEaser.hasArrived() ) {
    // Find out where the servo is currently pointed
    int currPos = servoEaser.getCurrPos();
    // Provides a variable rotation duration so that long distances aren't covered
    // in the same amount of time as short distances
    int duration = int(float(abs(destinationAngle - currPos)) / servSpeed * 1000);
    // Constrains the duration to reasonable bounds
    duration = constrain(duration, durationLower, durationUpper);
    Serial.println(duration);
    
    servoEaser.easeTo( destinationAngle, duration );
  }
}
