//ETHAN CLEVELAND | TAMU DBF | 9/17/2025
//150.5 
//THIS CODE IS CREATED TO TAKE IN data FROM THE STRAIN GUAGE, CONVERT THEM TO FORCE IN GRAMS
//AND UPLOAD TO A CSV ON AN SD CARD
//UPLOAD TIME IN FIRST COLLUMN AND FORCE IN SECOND
//NOTE: WILL NEED TO EXPERIMENT TO FIND FORCE CONVERSION
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <HX711.h> //allows to read voltage from amplifier
#include <Wire.h>
//#include <Q2HX711.h> //cheap pressure
#define PX4_I2C_ADDRESS 0x28  // Example I2C address; check the sensor datasheet


// Variables
const int DT_pin = 0; //The pin on the arduino that is connected to the DT pin on the amplifier
const int SCK_pin = 1; //The pin on the arduino that is connected to the SCK pin on the amplifier
const int SD_pin = BUILTIN_SDCARD; //the pin the sd card is connected to

//THESE ARE CHEAP PRESSURE COMMENT OUT IF NOT USING
/*
const int SCK_press1 = 14; //pressure clock pin 1
const int OUT_press1 = 15 //pressure data pin 1
const int SCK_press2 = 16; //pressure clock pin 2
const int OUT_press2 = 17 //pressure data pin 2
Q2HX711 MPS20N0040D1(OUT_press1, SCK_press1); // start comm with the HX710B
Q2HX711 MPS20N0040D2(OUT_press2, SCK_press2); // start comm with the HX710B
*/

//THIS IS IN GRAMS RN
const float conversion_factor = 203.8951801; //GRAMS //The factor to convert voltage to force based on experimental data ( = mean_data_for_known_weight / actual weight) 

unsigned long start_time = 0;
unsigned long end_time = 0;
unsigned long elapsed_time = 0;

File data_file;

HX711 scale; //sets up communication with the scale


//Might need to add a new variable or tweak code if using an arduino with external sd connected.

void setup() {
  Serial.begin(9600); 
  while(!Serial); // allow port to connect


  if(!SD.begin(SD_pin)){  // dont run if sd card not in
    Serial.println("Error: No Card Detected.");
    while(1);
  }
  if(SD.exists("data.csv")) SD.remove("data.csv"); //clear previous file
  data_file = SD.open("data.csv",FILE_WRITE);  // open file
  if(data_file){  //  if file created, set up header
    data_file.println("Time (ms),Grams (g), Pressure (PSI), Temperature(C)"); //change after finding conversion
  }
  else{
    Serial.println("Error: File cannot be opened.");
  }


//I2C AIRSPEED
  Wire.begin();
  Serial.println("Scanning I2C devices...");
  Wire.beginTransmission(28);
  if (Wire.endTransmission() == 0) {
    Serial.print("I2C device found at address 0x");
    Serial.println(28, HEX);
  }
  

  scale.begin(DT_pin,SCK_pin); // create scale
  scale.set_scale(conversion_factor); //  IN GRAMS
  scale.tare(); // tare scale to set at 0

 

  start_time = millis(); // get the start time
}

void raw_data_PX4(uint16_t& raw_pressure, uint16_t& raw_temperature); //reads from wire and updates given values

void loop() {  

  //PRESSURE PX4
  uint16_t raw_pressure = 0;
  uint16_t raw_temperature = 0;
  
  raw_data_PX4(raw_pressure, raw_temperature); //STILL NEED TO CONVERT
  float pressure_psi = (raw_pressure - 16383.1)*2/(16383*.8)+1.25; // conversion is in PSI, accuracy is ±0.25%
  float temp_C = raw_temperature*200.0/2047.0-50;

  //  Read data from strain gauge at this time
  double data = scale.get_units(1); 
  

  //TIME
  end_time = millis();
  elapsed_time = end_time - start_time; //  get timestamp

  //MPS Pressure Read
  /*
  int pressure_mps1 = MPS20N0040D1.read();
  int pressure_mps2 = MPS20N0040D1.read();
  */

  //write data
  if(!raw_pressure == 0){
    String data_string = String(elapsed_time) + "," + String(data) + "," + String(pressure_psi)+","+ String(temp_C);
    data_file.println(data_string);
    Serial.println(data_string);
  }
  


  //break condition
  if(elapsed_time >= 10000){ // change later
    data_file.close(); // saves to file
    Serial.println("Recording Ended.");
    while(true);
  }
  delay(10);
}


void raw_data_PX4(uint16_t& raw_pressure, uint16_t& raw_temperature){
  //Data Transmitts in I2C, meaning 8 bits at a time, so a 14 bit output needs two read requests
  //  Get Data From Pressure Sensor
  Wire.beginTransmission(PX4_I2C_ADDRESS);
  Wire.endTransmission(false);
  Wire.requestFrom(PX4_I2C_ADDRESS, 4); // requests four 
  Wire.endTransmission(false);
  if(Wire.available() == 4){
    uint8_t pressure_MSB = Wire.read(); // Status | Pressure MSB (14-bit)
    uint8_t pressure_LSB = Wire.read(); // Pressure LSB
    uint8_t temperature_MSB= Wire.read(); // Temperature MSB (11-bit)
    uint8_t temperature_LSB = Wire.read(); // Temperature LSB | CRC

/*DEBUGGING*/
    //Serial.println("Pressure "+String(pressure_MSB)+" "+String(pressure_LSB));



    //now we must combine the binary numbers
    //first two are status bits, get rid of them
    //bitwise AND with a mask of 00111111, AND cancels out first two, and keeps last 6 the same
    raw_pressure = pressure_MSB & 0x3F; //binary for 00111111
    //now shift over 8 bits, first two shifts get rid of zeroes, last 6 move up, and bitwise OR with LSB
    //that makes 14
    raw_pressure = raw_pressure << 8;
    raw_pressure |= pressure_LSB;

    //do the similar for the temperature
    raw_temperature = (temperature_MSB << 3) | (temperature_LSB >> 5);

  }
  else{
    //Serial.println("No data values.");
  }
}
