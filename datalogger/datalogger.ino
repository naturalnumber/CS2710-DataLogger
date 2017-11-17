/*
SD card read/write
* SD card attached to SPI bus as follows:
** MOSI - pin 11
** MISO - pin 12
** CLK - pin 13
** CS - pin 4
*/

// Includes
#include <DS1302.h>
#include <SPI.h>
#include <SD.h>
#include <GY_85.h>
#include <Wire.h>
#include <IRremote.h>

// Debug control (>0 on, <=0 off)
#define DEBUG 0

// Not usable, for reference
#define SD_MOSI 11
#define SD_MISO 12
#define SD_CLK 13
#define SDA A4
#define SLC A5

// Used pins
// SD lines
#define ETH_LINE 10
#define SD_LINE 4
// clock lines
#define RST A2
#define DAT A1
#define CLK A0
// IR pin
#define IR_RECV_PIN 2
// Button pin
#define BTN 7
// LEDs all pwm
#define RED_LED 3
#define YELLOW_LED 5
#define GREEN_LED 6

// Array Constants
#define X_POS 0
#define Y_POS 1
#define Z_POS 2
#define DIM 3
#define MIN 0
#define MAX 1
#define TOT 2
#define TYPES 3
// Data array variables
#define RANGE_SIZE TYPES*DIM

// Modes
#define MODE_ERROR -1
#define MODE_UNREADY 0
#define MODE_READY 1
#define MODE_RUNNING 2

// Flashing Constants
#define FLASH_TIME 500
#define FLASH_CYCLE 5000
#define FLASH_NUM 10
#define FLASH_1 0
#define FLASH_2 5

// Write variables
#define WRITE_INTERVAL 2000
#define SAMPLE_INTERVAL 1

// Format variables
#define SF_LENGTH 9
#define SF_PLACES 5

// TODO: fill this in with the actual IR button
#define OK_HEX_STRING "ff02fd"
#define OK_HASH_STRING "ff52ad"
#define OK_HEX 0xff02fd
#define OK_HASH 0xff52ad

// Constants
static String OUT_FILE_NAME = "accelbus.csv"; //F("accelbus.csv")
static char DELIM = ',';

// Member variables
File outFile;
GY_85 GY85;

// Real Time Clock
DS1302 rtc(RST, DAT, CLK);

// IR setup
IRrecv irrecv(IR_RECV_PIN);
decode_results results;

// Running Variables
int   seqnum = 1;
boolean isEnabled;

// Sampling
int     aRange[RANGE_SIZE]; //  * 0.0039
float   gRange[RANGE_SIZE]; //  
int     mRange[RANGE_SIZE]; //  * 0.92
//int   aTotal[DIM]; //  * 0.0039
//float   gTotal[DIM]; //  
//int   mTotal[DIM]; //  * 0.92

// Maybe don't need, keep as check right now
short   samples = 0;
float   w = 1.0;
byte    mode = MODE_ERROR;
unsigned short flash = FLASH_1;

// Temp storage
int     a[DIM];
float   g[DIM];
int     m[DIM];
int*    aRaw;
float*  gRaw;
int*    mRaw;
static char sBuffer[SF_LENGTH];

// Timing variables
unsigned long now;
unsigned long lastSample;
unsigned long lastWrite;


void setup() {
  // Initialize LEDs
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  mode = MODE_UNREADY;
  lightLEDs();
  
  #if DEBUG > 0
    // For debugging, disable in final
    // Open serial communications and wait for port to open:
    Serial.begin(9600);
  #endif
  
  // Disable the Ethernet interface
  pinMode(ETH_LINE, OUTPUT);
  digitalWrite(ETH_LINE, HIGH);
  
  // Initialize GY_85
  Wire.begin();
  delay(10);
  GY85.init();
  delay(10);

  // Initialize the IR sensor
  irrecv.enableIRIn();

  // Setup Button
  pinMode(BTN, INPUT);

  initializeFile();
  lightLEDs();
  
  now = millis();
  lastSample = now;
  lastWrite = now;
}

