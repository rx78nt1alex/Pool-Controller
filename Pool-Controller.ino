// This #include statement was automatically added by the Particle IDE.
#include "Adafruit_SSD1306/Adafruit_SSD1306.h" //OLED

// Refer to README.md for additional information
// https://github.com/Here-Be-Dragons/Pool-Controller

// TODO: Fix SmartThings override

////
//Start of user configurations
////

// Update the below array with the settings from 
// the pump for output to the Screen, SmartThings, and
// estimating turnover/energy use.  This doesn't actually
// change the speeds on the pump, it's just bookkeeping.
// speedRPM[0] must be 0 to turn the pump off
// 1200RPM is the minimum speed for chlorinator flow sensor
// 2750RPM is the recommended speed for 9 solar panels (~45GPM)
// 3450RPM for pool cleaning
uint16_t speedRPM[8] = {0,600,1200,1800,2300,2750,3000,3450};

// Manually trigger each of the above speeds and record their
// energy usage (Watts) below for accurate consumption tracking
uint16_t energyConsum[8] = {5,100,200,300,400,500,600,700};

// How long (in seconds) to run manual Overrides
uint16_t overrideLength = 3600;

// Speed activation times in HHMM format. If two speeds have the same
// time entered, the higher speed takes precidence.
uint16_t aSpeed1[] = {0000,                   2100};
uint16_t aSpeed2[] = {                            };
uint16_t aSpeed3[] = {    0600,   1200,   1500    };
uint16_t aSpeed4[] = {                            };
uint16_t aSpeed5[] = {                            };
uint16_t aSpeed6[] = {        1100,   1300        };
uint16_t aSpeed7[] = {                            };
uint16_t aSpeed8[] = {                            };

////
//End of user configurations
////

uint32_t currentEpochTime;       //The current Epoch time set at the beginning of each loop in getTimes()
uint16_t currentTime;			 //Friendly time converted from currentEpochTime via convertTime(), 10:00 PM is referenced as 2200
uint16_t previousTime;			 //The time as of the last loop, set at the bottom of loop()
uint16_t kWhTally = 0;			 //Daily count of kWh consumption, to upload to Google Docs for tracking
char publishString[40];			 //Temporary string to use for Particle.publish of kWhTally
uint16_t currentSpeed = 0;       //The current motor speed setting number (1-8)
uint16_t overrideSpeed;          //Stores the override speed when set manually
//uint16_t lastChange;           //Time the speed was last changed via scheduler (not currently used)
uint16_t scheduledSpeed = 0;     //What speed the schedule says you should be at
uint8_t autoOverride = 0;        //0 for scheduled, 1 for override.  Changes when solar kicks on
uint8_t manualOverride = 0;      //0 for scheduled, 1 for override.  Changes via user intervention
uint16_t overrideStarted = 0;    //This is set to currentEpochTime when a manual override is triggered

// Assign pins to relays
// D0 reserved for SDA of OLED
// D1 reserved for SCL of OLED
#define pPumpRelay1     D2
#define pPumpRelay2     D3
#define pPumpRelay3     D4
#define pSolarRelay1    D5
#define OLED_RESET      D6

Adafruit_SSD1306 oled(OLED_RESET);

void setup() {
    pinMode(pPumpRelay1,  OUTPUT);
    pinMode(pPumpRelay2,  OUTPUT);
    pinMode(pPumpRelay3,  OUTPUT);
    pinMode(pSolarRelay1, INPUT );

    oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);    // Initialize with the I2C addr 0x3D (for the 128x64 screen)
    oled.display();                            // Show splashscreen
    Particle.function("mOverride", mOverride); // Listen for a manual override via remote user input
    Time.zone(-5);			                   // Ignores DST... :(
    delay(5000);                               // Give it time to stabilize the RTC and get a time from NTP
	getTimes();								   // Set up initial times
    returnToSchedule();                        // Find what the current speed should be
}

void loop() {
  //Finds the current time each loop
  getTimes();

  //Check for expired manual Override
  if( manualOverride == 1 ) {
    uint32_t ovr = overrideStarted + overrideLength;
    if (ovr <= currentEpochTime) {
      returnToSchedule();
    }
  }

  //Check for new scheduled speed
  scheduledSpeed = findScheduledSpeed(currentTime);

  //Check for Solar Speed Override.  Applies in setPumpSpeed()
  autoOverride = digitalRead(pSolarRelay1);

  //Activate new speed if there is one
  setPumpSpeed();

  //Update the oled display
  updateDisplay();
  
  //Keep track of energy consumption
  if (currentTime != previousTime) {
    if (currentTime == 0) {
		sprintf(publishString, "%d", kWhTally); //  Convert uint16_t kWhTally to char[40] for Particle.publish()
		Particle.publish("24Hr kWh usage", publishString);
		kWhTally = 0;
	}
	kWhTally += energyConsum[currentSpeed-1]/60; //One minute worth of kWh
    previousTime = currentTime;
  }
}

void getTimes(){
  //Gets Time-zone adjusted Epoch Time from the cloud, and converted HHMM comparison time
  currentEpochTime = Time.local();
  currentTime = convertTime(currentEpochTime);
}

uint16_t convertTime(uint32_t testTime){
  //Converts Epoch to HHMM
  uint16_t returnTime;
  returnTime = Time.hour(testTime) * 100 + Time.minute(testTime);
  return returnTime;
}

