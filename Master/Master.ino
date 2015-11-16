// Serial port setup

#include <SoftEasyTransfer.h>
#include <SoftwareSerial.h>
#include <Metro.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_VS1053.h>

// debug level for output verbosity
#define DEBUG             10
#define SIMULATE_AUDIO          0
#define SIMULATE_MOTOR          0

#define SCRIPT_INTERMISSION_MS    10000
#define POST_STOP_DELAY_MS          500

// control constants
#define   CMD_PLAY            0
#define   CMD_MOVE            1
#define   CMD_TRACK           2
#define   CMD_STOP            3

#define   TRACK_INACTIVE      0
#define   TRACK_ACTIVE        1

#define   STATUE_ID_INVALID  -1
#define   STATUE_ID_ALL       0
#define   STATUE_ID_1         1
#define   STATUE_ID_2         2
#define   STATUE_ID_3         3
#define   STATUE_ID_4         4
#define   STATUE_ID_5         5

// pins for audio board and SD card
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)
#define CARDCS 4     // Card chip select pin
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin

// max length for script line
#define MAX_LINE_LENGTH 128
// max length for script argument
#define MAX_ARG_LENGTH 32

// script file status
#define SCRIPT_NOT_OPEN   0
#define SCRIPT_OPEN       1
#define SCRIPT_EOF        2

Adafruit_VS1053_FilePlayer musicPlayer = 
  Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

unsigned long ref_time = millis();
unsigned long delay_time = 0;
File scriptFile;

char line[MAX_LINE_LENGTH];
char statue_id[MAX_ARG_LENGTH];
char command[MAX_ARG_LENGTH];
char command_args[MAX_ARG_LENGTH];
char pause[MAX_ARG_LENGTH];

bool runningScript = false;
int scriptStatus = SCRIPT_NOT_OPEN;
bool parseLine = false;
bool scriptFinished = true;

String audiofilename;
char filename_char[10];


void terminate_arg_string(int arg_id, int arg_index, char statue_id[], char command[], char command_args[],  char pause[]) {
  // terminate arg with null
  if (arg_id == 0) {
    // statue_id
    statue_id[arg_index] = 0x0;
  }
  else if (arg_id == 1) {
    // command
    command[arg_index] = 0x0;
  }
  else if (arg_id == 2) {
    // command args
    command_args[arg_index] = 0x0;
  }
  else if (arg_id == 3) {
    // pause before next command
    pause[arg_index] = 0x0;
  }
}

void assign_character_to_arg(char c, int arg_id, int arg_index, char statue_id[], char command[], char command_args[],  char pause[]) {
  // save character into appropriate arg
  if (arg_id == 0) {
    // statue_id
    statue_id[arg_index] = c;
  }
  else if (arg_id == 1) {
    // command
    command[arg_index] = c;
  }
  else if (arg_id == 2) {
    // command args
    command_args[arg_index] = c;
  }
  else if (arg_id == 3) {
    // pause before next command
    pause[arg_index] = c;
  }
}

void parse_line_args(char line[], char statue_id[], char command[], char command_args[],  char pause[]) {
  int i = 0;
  int arg_index = 0;
  int arg_id = 0;  
  
  char c = line[i];
  while (c != 0x0) {
    if (c == 0x3A) { // arg delimiter (colon)
      terminate_arg_string(arg_id, arg_index, statue_id, command, command_args, pause);

      arg_index = 0;
      arg_id++;
    }
    else {
      assign_character_to_arg(c, arg_id, arg_index, statue_id, command, command_args, pause);
      arg_index++;
    }
    
    i++;
    c = line[i];
  }

  terminate_arg_string(arg_id, arg_index, statue_id, command, command_args, pause);
}