void loop() {
  now = millis();
  flash = (now % FLASH_CYCLE) / FLASH_TIME;
  
  if(isEnabled)
  {
    #if SAMPLE_INTERVAL < 5
      takeSample();
    #else
      if (now - lastSample > SAMPLE_INTERVAL) {
        takeSample();
        lastSample = millis();
      }
    #endif
    if (now - lastWrite > WRITE_INTERVAL) {
      writeToFile();
      lastWrite = millis();
    }
  }

  checkEnabled();
  // sample code delays for processing IR device input
  // dont know if it's needed
  // might be nice to get on a timer but who knows
  //delay(200);

  lightLEDs();
}

void checkEnabled()
{
  if (mode != MODE_ERROR) {
    if (irrecv.decode(&results))
    {
      //if (String(results.value, HEX).equals(OK_HEX_STRING))
      if (results.value == OK_HEX) // This is faster
      {
        #if DEBUG > 0
          Serial.println(F("Starting sampling..."));
        #endif
        isEnabled = true;
        lastWrite = millis();
        mode = MODE_RUNNING;
      } else if (results.value == OK_HASH) {
        #if DEBUG > 0
          Serial.println(F("Ending sampling..."));
        #endif
        isEnabled = false;
        writeToFile();
        mode = MODE_UNREADY;
      }
      
      irrecv.resume();
    } else { // Check button
      if(digitalRead(BTN)) {
        delay(100); // Get more consistent signal...
        if(digitalRead(BTN)) {
          #if DEBUG > 0
            Serial.println(F("Toggling enabled..."));
          #endif
          isEnabled = !isEnabled;
          if(isEnabled) {
            mode = MODE_RUNNING;
            lastWrite = millis();
          } else {
            writeToFile();
            mode = MODE_UNREADY;
          }
        }
      }
    }
  }
}

void lightLEDs() {
  switch (mode) {
    default:
    case MODE_ERROR:
      if (flash % 2) {
        digitalWrite(RED_LED, HIGH);
        digitalWrite(YELLOW_LED, LOW);
        digitalWrite(GREEN_LED, LOW);
      } else {
        digitalWrite(RED_LED, LOW);
        digitalWrite(YELLOW_LED, HIGH);
        digitalWrite(GREEN_LED, LOW);
      }
      break;
    case MODE_UNREADY:
      digitalWrite(RED_LED, HIGH);
      digitalWrite(YELLOW_LED, LOW);
      digitalWrite(GREEN_LED, LOW);
      break;
    case MODE_READY:
      digitalWrite(RED_LED, LOW);
      digitalWrite(YELLOW_LED, HIGH);
      digitalWrite(GREEN_LED, LOW);
      break;
    case MODE_RUNNING:
      if (flash == FLASH_1) {
        digitalWrite(RED_LED, LOW);
        digitalWrite(YELLOW_LED, LOW);
        digitalWrite(GREEN_LED, HIGH);
      } else {
        digitalWrite(RED_LED, LOW);
        digitalWrite(YELLOW_LED, LOW);
        digitalWrite(GREEN_LED, LOW);
      }
      break;
  }
}

inline short index(short i, short j, short J, short k, short K) {
  return (i*J+j)*K+k;
}

inline short index(short i, short j, short J) {
  return i*J+j;
}

void takeSample() {
  /*
  #if DEBUG > 0
    Serial.println("Taking sample "+String(samples));
  #endif
  */

  aRaw = GY85.readFromAccelerometer();
  gRaw = GY85.readGyro();
  mRaw = GY85.readFromCompass();
  
  a[X_POS] = GY85.accelerometer_x(aRaw);
  a[Y_POS] = GY85.accelerometer_y(aRaw);
  a[Z_POS] = GY85.accelerometer_z(aRaw);
  g[X_POS] = GY85.gyro_x(gRaw);
  g[Y_POS] = GY85.gyro_y(gRaw);
  g[Z_POS] = GY85.gyro_z(gRaw);
  //float gt = GY85.temp  (GY85.readGyro());
  m[X_POS] = GY85.compass_x(mRaw);
  m[Y_POS] = GY85.compass_y(mRaw);
  m[Z_POS] = GY85.compass_z(mRaw);

  if (samples == 0) { // Initialize if first sample
    for(int i = 0; i < DIM; i++) {
      aRange[index(TOT,i,DIM)] = aRange[index(MIN,i,DIM)] = aRange[index(MAX,i,DIM)] = a[i];
      gRange[index(TOT,i,DIM)] = gRange[index(MIN,i,DIM)] = gRange[index(MAX,i,DIM)] = g[i];
      mRange[index(TOT,i,DIM)] = mRange[index(MIN,i,DIM)] = mRange[index(MAX,i,DIM)] = m[i];
    }
  } else {
    for(int i = 0; i < DIM; i++) {
      if (a[i] < aRange[index(MIN,i,DIM)]) aRange[index(MIN,i,DIM)] = a[i];
      else if (a[i] > aRange[index(MAX,i,DIM)]) aRange[index(MAX,i,DIM)] = a[i];

      //if (g[i] != 0) // This and m
        if (g[i] < gRange[index(MIN,i,DIM)]) gRange[index(MIN,i,DIM)] = g[i];
        else if (g[i] > gRange[index(MAX,i,DIM)]) gRange[index(MAX,i,DIM)] = g[i];
      
      if (m[i] < mRange[index(MIN,i,DIM)]) mRange[index(MIN,i,DIM)] = m[i];
      else if (m[i] > mRange[index(MAX,i,DIM)]) mRange[index(MAX,i,DIM)] = m[i];
      
      // Add to running totals
      aRange[index(TOT,i,DIM)] += a[i];
      gRange[index(TOT,i,DIM)] += g[i];
      mRange[index(TOT,i,DIM)] += m[i];
    }
  }
  samples++;
}

