//This is software for an Arduino UNO + display board + protoboard stack to control Sarah's glass fusing kiln.
//I ran through version 1 of the software with a test program on the actual kiln.  It ran up to 1000 degrees at the proper controlled rate.  
//In this version I wish to fix the following:
//1. DONE - Allow the cooling rate to also be controlled.
//2. DONE - Add an asterisk to the edit menu to indicate which program will be edited.
//3. DONE - Add an asterisk to the run menu to indicate which program will be run.
//4. DONE - In addition to the current actual temperature, show the current tgt temperature.
//5. DONE - During hold stages, show how much time remains.
//6. DONE, but not tested - Allow the user to increase or decrease the remaining time during holds. (This one will be the hardest of the bug fixes).
//7. DONE - Sarah wants the temperatures to be in 5 degree increments instead of ten.  The useable range shoudl still fit in 8 bits, though
//8. Now that cool down is controlled, I need to see how the zeroed-out stages of a program are handled.  I need to make sure that they are trivially passed.  Also, the 
//   bottom end of setpoints is now 500 degrees, so it needs to be able to continue to control the cooling rate down to ambient.  

//Richard Moore
//January 2014
//are.kay.more@gmail.com
//Not tested for safety.  Use or modify at your own risk.  

// include the library code:
#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>
#include <EEPROM.h>
#include "Adafruit_MAX31855.h"
 
int thermoDO = 3;
int thermoCS = 4;
int thermoCLK = 5;
Adafruit_MAX31855 thermocouple(thermoCLK, thermoCS, thermoDO);
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

int state = 1;
//The states are as follows:
//  1: navigation - Main Menu       
//  2: Navigation - Run Menu        
//  3: Navigation - Edit Menu       
//  4: editing an existing program  
//  5: running                       
//  6: Creating a new program       

int currStep = 1;               //This is the program step currently being edited.  
int currCharVal = 65;           //This integer will be used to cycle through characters when the user names a new program.  
int statechange = 1;
int cursorHoriz = 1;            //This is the horizontal cursor location.
int cursorVert = 1;             //This is the certical cursor location.
int currentProg = 1;            //This is the currrently selected program.
int currentActualTemp = 70;     //The current actual temperature.
int currentTargetTemp = 70;     //This is the current target temperature;
int tempUpdateDelay = 3000;     //This is the time between relay actions in milliseconds.
int eepromAddress = 0;
byte numProgs;                  //This is the total number of stored programs, written to the first byte of the EEPROM memory.
char name1[11] = "          ";  //This is the name of the currently selected program
char name2[11] = "          ";  //This is the name of the program after the currently selected program
char newName[11] = "          ";
unsigned long progStartTime;
unsigned long currTime;
double elapsed = 0;
double currTemp = 0;
double planTemp = 0;
byte Tmp = 0;                  //Temperature in tens of degrees Farenheit
byte Rte = 0;                  //Rate in tens of degrees per hour
byte Hld = 0;                  //Hold time in minutes
int prgStart = 0;              //This is the initial memory address of the currently selected program
int heatIncreaseFlag = 1;

int currStage = 0;
double currInitTemp = 0;
double currTgtTemp = 0;
int currRate = 0;
unsigned long currStageStartTime = 0;
unsigned long currRunTime = 0;
int currStageState = 3;        //1 = heating, 2 = holding, 3 = stage finished
unsigned long currHoldTime = 0;
//double currTemp = 0;
unsigned long holdStartTime = 0;
int runflag = 0;

//I'll use pin 13 for the relay controller, because it already has an indicator LED on the Arduino UNO board.

#define WHITE 0x7

void setup() {
  // Debugging output
  Serial.begin(9600);
  pinMode(13, OUTPUT);
  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);
  //int time = millis();
  lcd.print("MELTINATOR 9000 ");//("Initializing...");
  lcd.setCursor(0,1);
  lcd.print("LezBurnSumStuff!");
  delay(2000);
  //time = millis() - time;
  //Serial.print("Took "); Serial.print(time); Serial.println(" ms");
  lcd.setBacklight(WHITE);
}