void parse_command_args(char command_args[], int *xPos, int *yPos, int *trackActive) {
  int i = 0;
  int arg_index = 0;
  int arg_id = 0;
  
  char xPosString[MAX_ARG_LENGTH];
  char yPosString[MAX_ARG_LENGTH];

  char c = command_args[i];

  if (String(c).equals("j") || String(c).equals("J")) {
    *trackActive = 1;
  }
  else {
    *trackActive = 0;
  }
  
  while (1) {
    if (c == 0x2C || c == 0x0) { // arg delimiter (comma or null character)
      if (arg_id == 0) {
        xPosString[arg_index] = 0x0;
      }
      else if (arg_id == 1) {
        yPosString[arg_index] = 0x0;
      }
      else {
        Serial.println("INVALID arg_id in parse_command_args!! (a)");
      }
      
      arg_index = 0;
      arg_id++;

      if (c == 0x0) {
        break;
      }
    }
    else { // regular character
      if (arg_id == 0) {
        xPosString[arg_index] = c;
      }
      else if (arg_id == 1) {
        yPosString[arg_index] = c;
      }
      else {
        Serial.println("INVALID arg_id in parse_command_args!! (b)");
      }
      
      arg_index++;
    }
    
    i++;
    c = command_args[i];
  }

  (*xPos) = String(xPosString).toInt();
  (*yPos) = String(yPosString).toInt();
}

int readline(File scriptFile, char line[]) {
  if (scriptFile && scriptFile.available()) {

    int i = 0;
    char c = scriptFile.read();
    while (1) {

      if (c == 0xA || c == EOF) {
        // end of line, parse and save
        line[i] = 0x0;
        i = 0;

        break;
      }
      else {
        line[i] = (char) c;
        i++;
      }
      
      c = scriptFile.read();
      
    }

    if (c == EOF) {
      return SCRIPT_EOF;
    }
    else {
      return SCRIPT_OPEN;
    }
  }
  else {
    return SCRIPT_NOT_OPEN;
  }
}

int statueIdStringToInt(char statue_id[]) {
  String statueStringId = String(statue_id);
  if (statueStringId.equals("A")) {
    return STATUE_ID_1;
  }
  else if (statueStringId.equals("B")) {
    return STATUE_ID_2;
  }
  else if (statueStringId.equals("C")) {
    return STATUE_ID_3;
  }
  else if (statueStringId.equals("D")) {
    return STATUE_ID_4;
  }
  else if (statueStringId.equals("E")) {
    return STATUE_ID_5;
  }
  else {
    Serial.print("Received invalid statue ID string: ");
    Serial.println(statue_id);
    return STATUE_ID_INVALID;
  }
}

SoftwareSerial XBee =  SoftwareSerial(2, 3);

// SoftEasyTransfer ETin, ETout; 
SoftEasyTransfer ETout;

// Used for scheduling distance readings and communications
Metro getDistance = Metro(100); // Take a reading every 100ms
Metro sendCoords = Metro(150);  // Send the result every 250ms


// Set of variables for choosing which file to play
//int currentFileNumber = 99;
//int fileNumber = 0;
//bool changed = true;

// Distance between the range finders
#define distRF 190.0
#define rangeA 7 // RF A
#define rangeB 8 // RF B
long pulseA, pulseB;

// Set up variables needed to smooth readings
float lastAngle = 90; // Used to prevent erratic readings
double angle;
// Set up variables for smoothing the stepper motion
const int numReadings  = 3; // Number of readings to average
int readingIndex = 0; // Index of current reading
float readings[numReadings]; // Array for holding readings
float readingsTotal = 0.0; // Running total
float smoothedAngle = 0.0;

// Set up variables for triangulation
float xCoord, yCoord;

#define NUM_STATUE 2
int statues[] = {1, 2};

struct SEND_DATA_STRUCTURE{
  int unitId; // statue id
  int commandType; // command type...eg move, play, track, etc.
  int trackActive; // set the tracking state for the statue...if 0, then don't track. if 1, then auto track
  int xPos; // x-position
  int yPos; // y-position
  int scriptId; // numeric id of the script currently being played
  int audioId; // numeric id of the audio file to be played from the selected script
};
SEND_DATA_STRUCTURE sendData;

// struct RECEIVE_DATA_STRUCTURE{
//   int blink;
// };
// RECEIVE_DATA_STRUCTURE receiveData;

unsigned int timestamp;

