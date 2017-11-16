/*
SD card read/write
* SD card attached to SPI bus as follows:
** MOSI - pin 11
** MISO - pin 12
** CLK - pin 13
** CS - pin 4
*/

// Includes
#include <SPI.h>
#include <SD.h>
#include <GY_85.h>
#include <Wire.h>
#include <IRremote.h>

// Debug control (>0 on, <=0 off)
#define DEBUG 1

// Not usable, for reference
#define SD_MOSI 11
#define SD_MISO 12
#define SD_CLK 13

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
// LEDs
#define RED_LED 3
#define YELLOW_LED 5
#define GREEN_LED 6
// Array Constants
#define X 0
#define Y 1
#define Z 2
#define DIM 3
#define MIN 0
#define MAX 1
#define TYPES 2

#define WRITE_INTERVAL 10
#define SAMPLE_INTERVAL 2

#define RANGE_SIZE TYPES*DIM
#define SAMPLE_SIZE EXPECTED_SAMPLES*DIM

// TODO: fill this in with the actual IR button
#define OK_HEX "ff02fd"

// Constants
//static String OUT_FILE_NAME = "accelbus.csv";
static char DELIM = ',';

// Member variables
File outFile;
GY_85 GY85;

int   seqnum = 1;

// Sampling
short   aRange[RANGE_SIZE];
float   gRange[RANGE_SIZE];
short   mRange[RANGE_SIZE];
short   aTotal[DIM];
float   gTotal[DIM];
short   mTotal[DIM];

// Maybe don't need, keep as check right now
short   samples = 0;

// Temp storage
short   a[DIM];
float   g[DIM];
short   m[DIM];

unsigned long now;
unsigned long lastSample; 
unsigned long lastEntry;
unsigned long lastWrite;

// IR setup
IRrecv irrecv(IR_RECV_PIN);
decode_results results;
boolean isEnabled;

void setup() {
  #if DEBUG > 0
    // For debugging, disable in final
    // Open serial communications and wait for port to open:
    Serial.begin(9600);
  #endif
  // Initialize GY_85
  Wire.begin();
  delay(10);
  GY85.init();
  delay(10);

  // Initialize the IR sensor
  irrecv.enableIRIn();
  
  // Disable the Ethernet interface
  pinMode(ETH_LINE, OUTPUT);
  digitalWrite(ETH_LINE, HIGH);
  
  now = millis();
  lastSample = now; 
  lastEntry = now;
  lastWrite = now;

  initializeFile();
}

void loop() {
  if(isEnabled)
  {
    now = millis();
    if (now - lastSample > SAMPLE_INTERVAL) {
      takeSample();
      lastSample = millis();
    }
    if (now - lastWrite > WRITE_INTERVAL) {
      writeToFile();
      lastSample = millis();
    }
  }

  checkEnabled();
  // sample code delays for processing IR device input
  // dont know if it's needed
  // might be nice to get on a timer but who knows
  delay(200);
}

void checkEnabled()
{
  if (irrecv.decode(&results))
  {
    if (String(results.value, HEX).equals(OK_HEX))
    {
      isEnabled = !isEnabled;
      irrecv.resume();
    }
  }
}



int index(int i, int j, int J, int k, int K) {
  return (i*J+j)*K+k;
}

int index(int i, int j, int J) {
  return i*J+j;
}