void writeToFile () {
  // Get time
  Time t = rtc.time();
  
  // Open the file
  outFile = SD.open(OUT_FILE_NAME, O_APPEND | O_WRITE); //FILE_WRITE);
  
  // if the file opened okay, write to it:
  if (outFile) {
    #if DEBUG > 0
      Serial.print(F("Writing to accelbus.csv... "));
      Serial.print(samples);
      Serial.print(F(" samples..."));
    #endif

    w = 1.0f / samples;
    //  seqnum, date/time, 
    //  acc_x_avg,  acc_x_min,  acc_x_max,  acc_y_avg,  acc_y_min,  acc_y_max,  acc_z_avg,  acc_z_min,  acc_z_max, 
    //  gyro_x_avg, gyro_x_min, gyro_x_max, gyro_y_avg, gyro_y_min, gyro_y_max, gyro_z_avg, gyro_z_min, gyro_z_max, 
    //  mgnt_x_avg, mgnt_x_min, mgnt_x_max, mgnt_y_avg, mgnt_y_min, mgnt_y_max, mgnt_z_avg, mgnt_z_min, mgnt_z_max

    //  Time Format: 2017-11-08 15:25:25
    outFile.print(seqnum);
    outFile.print(DELIM);
    outFile.print(t.yr);
    outFile.print('-');
    if (t.mon < 10) outFile.print('0');
    outFile.print(t.mon);
    outFile.print('-');
    if (t.date < 10) outFile.print('0');
    outFile.print(t.date);
    outFile.print(' ');
    if (t.hr < 10) outFile.print('0');
    outFile.print(t.hr);
    outFile.print(':');
    if (t.min < 10) outFile.print('0');
    outFile.print(t.min);
    outFile.print(':');
    if (t.sec < 10) outFile.print('0');
    outFile.print(t.sec);

    for(int i = 0; i < DIM; i++) {
      //outFile.print(DELIM+String(aTotal[i] * w * 0.0039)+DELIM+String(aRange[index(MIN,i,DIM)] * 0.0039)+DELIM+String(aRange[index(MAX,i,DIM)] * 0.0039));
      outFile.print(DELIM);
      dtostrf(aRange[index(TOT,i,DIM)] * w * 0.0039f, SF_LENGTH, SF_PLACES, sBuffer);
      outFile.print(sBuffer); // aTotal[i]
      outFile.print(DELIM);
      dtostrf(aRange[index(MIN,i,DIM)] * 0.0039f, SF_LENGTH, SF_PLACES, sBuffer);
      outFile.print(sBuffer);
      outFile.print(DELIM);
      dtostrf(aRange[index(MAX,i,DIM)] * 0.0039f, SF_LENGTH, SF_PLACES, sBuffer);
      outFile.print(sBuffer);
    }
    for(int i = 0; i < DIM; i++) {
      //outFile.print(DELIM+String(gTotal[i] * w)+DELIM+String(gRange[index(MIN,i,DIM)])+DELIM+String(gRange[index(MAX,i,DIM)]));
      outFile.print(DELIM);
      dtostrf(gRange[index(TOT,i,DIM)] * w, SF_LENGTH, SF_PLACES, sBuffer);
      outFile.print(sBuffer); // gTotal[i]
      outFile.print(DELIM);
      dtostrf(gRange[index(MIN,i,DIM)], SF_LENGTH, SF_PLACES, sBuffer);
      outFile.print(sBuffer);
      outFile.print(DELIM);
      dtostrf(gRange[index(MAX,i,DIM)], SF_LENGTH, SF_PLACES, sBuffer);
      outFile.print(sBuffer);
    }
    for(int i = 0; i < DIM; i++) {
      //outFile.print(DELIM+String(mTotal[i] * w * 0.92)+DELIM+String(mRange[index(MIN,i,DIM)] * 0.92)+DELIM+String(mRange[index(MAX,i,DIM)] * 0.92));
      outFile.print(DELIM);
      dtostrf(mRange[index(TOT,i,DIM)] * w * 0.92f, SF_LENGTH, SF_PLACES, sBuffer);
      outFile.print(sBuffer); // mTotal[i]
      outFile.print(DELIM);
      dtostrf(mRange[index(MIN,i,DIM)] * 0.92f, SF_LENGTH, SF_PLACES, sBuffer);
      outFile.print(sBuffer);
      outFile.print(DELIM);
      dtostrf(mRange[index(MAX,i,DIM)] * 0.92f, SF_LENGTH, SF_PLACES, sBuffer);
      outFile.print(sBuffer);
    }
    
    outFile.println();
    seqnum++;

    // Close the file
    outFile.flush();
    outFile.close();

    samples = 0;
    
    #if DEBUG > 0
      Serial.println(F(" done."));
    #endif
  } else {
    mode = MODE_ERROR;
    // if the file didn't open, print an error:
    #if DEBUG > 0
      Serial.println(F("error opening accelbus.csv"));
    #endif
  }
}