uint8_t i=0;
void loop() {
  //Serial.begin(9600);
  //Serial.print("Iteration...\n");
  //Serial.print(state);
  //Serial.print("\n");
  //Serial.print(runflag);
  //Serial.print("\n");
  Serial.print(currStageState);
  Serial.print("\n");
  Serial.print(currTgtTemp);
  Serial.print("\n");
  Serial.print(currRate);
  Serial.print("\n");
  Serial.print(currRunTime);
  Serial.print("\n");
  Serial.print(currInitTemp);
  Serial.print("\n");
  Serial.print(planTemp);
  Serial.print("\n");
  Serial.print("\n");
  //This first "if" loop is to allow the loop to run freely if the program is in the "run" state (5).  
  //if (state!=5) {
    uint8_t buttons = lcd.readButtons();
  //}
  //else {
  //  uint8_t buttons = 1;
  //  statechange = 1;
  //}
  if ((buttons)||(state==5)) {
    runflag = 1;
  }
  else {
    runflag = 0;
  }
  if ((runflag)||(statechange)) {
  //if ((buttons)||(statechange)) {
    statechange = 0;
  switch (state) {
    case 1:
      //State:Navigation - Main Menu
      //Draw the Main Menu screen
      lcd.clear();
      lcd.setCursor(0,0);
      //lcd.blink();
      lcd.print("Main Menu       ");
      lcd.setCursor(0,1);
      switch (cursorHoriz) {
        case 1:
          lcd.print("*Run _Edit _New ");
          break;
        case 2:
          lcd.print("_Run *Edit _New ");
          break;
        case 3:
          lcd.print("_Run _Edit *New ");
          break;
      }
      //Place the cursor in the appropriate location
      //Allow the user to move the cursor location and press enter.
      if (buttons) {
        if (buttons & BUTTON_LEFT) {
          cursorHoriz = cursorHoriz - 1;
          statechange = 1;
        }
        if (buttons & BUTTON_RIGHT) {
          cursorHoriz = cursorHoriz + 1;
          statechange = 1;
        }
        //Keep the cursor location valid:
        cursorHoriz = constrain(cursorHoriz,1,3);
        //If the select button was pressed, change to the next state:
        if (buttons & BUTTON_SELECT) {
          statechange = 1;
          switch (cursorHoriz) {
            case 1:
              state = 2;
              //Set the vertical cursor to 1 to start at the beginning of the programs list:
              cursorVert = 1;
              break;
            case 2:
              state = 3;
              break;
            case 3:
              //This one is for initializing a new program.
              state = 6;
              cursorHoriz = 0;
              lcd.clear();
              lcd.setCursor(0,0);
              lcd.print("New Program Name");
              break;
          }
        }
      }
      break;
    case 2:
      //State:Navigation - Run Menu
      //Get the list of programs
      numProgs = EEPROM.read(0);
      for (int n = 0; n < 10; n++) {
        name1[n] = EEPROM.read((cursorVert-1)*40+1+n);//Here the program length is 40: a ten character name with 10 3-byte envelope stages.
      }
      for (int n = 0; n < 10; n++) {
        name2[n] = EEPROM.read((cursorVert)*40+1+n);//Here the program length is 40: a ten character name with 10 3-byte envelope stages.
      }
      //Draw the Run Menu screen
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("_");
      lcd.print(name1);
      lcd.setCursor(10,0);
      lcd.print("  RUN");
      lcd.setCursor(0,1);
      lcd.print("*");
      lcd.print(name2);
      //Place the cursor in the appropriate location
      //If up, cursorVert = cursorVert--
      //If down, cursorVert = cursorVert++
      //If enter, change state to Run
      if (buttons) {
        statechange = 1;
        if (buttons & BUTTON_UP) {
          cursorVert = cursorVert - 1;
        }
        if (buttons & BUTTON_DOWN) {
          cursorVert = cursorVert + 1;
        }
        //Keep the cursor location valid:
        cursorVert = constrain(cursorVert,1,int(numProgs));
        //If the select button was pressed, start running the selected program:
        if (buttons & BUTTON_SELECT) {
          statechange = 1;
          state = 5;
          currentProg = cursorVert;
          //Start run timer
          progStartTime = millis();
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Program Running");
          prgStart = (currentProg-1)*40+1;
        }
      }
      break;
    case 3:
      //State:Navigation - Edit Menu
      //Get the list of programs
      numProgs = EEPROM.read(0);
      for (int n = 0; n < 10; n++) {
        name1[n] = EEPROM.read((cursorVert-1)*40+1+n);//Here the program length is 40: a ten character name with 10 3-byte envelope stages.
      }
      for (int n = 0; n < 10; n++) {
        name2[n] = EEPROM.read((cursorVert)*40+1+n);//Here the program length is 40: a ten character name with 10 3-byte envelope stages.
      }
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("_");
      lcd.print(name1);
      lcd.setCursor(10,0);
      lcd.print(" EDIT");
      lcd.setCursor(0,1);
      lcd.print("*");
      lcd.print(name2);
      //Place the cursor in the appropriate location
      //If up, cursorVert = cursorVert--
      //If down, cursorVert = cursorVert++
      //If enter, change state to Edit
      if (buttons) {
        statechange = 1;
        if (buttons & BUTTON_UP) {
          cursorVert = cursorVert - 1;
        }
        if (buttons & BUTTON_DOWN) {
          cursorVert = cursorVert + 1;
        }
        //Keep the cursor location valid:
        cursorVert = constrain(cursorVert,1,int(numProgs));
        //If the select button was pressed, start running the selected program:
        if (buttons & BUTTON_SELECT) {
          statechange = 1;
          state = 4;
          currentProg = cursorVert;
          //Let's initialize the cursor position prior to the edit state:
          cursorHoriz = 1;
          cursorVert = 1;
          prgStart = (currentProg-1)*40+1;
        }
      }
      break;
    case 4:
      //State:Programming
      //Depending on the horizontal cursor position, the vertical buttons will 1: change the step, 2: change the target temp, 3: change the rate, or 4: change the hold time 
      Tmp = EEPROM.read(prgStart+10+(currStep-1)*3);
      Rte = EEPROM.read(prgStart+10+(currStep-1)*3+1);
      Hld = EEPROM.read(prgStart+10+(currStep-1)*3+2);
      if (buttons) {
        statechange = 1;
        if (buttons & BUTTON_UP) {
          switch (cursorHoriz) {
            case 1:
              currStep = currStep + 1;
              currStep = constrain(currStep,1,10);
              Tmp = EEPROM.read(prgStart+10+(currStep-1)*3);
              Rte = EEPROM.read(prgStart+10+(currStep-1)*3+1);
              Hld = EEPROM.read(prgStart+10+(currStep-1)*3+2);
              break;
            case 2:
              Tmp = constrain(Tmp,0,254);
              Tmp = Tmp + 1;
              break;
            case 3:
              Rte = constrain(Rte,0,254);
              Rte = Rte + 1;
              break;
            case 4:
              Hld = constrain(Hld,0,254);
              Hld = Hld + 1;
              break;
          }   
        }
        if (buttons & BUTTON_DOWN) {
          switch (cursorHoriz) {
            case 1:
              currStep = currStep - 1;
              currStep = constrain(currStep,1,10);
              Tmp = EEPROM.read(prgStart+10+(currStep-1)*3);
              Rte = EEPROM.read(prgStart+10+(currStep-1)*3+1);
              Hld = EEPROM.read(prgStart+10+(currStep-1)*3+2);
              break;
            case 2:
              Tmp = constrain(Tmp,1,255);
              Tmp = Tmp - 1;
              break;
            case 3:
              Rte = constrain(Rte,1,255);
              Rte = Rte - 1;              
              break;
            case 4:
              Hld = constrain(Hld,1,255);
              Hld = Hld - 1;              
              break;
          }
        }
        if (buttons & BUTTON_LEFT) {
          cursorHoriz = cursorHoriz - 1;
        }
        if (buttons & BUTTON_RIGHT) {
          cursorHoriz = cursorHoriz + 1;
        }
        cursorHoriz = constrain(cursorHoriz,1,4);
      }
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Stp Tmp Rate Hld");
      //get values for the current vertical cursor step from memory
      EEPROM.write(prgStart+10+(currStep-1)*3, Tmp);
      EEPROM.write(prgStart+10+(currStep-1)*3+1, Rte);
      EEPROM.write(prgStart+10+(currStep-1)*3+2, Hld);
      Tmp = EEPROM.read(prgStart+10+(currStep-1)*3);
      Rte = EEPROM.read(prgStart+10+(currStep-1)*3+1);
      Hld = EEPROM.read(prgStart+10+(currStep-1)*3+2);
      //Display the current step values
      lcd.setCursor(0,1);
      lcd.print(currStep);
      lcd.setCursor(3,1);
      //lcd.print(Tmp*10);
      //Sarah requested temperature control in 5 degree increments.  
      lcd.print(Tmp*5+500);
      lcd.setCursor(8,1);
      lcd.print(Rte*10);
      lcd.setCursor(13,1);
      lcd.print(Hld);
      //This should currently allow the user to edit the envelope step values, but it writes values to the EEPROM with each button press.
      //Instead, I should hold these values in memory and write them only at the end, since the EEPROMs are only rated for 100,000 writes.  
      break;
    case 5:
      //State:Running
      switch (currStageState) {
        case 1:
          //Heating
          currRunTime = millis() - currStageStartTime;
          //There are different expressions to find planTemp depending on if it's heating or cooling.
          if (heatIncreaseFlag) 
          {
            planTemp = double(currRate)*double(10)*(double(currRunTime)/(double(1000)*double(60)*double(60))) + double(currInitTemp);
          } 
          else 
          {
            planTemp = double(currInitTemp) - double(currRate)*double(10)*(double(currRunTime)/(double(1000)*double(60)*double(60)));
          }
          currTemp = thermocouple.readFarenheit(); //Get the current temperature*********
          lcd.setCursor(0,1);
          lcd.print("Act:");
          lcd.print(int(currTemp));
          lcd.setCursor(8,1);
          lcd.print("Tgt:");
          lcd.print(int(planTemp));
          Serial.print("Current Temperature: "); Serial.print(currTemp); Serial.println(" degrees F");
          Serial.print("Planned Temperature: "); Serial.print(planTemp); Serial.println(" degrees F");
          Serial.print(" \n");
          //Close the relay if the actual temperature is lower than the planned one.  Open it otherwise.
          if (currTemp<planTemp) {
            digitalWrite(13, HIGH);
          }
          else {
            digitalWrite(13, LOW);
          }
          //Here the heatIncreaseFlag is used to determine if the currTgtTemp is going to be crossed from above or below before the stage is incremented.
          if (heatIncreaseFlag) {
            if (currTemp>currTgtTemp) {
              //Change to the hold state.
              currStageState = 2;
              holdStartTime = millis();
            }
          }
          else
          {
            if (currTemp<currTgtTemp) {
            //Change to the hold state.
            currStageState = 2;
            holdStartTime = millis();
            }
          }
          break;
        case 2:
          //Holding
          currTemp = thermocouple.readFarenheit(); 
          lcd.setCursor(0,1);
          lcd.print("Act:");
          lcd.print(int(currTemp));
          lcd.setCursor(8,1);
          lcd.print("Tgt:");
          lcd.print(int(currTgtTemp));
          if (currTemp<currTgtTemp) {
            digitalWrite(13, HIGH);
          }
          else {
            digitalWrite(13, LOW);
          }
          //If the user has pressed the up button, add a minute to the hold time.
          //If the user has pressed the down button, subtract a minute from the hold time.  
          if (buttons & BUTTON_UP) {
            currHoldTime = currHoldTime + 1;
          }
          if (buttons & BUTTON_DOWN) {
            currHoldTime = currHoldTime - 1;
          }
          currRunTime = (double(millis())-double(holdStartTime))/(double(1000)*double(60));
          //Print the current remaining hold time.  
          lcd.setCursor(5,0);
          lcd.print("Time:");
          lcd.print(int((currHoldTime-currRunTime)));
          if (currRunTime>=currHoldTime) {
            currStageState = 3;
          }
          break;
        case 3:
          //Stage finished
          currStageState = 1;
          currStage = currStage + 1;
          lcd.setCursor(0,0);
          lcd.print("Stg:");
          lcd.print(currStage);
          lcd.print("           ");
          currHoldTime = EEPROM.read(prgStart+10+(currStage-1)*3+2);
          currRate = EEPROM.read(prgStart+10+(currStage-1)*3+1);
          //currTgtTemp = 10*double(EEPROM.read(prgStart+10+(currStage-1)*3));
          currTgtTemp = 500+5*double(EEPROM.read(prgStart+10+(currStage-1)*3));
          currStageStartTime = millis();
          currInitTemp = thermocouple.readFarenheit();
          if (currTgtTemp>currInitTemp) {
            heatIncreaseFlag = 1;
          }
          else
          {
            heatIncreaseFlag = 0;
          }
          if (currStage==11) {
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Program Complete");
          }
          break;
      }
      //Wait for the appropriate delay 
      delay(tempUpdateDelay);
      break;
    case 6:
      //State: Create a new program
      //Left and right change the cursor position.  Up and down cycle through characters.  Enter saves.  
      lcd.setCursor(cursorHoriz,1);
      //Okay.  It looks like I can write characters this way.
      lcd.print(char(currCharVal));
      newName[cursorHoriz] = char(currCharVal);
      if (buttons) {
        statechange = 1;
        if (buttons & BUTTON_UP) {
          currCharVal = currCharVal + 1;
        }
        if (buttons & BUTTON_DOWN) {
          currCharVal = currCharVal - 1;
        }
        if (buttons & BUTTON_RIGHT) {
          cursorHoriz = cursorHoriz + 1;
        }
        if (buttons & BUTTON_LEFT) {
          cursorHoriz = cursorHoriz - 1;
        }
        if (buttons & BUTTON_SELECT) {
          //And read the number of programs
          numProgs = EEPROM.read(0);
          //Increment the number of programs and save to EEPROM
          numProgs = numProgs + 1;
          EEPROM.write(0,numProgs);
          //Make the new program the current program in currentProg
          currentProg = numProgs;
          //Write the name from the display to the EEPROM
          for (int n = 0; n<10; n++) {
            EEPROM.write(numProgs*40+1+n,newName[n]);
          }
          //EEPROM.write(numProgs*40,newName);
          //And pass control to the program editing state (4).
          state = 4;
          prgStart = (numProgs-1)*40+1;
        }
      }
      //This delay will ensure that single button presses don't double trigger.
      delay(200);   
      break;
  }
  }
}