uint16_t findScheduledSpeed(uint16_t atTime){ // Find the Scheduled Speed
  uint16_t x = scheduledSpeed;
  for (uint16_t i=0; i < sizeof(aSpeed1) / sizeof(uint16_t); i++) {
    if ( aSpeed1[i] == atTime ) {
      x = 1;
    }
  }
  for (uint16_t i=0; i < sizeof(aSpeed2) / sizeof(uint16_t); i++) {
    if ( aSpeed2[i] == atTime ) {
      x = 2;
    }
  }
  for (uint16_t i=0; i < sizeof(aSpeed3) / sizeof(uint16_t); i++) {
    if ( aSpeed3[i] == atTime ) {
      x = 3;
    }
  }
  for (uint16_t i=0; i < sizeof(aSpeed4) / sizeof(uint16_t); i++) {
    if ( aSpeed4[i] == atTime ) {
      x = 4;
    }
  }
  for (uint16_t i=0; i < sizeof(aSpeed5) / sizeof(uint16_t); i++) {
    if ( aSpeed5[i] == atTime ) {
      x = 5;
    }
  }
  for (uint16_t i=0; i < sizeof(aSpeed6) / sizeof(uint16_t); i++) {
    if ( aSpeed6[i] == atTime ) {
      x = 6;
    }
  }
  for (uint16_t i=0; i < sizeof(aSpeed7) / sizeof(uint16_t); i++) {
    if ( aSpeed7[i] == atTime ) {
      x = 7;
    }
  }
  for (uint16_t i=0; i < sizeof(aSpeed8) / sizeof(uint16_t); i++) {
    if ( aSpeed1[8] == atTime ) {
      x = 8;
    }
  }
  return x;
}

void setPumpSpeed() {
  uint8_t newSpeed;
  newSpeed = scheduledSpeed;
  
  //Set Manual Override Speed to newSpeed if active
  if( manualOverride == 1 ){
    newSpeed = overrideSpeed;
  }
  
  //If auto override is active, set a minimum speed
  if( autoOverride == 1 && newSpeed < 6 ){
    newSpeed = 6;
  }
      switch (newSpeed) {
        case 1:
          digitalWrite(pPumpRelay1, LOW );
          digitalWrite(pPumpRelay2, LOW );
          digitalWrite(pPumpRelay3, LOW );
          break;
        case 2:
          digitalWrite(pPumpRelay1, HIGH);
          digitalWrite(pPumpRelay2, LOW );
          digitalWrite(pPumpRelay3, LOW );
          break;
        case 3:
          digitalWrite(pPumpRelay1, LOW );
          digitalWrite(pPumpRelay2, HIGH);
          digitalWrite(pPumpRelay3, LOW );
          break;
        case 4:
          digitalWrite(pPumpRelay1, HIGH);
          digitalWrite(pPumpRelay2, HIGH);
          digitalWrite(pPumpRelay3, LOW );
          break;
        case 5:
          digitalWrite(pPumpRelay1, LOW );
          digitalWrite(pPumpRelay2, LOW );
          digitalWrite(pPumpRelay3, HIGH);
          break;
        case 6:
          digitalWrite(pPumpRelay1, HIGH);
          digitalWrite(pPumpRelay2, LOW );
          digitalWrite(pPumpRelay3, HIGH);
          break;
        case 7:
          digitalWrite(pPumpRelay1, LOW );
          digitalWrite(pPumpRelay2, HIGH);
          digitalWrite(pPumpRelay3, HIGH);
          break;
        case 8:
          digitalWrite(pPumpRelay1, HIGH);
          digitalWrite(pPumpRelay2, HIGH);
          digitalWrite(pPumpRelay3, HIGH);
          break;
	//This condition should never happen, but default to Speed 1 if it does
        default:
          digitalWrite(pPumpRelay1, LOW );
          digitalWrite(pPumpRelay2, LOW );
          digitalWrite(pPumpRelay3, LOW );
          returnToSchedule(); //Try a schedule reset
          break;
      }
      currentSpeed = newSpeed; //Update current speed value
}

int mOverride(String command) { //Triggered by SmartThings
  overrideSpeed = atoi(command); //Convert string to integer
  if( overrideSpeed <= 8 ){ //These are direct speeds
    manualOverride = 1;
    overrideStarted = currentEpochTime;
    setPumpSpeed();
  } else {
    if( overrideSpeed == 9 ){ //9 is "return to schedule"
      manualOverride = 0;
      returnToSchedule();
    }
    else if( overrideSpeed == 10){ //10 is a poll for current speed, no changes
        //Just a poll, no changes
    }
  }
  return speedRPM[currentSpeed-1];
}

void returnToSchedule() {
  uint32_t testTimeEpoch = currentEpochTime;
  uint16_t testTime;
  while( scheduledSpeed == 0 ) {
	testTime = convertTime(testTimeEpoch);
    scheduledSpeed = findScheduledSpeed(testTime);
    testTimeEpoch--;
  }
  manualOverride = 0;
  overrideSpeed = 0;
  overrideStarted = 0;
}
void updateDisplay(){
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0,0);
  oled.print("Current Time: ");
  oled.println(currentTime);
  oled.print("Speed: ");
  oled.print(currentSpeed);
  oled.print(" (");
  oled.print(speedRPM[currentSpeed-1]);
  oled.println(" RPM)");
  oled.print("Sched: ");
  oled.print(scheduledSpeed);
  oled.print(" (");
  oled.print(speedRPM[scheduledSpeed-1]);
  oled.println(" RPM)");
  oled.print("Manual Override? ");
  if( manualOverride ) oled.println("YES");
  else oled.println("no");
  oled.print("Auto Override? ");
  if( autoOverride ) oled.println("YES");
  else oled.println("no");
  oled.print("Override Spd: ");
  oled.println(overrideSpeed);
  oled.print("Override timer: ");
  oled.println(overrideStarted + 60 - currentTime);
  oled.display();
  
}