void takeSample() {
  #if DEBUG > 0
    Serial.println("Taking sample "+String(samples));
  #endif
  
  a[X] = GY85.accelerometer_x(GY85.readFromAccelerometer());
  a[Y] = GY85.accelerometer_y(GY85.readFromAccelerometer());
  a[Z] = GY85.accelerometer_z(GY85.readFromAccelerometer());
  g[X] = GY85.gyro_x(GY85.readGyro());
  g[Y] = GY85.gyro_y(GY85.readGyro());
  g[Z] = GY85.gyro_z(GY85.readGyro());
  //float gt = GY85.temp  (GY85.readGyro());
  m[X] = GY85.compass_x(GY85.readFromCompass()); // Want the *0.92??????
  m[Y] = GY85.compass_y(GY85.readFromCompass());
  m[Z] = GY85.compass_z(GY85.readFromCompass());

  if (samples == 0) { // Initialize if first sample
    for(int i = 0; i < DIM; i++) {
      aRange[index(MIN,i,DIM)] = aRange[index(MAX,i,DIM)] = a[i];
      gRange[index(MIN,i,DIM)] = gRange[index(MAX,i,DIM)] = g[i];
      mRange[index(MIN,i,DIM)] = mRange[index(MAX,i,DIM)] = m[i];

      // Initialize running totals
      aTotal[i] = a[i];
      gTotal[i] = g[i];
      mTotal[i] = m[i];
    }
  } else {
    for(int i = 0; i < DIM; i++) {
      if (a[i] < aRange[index(MIN,i,DIM)]) aRange[index(MIN,i,DIM)] = a[i];
      else if (a[i] > aRange[index(MAX,i,DIM)]) aRange[index(MAX,i,DIM)] = a[i];
      if (g[i] < gRange[index(MIN,i,DIM)]) gRange[index(MIN,i,DIM)] = g[i];
      else if (g[i] > gRange[index(MAX,i,DIM)]) gRange[index(MAX,i,DIM)] = g[i];
      if (m[i] < mRange[index(MIN,i,DIM)]) mRange[index(MIN,i,DIM)] = m[i];
      else if (m[i] > mRange[index(MAX,i,DIM)]) mRange[index(MAX,i,DIM)] = m[i];
      
      // Add to running totals
      aTotal[i] += a[i];
      gTotal[i] += g[i];
      mTotal[i] += m[i];
    }
  }
  samples++;
}

void writeToFile () {
  // Open file
  #if DEBUG > 0
    Serial.print(F("Initializing SD card communications..."));
  #endif
  if (!SD.begin(SD_LINE)) {
    #if DEBUG > 0
      Serial.println(F(" initialization failed!"));
    #endif
    return;
  }
  #if DEBUG > 0
    Serial.println(F(" initialization done."));
  #endif
  
  // Open the file
  outFile = SD.open(F("accelbus.csv"), FILE_WRITE);
  
  // if the file opened okay, write to it:
  if (outFile) {
    #if DEBUG > 0
      Serial.print(F("Writing to accelbus.csv..."));
    #endif

    float w = 1.0 / samples;
    //  seqnum, date/time, 
    //  acc_x_avg,  acc_x_min,  acc_x_max,  acc_y_avg,  acc_y_min,  acc_y_max,  acc_z_avg,  acc_z_min,  acc_z_max, 
    //  gyro_x_avg, gyro_x_min, gyro_x_max, gyro_y_avg, gyro_y_min, gyro_y_max, gyro_z_avg, gyro_z_min, gyro_z_max, 
    //  mgnt_x_avg, mgnt_x_min, mgnt_x_max, mgnt_y_avg, mgnt_y_min, mgnt_y_max, mgnt_z_avg, mgnt_z_min, mgnt_z_max

    outFile.print(String(seqnum));//+DELIM+date/Time

    for(int i = 0; i < DIM; i++) {
      outFile.print(DELIM+String(aTotal[i] * w)+DELIM+String(aRange[index(MIN,i,DIM)])+DELIM+String(aRange[index(MAX,i,DIM)]));
    }
    for(int i = 0; i < DIM; i++) {
      outFile.print(DELIM+String(gTotal[i] * w)+DELIM+String(gRange[index(MIN,i,DIM)])+DELIM+String(gRange[index(MAX,i,DIM)]));
    }
    for(int i = 0; i < DIM; i++) {
      outFile.print(DELIM+String(mTotal[i] * w * 0.92)+DELIM+String(mRange[index(MIN,i,DIM)] * 0.92)+DELIM+String(mRange[index(MAX,i,DIM)] * 0.92));
    }
    seqnum++;

    // Close the file
    outFile.close();
    
    #if DEBUG > 0
      Serial.println(F(" done."));
    #endif
  } else {
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
    return;
  }
  #if DEBUG > 0
    Serial.println(F(" initialization done."));
  #endif
  
  // Open the file
  outFile = SD.open(F("accelbus.csv"), FILE_WRITE);
  
  // if the file opened okay, write to it:
  if (outFile) {
    #if DEBUG > 0
      Serial.print(F("Writing to accelbus.csv..."));
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
  } else {
    // if the file didn't open, print an error:
    #if DEBUG > 0
      Serial.println(F("error opening accelbus.csv"));
    #endif
  }
}

