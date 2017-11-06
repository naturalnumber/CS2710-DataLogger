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

// Constants
static String OUT_FILE_NAME = "accelbus.csv";
static long UPDATE_INTERVAL = 2000;
static long DATA_INTERVAL = 20;
static int EXPECTED_ENTRIES = UPDATE_INTERVAL/DATA_INTERVAL;
static String DELIMITER = ",";
static String[] CSV_HEADERS = {"Sequence Number", "Date/Time",
  "acc_x_avg",  "acc_x_min",  "acc_x_max",  "acc_y_avg",  "acc_y_min",  "acc_y_max",  "acc_z_avg",  "acc_z_min",  "acc_z_max", 
  "gyro_x_avg", "gyro_x_min", "gyro_x_max", "gyro_y_avg", "gyro_y_min", "gyro_y_max", "gyro_z_avg", "gyro_z_min", "gyro_z_max", 
  "mgnt_x_avg", "mgnt_x_min", "mgnt_x_max", "mgnt_y_avg", "mgnt_y_min", "mgnt_y_max", "mgnt_z_avg", "mgnt_z_min", "mgnt_z_max"};

// Member variables
File outFile;

// Data stuff
// Data to store: (in this order)
// seqnum, date/time, 
// acc_x_avg,  acc_x_min,  acc_x_max,  acc_y_avg,  acc_y_min,  acc_y_max,  acc_z_avg,  acc_z_min,  acc_z_max, 
// gyro_x_avg, gyro_x_min, gyro_x_max, gyro_y_avg, gyro_y_min, gyro_y_max, gyro_z_avg, gyro_z_min, gyro_z_max, 
// mgnt_x_avg, mgnt_x_min, mgnt_x_max, mgnt_y_avg, mgnt_y_min, mgnt_y_max, mgnt_z_avg, mgnt_z_min, mgnt_z_max


void setup() {
  #if DEBUG > 0
    // For debugging, disable in final
    // Open serial communications and wait for port to open:
    Serial.begin(9600);
  #endif

  
  // Disable the Ethernet interface
  pinMode(ETH_LINE, OUTPUT);
  digitalWrite(ETH_LINE, HIGH);

}

void loop() {
  // put your main code here, to run repeatedly:

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