void setup() {  
  Serial.begin(9600);
  Serial.println("Testing Up!");

  Serial.println("Initializing SD card...");
  SD.begin(CARDCS);    // initialise the SD card
  Serial.println("SD card initialized.");

  ref_time = millis();

  if (SIMULATE_AUDIO) {
    if (! musicPlayer.begin()) { // initialise the music player
       Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
       while (1);
    }
    Serial.println(F("VS1053 found"));

    musicPlayer.setVolume(10,10);
    musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
    Serial.println(F("Playing test file..."));
    musicPlayer.playFullFile("left.mp3");
  }



  // Set up the pins for the ultrasonic sensors
  pinMode(rangeA, INPUT);
  pinMode(rangeB, INPUT);
  // set the data rate for the SoftwareSerial port
  XBee.begin(9600);

  ETout.begin(details(sendData), &XBee);
  // ETin.begin(details(receiveData), &XBee);

  // Set up the smoothing array by making it all 0s
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
    }
  }

// Calculates the angle of the tracked object relative to the first sensor
float angleCalc(float distA, float distB) {
  float angleRad;
  // Law of Cosines rearranged
  angleRad = acos((distA * distA + distRF * distRF - distB * distB) / (2 * distA * distRF));

  // Prevent erratic readings
  if (isnan(angleRad) == 1) {
    return lastAngle;
  } else {
    lastAngle = angleRad;
    return angleRad;
  }
}