/*  OLD
 *   
//#define ENTRY_INTERVAL 10
//#define EXPECTED_ENTRIES WRITE_INTERVAL/ENTRY_INTERVAL*2
//#define EXPECTED_SAMPLES ENTRY_INTERVAL/SAMPLE_INTERVAL*2
//#define DATA_SIZE EXPECTED_ENTRIES*TYPES*DIM
//#define AVERAGE_SIZE EXPECTED_ENTRIES*TYPES*DIM

#define WRITE_INTERVAL 20
#define ENTRY_INTERVAL 10
#define SAMPLE_INTERVAL 2

#define EXPECTED_ENTRIES WRITE_INTERVAL/ENTRY_INTERVAL*2
#define EXPECTED_SAMPLES ENTRY_INTERVAL/SAMPLE_INTERVAL*2
#define DATA_SIZE EXPECTED_ENTRIES*TYPES*DIM
#define AVERAGE_SIZE EXPECTED_ENTRIES*TYPES*DIM
#define RANGE_SIZE TYPES*DIM
#define SAMPLE_SIZE EXPECTED_SAMPLES*DIM

//short entries = 0;
//Date[] dates    = new Date[EXPECTED_ENTRIES];
//short   aData[DATA_SIZE]; //[DATA_SIZE]; //[][][] = new int[EXPECTED_ENTRIES][TYPES][DIM];
//float   gData[DATA_SIZE]; //[][][] = new float[EXPECTED_ENTRIES][TYPES][DIM];
//short   mData[DATA_SIZE]; //[][][] = new int[EXPECTED_ENTRIES][TYPES][DIM]; // Previously used this * 0.92

//float   aAverage[AVERAGE_SIZE]; //[][] = new float[EXPECTED_ENTRIES][DIM];
//float   gAverage[AVERAGE_SIZE]; //[][] = new float[EXPECTED_ENTRIES][DIM];
//float   mAverage[AVERAGE_SIZE]; //[][] = new float[EXPECTED_ENTRIES][DIM]; // Previously used this * 0.92


//Date[] dates    = new Date[EXPECTED_ENTRIES];
//short   aData[DATA_SIZE]; //[DATA_SIZE]; //[][][] = new int[EXPECTED_ENTRIES][TYPES][DIM];
//float   gData[DATA_SIZE]; //[][][] = new float[EXPECTED_ENTRIES][TYPES][DIM];
//short   mData[DATA_SIZE]; //[][][] = new int[EXPECTED_ENTRIES][TYPES][DIM]; // Previously used this * 0.92

//float   aAverage[AVERAGE_SIZE]; //[][] = new float[EXPECTED_ENTRIES][DIM];
//float   gAverage[AVERAGE_SIZE]; //[][] = new float[EXPECTED_ENTRIES][DIM];
//float   mAverage[AVERAGE_SIZE]; //[][] = new float[EXPECTED_ENTRIES][DIM]; // Previously used this * 0.92
//int   aSample[SAMPLE_SIZE]; //[][] = new int[EXPECTED_SAMPLES][DIM];
//float gSample[SAMPLE_SIZE]; //[][] = new float[EXPECTED_SAMPLES][DIM];
//int   mSample[SAMPLE_SIZE]; //[][] = new int[EXPECTED_SAMPLES][DIM];
//float[] aAvg = new float[DIM];
//float[] gAvg = new float[DIM];
//float[] mAvg = new float[DIM];

void takeEntry() {
  float w = 1.0 / samples;
  for(int i = 0; i < DIM; i++) {
    for(int j = 0; j < TYPES; j++) {
      aData[index(entries,j,TYPES,i,DIM)] = aRange[index(j,i,DIM)];
      gData[index(entries,j,TYPES,i,DIM)] = gRange[index(j,i,DIM)];
      mData[index(entries,j,TYPES,i,DIM)] = mRange[index(j,i,DIM)];
    }
    
    aAverage[index(entries,i,DIM)] = aTotal[i] * w ;
    gAverage[index(entries,i,DIM)] = gTotal[i] * w ;
    mAverage[index(entries,i,DIM)] = mTotal[i] * w ;
  }
  samples = 0;
  entries++;
  
  if (entries == EXPECTED_ENTRIES) { // Could be >=
    writeToFile();
    
    #if DEBUG > 0
      Serial.println("Entry data buffer full, emergency dump!");
    #endif
  }
}

void takeSample() {
  #if DEBUG > 0
    Serial.println("Taking sample "+String(samples));
  #endif
  
  a[X] = GY85.accelerometer_x(GY85.readFromAccelerometer());
  a[Y] = GY85.accelerometer_y(GY85.readFromAccelerometer());
  a[Z] = GY85.accelerometer_z(GY85.readFromAccelerometer());
  g[X] = GY85.gyro_x(GY85.readGyro());
  g[Y] = GY85.gyro_y(GY85.readGyro());
  g[Z] = GY85.gyro_z(GY85.readGyro());
  //float gt = GY85.temp  (GY85.readGyro());
  m[X] = GY85.compass_x(GY85.readFromCompass()); // Want the *0.92??????
  m[Y] = GY85.compass_y(GY85.readFromCompass());
  m[Z] = GY85.compass_z(GY85.readFromCompass());

  if (samples == 0) { // Initialize if first sample
    for(int i = 0; i < DIM; i++) {
      aRange[index(MIN,i,DIM)] = aRange[index(MAX,i,DIM)] = a[i];
      gRange[index(MIN,i,DIM)] = gRange[index(MAX,i,DIM)] = g[i];
      mRange[index(MIN,i,DIM)] = mRange[index(MAX,i,DIM)] = m[i];

      // Initialize running totals
      aTotal[i] = a[i];
      gTotal[i] = g[i];
      mTotal[i] = m[i];
    }
  } else {
    for(int i = 0; i < DIM; i++) {
      if (a[i] < aRange[index(MIN,i,DIM)]) aRange[index(MIN,i,DIM)] = a[i];
      else if (a[i] > aRange[index(MAX,i,DIM)]) aRange[index(MAX,i,DIM)] = a[i];
      if (g[i] < gRange[index(MIN,i,DIM)]) gRange[index(MIN,i,DIM)] = g[i];
      else if (g[i] > gRange[index(MAX,i,DIM)]) gRange[index(MAX,i,DIM)] = g[i];
      if (m[i] < mRange[index(MIN,i,DIM)]) mRange[index(MIN,i,DIM)] = m[i];
      else if (m[i] > mRange[index(MAX,i,DIM)]) mRange[index(MAX,i,DIM)] = m[i];
      
      // Add to running totals
      aTotal[i] += a[i];
      gTotal[i] += g[i];
      mTotal[i] += m[i];
    }
  }
  samples++;

  // Emergency data buffer overflow prevention, shouldn't trigger
  if (samples == EXPECTED_SAMPLES) { // Could be >=
    takeEntry();
    
    #if DEBUG > 0
      Serial.println("Sample data buffer full, emergency dump!");
    #endif
  }
}


    /*
    for (int i = 0; i < entries; i++) {
      outFile.print(String(seqnum));//+DELIM+date/Time

      for(int j = 0; j < DIM; j++) {
        outFile.print(DELIM+String(aAverage[index(i,j,DIM)])+DELIM+String(aData[index(i,MIN,TYPES,j,DIM)])+DELIM+String(aData[index(i,MAX,TYPES,j,DIM)]));
      }
      for(int j = 0; j < DIM; j++) {
        outFile.print(DELIM+String(gAverage[index(i,j,DIM)])+DELIM+String(gData[index(i,MIN,TYPES,j,DIM)])+DELIM+String(gData[index(i,MAX,TYPES,j,DIM)]));
      }
      for(int j = 0; j < DIM; j++) {
        outFile.print(DELIM+String(mAverage[index(i,j,DIM)])+DELIM+String(mData[index(i,MIN,TYPES,j,DIM)])+DELIM+String(mData[index(i,MAX,TYPES,j,DIM)]));
      }
      seqnum++;
    }
 */
