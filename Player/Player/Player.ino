
#include <SPI.h>
#include <SD.h>
#include <Adafruit_VS1053.h>

#define DEBUG 5
#define SCRIPT_INTERMISSION_MS    5000

#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)

// These are common pins between breakout and shield
#define CARDCS 4     // Card chip select pin
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin

#define MAX_LINE_LENGTH 128
#define MAX_ARG_LENGTH 32

#define SCRIPT_NOT_OPEN   0
#define SCRIPT_OPEN       1
#define SCRIPT_EOF        2



Adafruit_VS1053_FilePlayer musicPlayer = 
  // create breakout-example object!
  //Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, CARDCS);
  // create shield-example object!
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

void setup() {
  Serial.begin(9600);
  Serial.println("Initializing SD card...");
  SD.begin(CARDCS);    // initialise the SD card
  Serial.println("SD card initialized.");

  ref_time = millis();

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
      scriptFile = SD.open("z/script.txt");
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
        }
        else if (String(command).equals("PLAY")) {
          if (DEBUG > 1) {
            Serial.println("HANDLING PLAY COMMAND");
          }
          musicPlayer.stopPlaying();

          audiofilename = "z/";
          audiofilename.concat(command_args);
          audiofilename.concat(".mp3");

          audiofilename.toCharArray(filename_char, 10);
          
          musicPlayer.startPlayingFile(filename_char);
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


    if (DEBUG > 10) {
      Serial.println("  Exiting loop iteration...");
    }
  }
}
