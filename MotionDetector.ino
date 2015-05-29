//http://www.micropik.com/PDF/HCSR04.pdf
///Sonar pin
#define TRIG_PIN   4
#define ECHO_PIN   3
#define BAUD_RATE  115200
#define READ_DELAY 990

#define MS_PER_CM  29.411
#define MS_PER_M   2941.17
#define MS_PER_IN  74.746

//print the distance when variance is more than 5 cm
#define THRESHOLD  3*MS_PER_CM*2 

#define LOG_FILE "motion.txt"

#include <SPI.h>
#include <SD.h>
/*
 The circuit:
 * analog sensors on analog ins 0, 1, and 2
 * SD card attached to SPI bus as follows:
 ** MOSI - pin 11 - Mega 51
 ** MISO - pin 12 - Mega 50
 ** CLK - pin 13  - Mega 52
 ** CS - pin 4    - Mega 53
*/
#define FILE_CONFIRM_PIN 5

/*
 On the Ethernet Shield, CS is pin 4. Note that even if it's not
 used as the CS pin, the hardware CS pin (10 on most Arduino boards,
 53 on the Mega) must be left as an output or the SD library
 functions will not work. */
#define CHIP_SELECT 53



double tempMS, lastMS, diffMS;

unsigned long timeCounter = 0;

//////////////////////// ULTRASONIC FUNCTIONS ////////////////////////////////
const int kMicroSec = 0, kCentiMeters = 1, kMeters = 2, kInches = 3;

/* Send low-high-low pulse to activate the trigger pulse of the sensor */
void triggerSignal(){
  digitalWrite(TRIG_PIN, LOW); // Send low pulse
  delayMicroseconds(2); // Wait for 2 microseconds
  digitalWrite(TRIG_PIN, HIGH); // Send high pulse
  delayMicroseconds(10); // Wait for 10 microseconds
  digitalWrite(TRIG_PIN, LOW); // Holdoff
}

double msToInches(double microseconds){
  // According to Parallax's datasheet for the PING))), there are
  // 73.746 microseconds per inch (i.e. sound travels at 1130 feet per
  // second).  This gives the distance travelled by the ping, outbound
  // and return, so we divide by 2 to get the distance of the obstacle.
  // See: http://www.parallax.com/dl/docs/prod/acc/28015-PING-v1.3.pdf
  return microseconds/= (MS_PER_IN * 2);
}

double msToCm(double microseconds){
  // The speed of sound is 340 m/s or 29.41 microseconds per centimeters.
  // The ping travels out and back, so to find the distance of the
  // object we take half of the distance travelled.
  return microseconds/= (MS_PER_CM * 2);
}

/* Derived convertions*/
double msToM(double microseconds){
  return microseconds/= (MS_PER_M * 2);
}

double getSonarDistance(int type){//true if in CM, false Inches
 double  timecount = 0; // Echo counter
 triggerSignal();
 /* waits to assigned pin to be HIGH, then counts microsecounds until pin is LOW*/
 timecount = pulseIn(ECHO_PIN, HIGH);
  switch(type){
    case kMicroSec:   return timecount;
    case kCentiMeters:   return msToCm(timecount);
    case kMeters:        return msToM(timecount);
    case kInches:  return msToInches(timecount);
    default: return timecount;
  }
  return timecount;
}


//////////////////////// SD CARD FUNCTIONS //////////////////////////////
File logFile;

boolean openFile(){
  logFile = SD.open(LOG_FILE, FILE_WRITE);
  return logFile;
}

boolean closeFile(){
  if (logFile){
    logFile.close();
    return true;
  }
  return false;
}