void loop() {
  if (DEBUG > 10) {
    Serial.println("  Entering loop iteration...");
  }
  
  if (runningScript == false) {
    // only start a script if delay_time has elapsed (initially 0, later set to script intermission time)
    if (millis() - ref_time >= delay_time) {
      Serial.println("*****************************");
      Serial.println("         [ START ]");
      Serial.println("*****************************");

      sendData.unitId = STATUE_ID_ALL;
      sendData.commandType = CMD_STOP;
      ETout.sendData();

      if (DEBUG > 4) {
        Serial.print("STOP ALL SIGNAL SENT");
      }

      delay(POST_STOP_DELAY_MS);
      
      scriptFile = SD.open("z/script.txt"); // TODO: edit to support multiple scripts
      runningScript = true;
      scriptFinished = false;
      delay_time = 0;
      ref_time = millis();
    }
  }
  else {
  
    // only play the next line of the script if delay_time has elapsed
    if (millis() - ref_time >= delay_time) {
      scriptStatus = readline(scriptFile, line);
  
      switch (scriptStatus) {
        case SCRIPT_NOT_OPEN:
          parseLine = false;
          scriptFinished = true;
          break;
    
        case SCRIPT_OPEN:
          parseLine = true;
          break;
    
        case SCRIPT_EOF:
          parseLine = true;
          scriptFinished = true;
          break;
      }
    
      if (parseLine == true) {
        parse_line_args(line, statue_id, command, command_args, pause);
    
        if (DEBUG) {
          Serial.println(line);
        }
        if (DEBUG > 1) {
          Serial.print("  "); Serial.println(statue_id);
          Serial.print("  "); Serial.println(command);
          Serial.print("  "); Serial.println(command_args);
          Serial.print("  "); Serial.println(pause);
          Serial.println();
        }
    
        if (String(command).equals("MOVE")) {
          if (DEBUG > 1) {
            Serial.println("HANDLING MOVE COMMAND");
          }

          if (!SIMULATE_MOTOR) {

            // send move signal
            sendData.unitId = statueIdStringToInt(statue_id);
            sendData.commandType = CMD_MOVE;

            // parse out move coordinates
            parse_command_args(command_args, &(sendData.xPos), &(sendData.yPos), &(sendData.trackActive));

            if (DEBUG > 4) {
              Serial.print("Move position: ");
              Serial.print(sendData.xPos);
              Serial.print(", ");
              Serial.println(sendData.yPos);
            }
            
            ETout.sendData();
          }
        }
        else if (String(command).equals("PLAY")) {
          if (DEBUG > 1) {
            Serial.println("HANDLING PLAY COMMAND");
          }

          if (SIMULATE_AUDIO) {
            // TODO: edit to support multiple scripts
            audiofilename = "z/";
            audiofilename.concat(command_args);
            audiofilename.concat(".mp3");
            audiofilename.toCharArray(filename_char, 10);
            musicPlayer.stopPlaying();
            musicPlayer.startPlayingFile(filename_char);
          }
          else {
            sendData.unitId = statueIdStringToInt(statue_id);
            sendData.commandType = CMD_PLAY;
            sendData.scriptId = 0; // TODO: edit to support multiple scripts
            sendData.audioId = String(command_args).toInt();
      
            if (DEBUG > 4) {
              Serial.print("Play audio: ");
              Serial.print(sendData.scriptId);
              Serial.print(", ");
              Serial.println(sendData.audioId);
            }
            
            ETout.sendData();
          }
        }
  
        delay_time = (unsigned long) String(pause).toInt();
        ref_time = millis();
        if (DEBUG > 1) {
          Serial.print("delaying ");
          Serial.print(delay_time);
          Serial.print(" ms before next script line...\n");
        }
      }
    
      if (DEBUG) {
        Serial.println();
        Serial.println();
      }
    }
    
    
  
    // Calculate the distance reading at the interval specified above
    timestamp = millis();
    if (getDistance.check() == 1) {


      if (!SIMULATE_MOTOR) {
        // Get the distance from sensor A and B
        // THIS IS AMAZINGLY SLOW IF DISTANCE SENSORS AREN'T HOOKED UP
        pulseA = pulseIn(rangeA, HIGH);
        pulseA = pulseA / 10.0; // 10 uS per cm
        pulseB = pulseIn(rangeB, HIGH);
        pulseB = pulseB / 10.0;
      }
      else {
        // cheap hack to speed things up while we don't have distance sensors
        pulseA = 0;
        pulseB = 0;
      }

      // Get the angle relative to the first sensor
      angle = angleCalc(pulseA, pulseB);
      // Serial.println(angle * 180 / 3.14159);
  
      // Smooth the movement
      readingsTotal = readingsTotal - readings[readingIndex]; // Subtract the old
      readings[readingIndex] = angle; // Insert the new
      readingsTotal = readingsTotal + readings[readingIndex]; // Add the new
      readingIndex = readingIndex + 1; // Move to the next index
      
      // Loop around the array
      if (readingIndex >= numReadings) {
        readingIndex = 0; // Start the index back at 0
      }
      
      // Take the average of the last three readings
      smoothedAngle = readingsTotal / (float)numReadings;
      // Find the x-coordinate and y-coordinate of the person. All y values are
      // negative because of the coordinate system used relative to the position
      // of the sensors.
      xCoord = pulseA * cos(smoothedAngle);
      yCoord = pulseA * sin(smoothedAngle);
    }
    Serial.print("getDistance elapsed time: "); Serial.println(millis() - timestamp);
  
  //  Serial.println("LOL");
  
    // Send the smoothed distance reading at the rate specified above
    timestamp = millis();
    if (sendCoords.check() == 1) {
      sendData.unitId = STATUE_ID_ALL;
      sendData.commandType = CMD_TRACK;
      sendData.xPos = xCoord;
      sendData.yPos = yCoord;

      if (DEBUG > 4) {
        Serial.print("Track position: ");
        Serial.print(sendData.xPos);
        Serial.print(", ");
        Serial.println(sendData.yPos);
      }
      
      ETout.sendData();
    }
    Serial.print("sendCoords elapsed time: "); Serial.println(millis() - timestamp);

    if (scriptFinished == true){
      Serial.println("*****************************");
      Serial.println("          [ END ]");
      Serial.println("*****************************");
      
      scriptFile.close();
      runningScript = false;
  
      ref_time = millis();
      delay_time = SCRIPT_INTERMISSION_MS;
      if (DEBUG > 1) {
        Serial.print("delaying ");
        Serial.print(delay_time);
        Serial.print(" ms before next starting the next script...\n");
      }
    }
  }

  if (DEBUG > 10) {
    Serial.println("  Exiting loop iteration...");
  }
}