void initializeFile () {
  // Open file
  #if DEBUG > 0
    Serial.print(F("Initializing SD card communications..."));
  #endif
  if (!SD.begin(SD_LINE)) {
    #if DEBUG > 0
      Serial.println(F(" initialization failed!"));
    #endif
  }
  #if DEBUG > 0
    Serial.println(F(" initialization done."));
  #endif

  if (SD.exists(OUT_FILE_NAME)) {
    SD.remove(OUT_FILE_NAME);
  }
  
  // Open the file
  outFile = SD.open(OUT_FILE_NAME, O_CREAT | O_WRITE); //FILE_WRITE);
  
  // if the file opened okay, write to it:
  if (outFile) {
    #if DEBUG > 0
      Serial.print(F("Writing header to accelbus.csv..."));
    #endif

    outFile.print(F("Sequence Number,Date/Time,acc_x_avg,acc_x_min,acc_x_max,acc_y_avg,acc_y_min,acc_y_max,acc_z_avg,acc_z_min,acc_z_max,"));
    outFile.print(F("gyro_x_avg,gyro_x_min,gyro_x_max,gyro_y_avg,gyro_y_min,gyro_y_max,gyro_z_avg,gyro_z_min,gyro_z_max,"));
    outFile.println(F("mgnt_x_avg,mgnt_x_min,mgnt_x_max,mgnt_y_avg,mgnt_y_min,mgnt_y_max,mgnt_z_avg,mgnt_z_min,mgnt_z_max"));
    //  seqnum, date/time, 
    //  acc_x_avg,  acc_x_min,  acc_x_max,  acc_y_avg,  acc_y_min,  acc_y_max,  acc_z_avg,  acc_z_min,  acc_z_max, 
    //  gyro_x_avg, gyro_x_min, gyro_x_max, gyro_y_avg, gyro_y_min, gyro_y_max, gyro_z_avg, gyro_z_min, gyro_z_max, 
    //  mgnt_x_avg, mgnt_x_min, mgnt_x_max, mgnt_y_avg, mgnt_y_min, mgnt_y_max, mgnt_z_avg, mgnt_z_min, mgnt_z_max

    // Close the file
    outFile.close();
    
    #if DEBUG > 0
      Serial.println(F(" done."));
    #endif
    mode = MODE_READY;
  } else {
    mode = MODE_ERROR;
    // if the file didn't open, print an error:
    #if DEBUG > 0
      Serial.println(F("error opening accelbus.csv"));
    #endif
  }
}