void printSdInfo(){
  Sd2Card card;
  SdVolume volume;
  SdFile root;
  
  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  if (!card.init(SPI_HALF_SPEED, CHIP_SELECT)) {
    Serial.println("initialization failed. Things to check:\n* is a card is inserted?");
    Serial.println("* Is your wiring correct?\n* did you change the chipSelect pin to match your shield or module?");
    return;
  } else {
   Serial.println("Wiring is correct and a card is present."); 
  }

  Serial.print("\nCard type: ");
  switch(card.type()) {// print the type of card
    case SD_CARD_TYPE_SD1:
      Serial.println("SD1");
      break;
    case SD_CARD_TYPE_SD2:
      Serial.println("SD2");
      break;
    case SD_CARD_TYPE_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Unknown");
  }
  
  // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
  if (!volume.init(card)) {
    Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
  }
  
  // print the type and size of the first FAT-type volume
  uint32_t volumesize;
  Serial.print("\nVolume type is FAT");
  Serial.println(volume.fatType(), DEC);
  Serial.println();
  
  volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
  volumesize *= volume.clusterCount();       // we'll have a lot of clusters
  volumesize *= 512;                            // SD card blocks are always 512 bytes
  Serial.print("Volume size (bytes): ");
  Serial.print(volumesize);
  Serial.print("\tVolume size (Kbytes): ");
  volumesize /= 1024;
  Serial.println(volumesize);
  Serial.print("Volume size (Mbytes): ");
  volumesize /= 1024;
  Serial.print(volumesize);
   Serial.print("\nVolume size (Gbytes): ");
  volumesize /= 1024;
  Serial.println(volumesize);
  
  Serial.println("\nFiles found on the card (name, date and size in bytes): ");
  root.openRoot(volume);
  root.ls(LS_R | LS_DATE | LS_SIZE);// list all files in the card with date and size
}

void printResults(){
  String outputDiff = "Difference: ";
  String outputCm = "Centimeters: ";
  String outputInches = "Inches: ";

  outputDiff.concat( msToCm(diffMS) );
  outputCm.concat( msToCm(tempMS) );
  outputInches.concat( msToInches(tempMS) );
  
  Serial.println(outputDiff);
  Serial.print(outputCm);
  Serial.print(outputInches);
  Serial.println();
  
  if (openFile()){
    logFile.println(outputDiff);
    logFile.print(outputCm);
    logFile.print(outputInches);
    logFile.println();
    closeFile();
    digitalWrite(FILE_CONFIRM_PIN, LOW);
  } else {
//    Serial.print("Error opening: ");
//    Serial.println(LOG_FILE);
  }
}

//////////////////////// MAIN FUNCTIONS //////////////////////////////

void(* resetFunc) (void) = 0;//declare reset function at address 0

void repeater(){
  if(Serial.available()){
    String str = Serial.readString();    
    Serial.println(str);
    if (str == "r"){
      Serial.println("Reseting ...");
      delay(25);
      resetFunc();
    } else if (str == "p"){
      Serial.println("Printing SD card info ...");
      printSdInfo();
    } else {
      while(str == "z"){
        repeater();
      }
    }
  }
}

void setup(){
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(CHIP_SELECT, OUTPUT);
  pinMode(FILE_CONFIRM_PIN, OUTPUT);
  digitalWrite(FILE_CONFIRM_PIN, HIGH);
  
  //Setup Serial com:
  Serial.begin(BAUD_RATE);
  while(!Serial);
  
  //Setup SD card:
  Serial.print("Initializing SD card...");
  // see if the card is present and can be initialized:
  if (!SD.begin(CHIP_SELECT)) {
    Serial.println("Card failed, or not present. No data will be logged");
  } else {
    Serial.println("Card initialized.");
  }
  
  //Setup MotionDetector:
  tempMS = 0;
  lastMS = 0;
  diffMS = 0;
  pinMode(TRIG_PIN, OUTPUT); // Switch signalpin to output
  pinMode(ECHO_PIN, INPUT); // Switch signalpin to input
  Serial.println("Entering to repeater mode, type 'z' to exit or 'r' to reset.");
}

void loop(){
  if( millis() - timeCounter  > READ_DELAY){  
    digitalWrite(FILE_CONFIRM_PIN, HIGH); // turn it off if it was on
    tempMS = getSonarDistance(kMicroSec); //get distance in ms
    diffMS = abs(tempMS - lastMS);
    if( diffMS > THRESHOLD ){
      printResults();
      lastMS = tempMS;
    }
    timeCounter = millis();
  }
  repeater();
}

