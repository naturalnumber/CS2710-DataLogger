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

// Debug control (>0 on, <=0 off)
#define DEBUG 1
// Use running totals or iterative solution
#define USERT 1

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
#define AVG 2
#define TYPES 3


// Constants
static String OUT_FILE_NAME = "accelbus.csv";
static long UPDATE_INTERVAL = 2000;
static long DATA_INTERVAL = 20;
static long SAMPLE_INTERVAL = 2;
static int EXPECTED_ENTRIES = UPDATE_INTERVAL/DATA_INTERVAL*2;
static int EXPECTED_SAMPLES = DATA_INTERVAL/SAMPLE_INTERVAL*2;
static String DELIMITER = ",";
static String[] CSV_HEADERS = {"Sequence Number", "Date/Time",
  "acc_x_avg",  "acc_x_min",  "acc_x_max",  "acc_y_avg",  "acc_y_min",  "acc_y_max",  "acc_z_avg",  "acc_z_min",  "acc_z_max", 
  "gyro_x_avg", "gyro_x_min", "gyro_x_max", "gyro_y_avg", "gyro_y_min", "gyro_y_max", "gyro_z_avg", "gyro_z_min", "gyro_z_max", 
  "mgnt_x_avg", "mgnt_x_min", "mgnt_x_max", "mgnt_y_avg", "mgnt_y_min", "mgnt_y_max", "mgnt_z_avg", "mgnt_z_min", "mgnt_z_max"};

// Member variables
File outFile;
GY_85 GY85;

// Data stuff
// Data to store: (in this order)
// seqnum, date/time, 
// acc_x_avg,  acc_x_min,  acc_x_max,  acc_y_avg,  acc_y_min,  acc_y_max,  acc_z_avg,  acc_z_min,  acc_z_max, 
// gyro_x_avg, gyro_x_min, gyro_x_max, gyro_y_avg, gyro_y_min, gyro_y_max, gyro_z_avg, gyro_z_min, gyro_z_max, 
// mgnt_x_avg, mgnt_x_min, mgnt_x_max, mgnt_y_avg, mgnt_y_min, mgnt_y_max, mgnt_z_avg, mgnt_z_min, mgnt_z_max
int seqnum = 1;
int entries = 0;
//Date[] dates    = new Date[EXPECTED_ENTRIES];
int[][][]   aData = new int[EXPECTED_ENTRIES][TYPES][DIM];
float[][][] gData = new float[EXPECTED_ENTRIES][TYPES][DIM];
int[][][]   mData = new int[EXPECTED_ENTRIES][TYPES][DIM]; // Previously used this * 0.92

// Sampling
int[][]   aRange = new int[TYPES-1][DIM];
float[][] gRange = new float[TYPES-1][DIM];
int[][]   mRange = new int[TYPES-1][DIM];
long[]    aTotal = new long[DIM];
double[]  gTotal = new double[DIM];
long[]    mTotal = new long[DIM];

// Maybe don't need, keep as check right now
int samples = 0;
int[][]   aSample = new int[EXPECTED_SAMPLES][DIM];
float[][] gSample = new float[EXPECTED_SAMPLES][DIM];
int[][]   mSample = new int[EXPECTED_SAMPLES][DIM];

// Temp storage
int[]   a = new int[DIM];
float[] g = new float[DIM];
int[]   m = new int[DIM];
float[] aAvg = new float[DIM];
float[] gAvg = new float[DIM];
float[] mAvg = new float[DIM];

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
  
  // Disable the Ethernet interface
  pinMode(ETH_LINE, OUTPUT);
  digitalWrite(ETH_LINE, HIGH);

}

void loop() {
  // put your main code here, to run repeatedly:

}

void takeEntry() {
  for(int i = 0; i < DIM; i++) {
    for(int j = 0; j < AVG; j++) {
      aData[entries][j][i] = aRange[j][i];
      gData[entries][j][i] = gRange[j][i];
      mData[entries][j][i] = mRange[j][i];
    }

// Todo Here

    #if USERT > 0
      // Use running totals
      aTotal[i] = a[i];
      gTotal[i] = g[i];
      mTotal[i] = m[i];
    #else
      // Use samples
      aSample[0][i] = a[i];
      gSample[0][i] = g[i];
      mSample[0][i] = m[i];
    #endif

    
    aData[entries][AVG][i] = ;
    gData[entries][AVG][i] = ;
    mData[entries][AVG][i] = ;
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
      aRange[MIN][i] = aRange[MAX][i] = a[i];
      gRange[MIN][i] = gRange[MAX][i] = g[i];
      mRange[MIN][i] = mRange[MAX][i] = m[i];

      #if USERT > 0
        // Initialize running totals
        aTotal[i] = a[i];
        gTotal[i] = g[i];
        mTotal[i] = m[i];
      #else
        // Store sample
        aSample[0][i] = a[i];
        gSample[0][i] = g[i];
        mSample[0][i] = m[i];
      #endif
    }
  } else {
    for(int i = 0; i < DIM; i++) {
      if (a[i] < aRange[MIN][i]) aRange[MIN][i] = a[i];
      else if (a[i] > aRange[MAX][i]) aRange[MAX][i] = a[i];
      if (g[i] < gRange[MIN][i]) gRange[MIN][i] = g[i];
      else if (g[i] > gRange[MAX][i]) gRange[MAX][i] = g[i];
      if (m[i] < mRange[MIN][i]) mRange[MIN][i] = m[i];
      else if (m[i] > mRange[MAX][i]) mRange[MAX][i] = m[i];
      
      #if USERT > 0
        // Add to running totals
        aTotal[i] += a[i];
        gTotal[i] += g[i];
        mTotal[i] += m[i];
      #else
        // Store sample
        aSample[samples][i] = a[i];
        gSample[samples][i] = g[i];
        mSample[samples][i] = m[i];
      #endif
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

void printToFile () {
  // Open file
  #if DEBUG > 0
    Serial.print("Initializing SD card communications...");
  #endif
  if (!SD.begin(SD_LINE)) {
    #if DEBUG > 0
      Serial.println(" initialization failed!");
    #endif
    return;
  }
  #if DEBUG > 0
    Serial.println(" initialization done.");
  #endif
  
  // Open the file
  outFile = SD.open(OUT_FILE_NAME, FILE_WRITE);
  
  // if the file opened okay, write to it:
  if (outFile) {
    #if DEBUG > 0
      Serial.print("Writing to "+OUT_FILE_NAME+"...");
    #endif



    // Print data here
    outFile.println("");


    
    // Close the file
    myFile.close();
    
    #if DEBUG > 0
      Serial.println(" done.");
    #endif
  } else {
    // if the file didn't open, print an error:
    #if DEBUG > 0
      Serial.println("error opening "+OUT_FILE_NAME);
    #endif
  }
}

