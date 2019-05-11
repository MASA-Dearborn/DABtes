#include <Adafruit_BME280.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_GPS.h>
#include <Adafruit_Sensor.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <utility/imumaths.h> //bno
#include <SD.h> //use SDFat library to format new SD cards
#include <SPI.h>
#include <XBee.h>

//Teensy 3.5 and 3.6 SD Card
const int chipSelect = BUILTIN_SDCARD;

//create SD card files
File BMEData;
File BNOData;
File GPSData;

//define sea level pressure for accurate altitude calulations
#define SEALEVELPRESSURE_HPA (1013.25)

//pin config. using last years board to test
#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10


#define BNO055_SAMPLERATE_DELAY_MS 100

//I2C setup for BNO and BME
Adafruit_BNO055 bno = Adafruit_BNO055(55);
Adafruit_BME280 bme;

//hardware serial setup for GPS
HardwareSerial GPSSerial = Serial5;
Adafruit_GPS GPS(&GPSSerial);

//Hardware serial
uint32_t timer = millis(); //timer GPS

//GPSSerial teensy hardware serial port
IntervalTimer myTimer1;
IntervalTimer myTimer2;
IntervalTimer myTimer3;

void setup() {
  Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");
  //test is file was created
  GPSData = SD.open("GPS.txt", FILE_WRITE);
  if (GPSData) {
    Serial.print("Writing to test.txt...");
    GPSData.println("GPS data Write");
    GPSData.close(); // close the file:
    Serial.println("GPS data done.");
  } else {
    Serial.println("error opening GPS.txt");
  }
  BNOData = SD.open("BNO.txt", FILE_WRITE);
  if (BNOData) {
    Serial.print("Writing to test.txt...");
    BNOData.println("BNO data Write");
    BNOData.println("orientation,gyroscope,accelerometer,LinearAcceleration");
    BNOData.close(); // close the file:
    Serial.println("BNO data done.");
  }
  else {
    Serial.println("error opening BNO.txt");
  }
  BMEData = SD.open("BME.txt", FILE_WRITE);
  if (BMEData) {
    Serial.print("Writing to test.txt...");
    BMEData.println("BME data Write");
    BMEData.close();
    Serial.println("BME data done. Temprature (C), Pressure (hPa), Altitude (m), Humidity (%)");
  } else {
    Serial.println("error opening BME.txt");
  }
  GPSSerial.begin(9600); //GPS.begin(9600)
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA); //RMC (recommended minimum) and GGA (fix data) including altitude
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY); //"minimum recommended" data
  // For parsing data, we don't suggest using anything but either RMC only or RMC+GGA since
  // the parser doesn't care about other sentences at this time
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); // 1 Hz update rate For parsing, we don't suggest using anything higher than 1 Hz
  GPS.sendCommand(PGCMD_ANTENNA); // optional: updates on antenna status
  delay(1000);
  GPSSerial.println(PMTK_Q_RELEASE); //firmware realease info

  //start communication with BNO
  Serial.println("BNO055 Test"); Serial.println("");
  if (!bno.begin()) {
    Serial.print("Could not find a BNO055 sensor... Check your wiring or I2C ADDR!");
    return;
  }
  delay(1000);
  displaySensorDetails(); //optional
  displaySensorStatus(); //optional
  bno.setExtCrystalUse(true); //external timer

  Serial.begin(9600);
  Serial.println(F("BME280 Test"));
  if (!bme.begin()) {
    Serial.println("Could not find a valid BME280 sensor... Check your wiring or I2C ADDR!");
    return;
  }
  else {
    Serial.println("DEFAULT TEST");
    Serial.println();
    delay(1000); // let sensor boot up
    myTimer1.begin(readBNO, 150000);  // micros: readBNO to run every .15 seconds
    delay(1000); // let sensor boot up
    myTimer2.begin(readGPS, 100000);  // micros: readGPS to run every 1 seconds
    delay(1000); // let sensor boot up
    myTimer3.begin(readBME, 150000);  // micros: readBME to run every .15 seconds
  }
}

void loop() {
  //last year possibly ran GPS.read(); here
}

void displaySensorDetails()
{
  sensor_t sensor;
  bno.getSensor(&sensor);
  Serial.print  ("Sensor:       ");
  Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   ");
  Serial.println(sensor.version);
  Serial.print  ("Unique ID:    ");
  Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    ");
  Serial.print(sensor.max_value);
  Serial.println(" xxx");
  Serial.print  ("Min Value:    ");
  Serial.print(sensor.min_value);
  Serial.println(" xxx");
  Serial.print  ("Resolution:   ");
  Serial.print(sensor.resolution);
  Serial.println(" xxx");
  Serial.println("");
  delay(1000);
}

void displaySensorStatus() //optional
{
  /* Get the system status values (mostly for debugging purposes) */
  uint8_t system_status, self_test_results, system_error;
  system_status = self_test_results = system_error = 0;
  bno.getSystemStatus(&system_status, &self_test_results, &system_error);

  /* Display the results in the Serial Monitor */
  Serial.println("");
  Serial.print("System Status: 0x");
  Serial.println(system_status, HEX);
  Serial.print("Self Test:     0x");
  Serial.println(self_test_results, HEX);
  Serial.print("System Error:  0x");
  Serial.println(system_error, HEX);
  Serial.println("");
  delay(1000);
}

void displayCalStatus() //optional
{
  /* Get the four calibration values (0..3) */
  /* Any sensor data reporting 0 should be ignored, */
  /* 3 means 'fully calibrated" */
  uint8_t system, gyro, accel, mag;
  system = gyro = accel = mag = 0;
  bno.getCalibration(&system, &gyro, &accel, &mag);

  /* The data should be ignored until the system calibration is > 0 */
  if (!system)
  {
    Serial.println("System not calibrated!");
  }
  else
  {
    /* Display the individual values */
    Serial.print("Sys:");
    Serial.print(system, HEX); //DEC or HEX
    Serial.print(" G:");
    Serial.print(gyro, HEX);
    Serial.print(" A:");
    Serial.print(accel, HEX);
    Serial.print(" M:");
    Serial.println(mag, HEX);
  }
}

void readBNO() {
  File BNOData = SD.open("BNO.txt", FILE_WRITE);
  /* Get a new sensor event */
  sensors_event_t event;
  bno.getEvent(&event);
  double dataX = event.orientation.x;
  double dataY = event.orientation.y;
  double dataZ = event.orientation.z;
  imu::Vector<3> gyro = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
  imu::Vector<3> Accel_Meter = bno.getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
  imu::Vector<3> Linear_Accel = bno.getVector(Adafruit_BNO055::VECTOR_LINEARACCEL);
  /* Display the floating point data */
  Serial.print("Orientation X: ");
  Serial.print(dataX, 4);
  Serial.print("\tY: ");
  Serial.print(dataY, 4);
  Serial.print("\tZ: ");
  Serial.print(dataZ, 4);
  Serial.println("");
  Serial.print("Gyro X: ");
  Serial.print(gyro.x());
  Serial.print("\tY: ");
  Serial.print(gyro.y());
  Serial.print("\tZ: ");
  Serial.print(gyro.z());
  Serial.println("");
  Serial.print("Accel_Meter X: ");
  Serial.print(Accel_Meter.x());
  Serial.print("\tY: ");
  Serial.print(Accel_Meter.y());
  Serial.print("\tZ: ");
  Serial.print(Accel_Meter.z());
  Serial.println("");
  Serial.print("Linear_Accel X: ");
  Serial.print(Accel_Meter.x());
  Serial.print("\tY: ");
  Serial.print(Accel_Meter.y());
  Serial.print("\tZ: ");
  Serial.print(Accel_Meter.z());
  Serial.println("");
  Serial.println();
  //log to SD card
  BNOData.print(dataX, 4);
  BNOData.print(",");
  BNOData.print(dataY, 4);
  BNOData.print(",");
  BNOData.print(dataZ, 4);
  BNOData.print(",");
  BNOData.print(gyro.x());
  BNOData.print(",");
  BNOData.print(gyro.y());
  BNOData.print(",");
  BNOData.print(gyro.z());
  BNOData.print(",");
  BNOData.print(Accel_Meter.x());
  BNOData.print(",");
  BNOData.print(Accel_Meter.y());
  BNOData.print(",");
  BNOData.print(Accel_Meter.z());
  BNOData.print(",");
  BNOData.print(Accel_Meter.x());
  BNOData.print(",");
  BNOData.print(Accel_Meter.y());
  BNOData.print(",");
  BNOData.println(Accel_Meter.z());
  /* Wait the specified delay before requesting nex data */
  delay(BNO055_SAMPLERATE_DELAY_MS);
  BNOData.close();
}

void readGPS() {
  File GPSData = SD.open("GPS.txt", FILE_WRITE);
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences!
    Serial.println(GPS.lastNMEA()); // this also sets the newNMEAreceived() flag to false
    if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
      return; // we can fail to parse a sentence in which case we should just wait for another
  }
  // if millis() or timer wraps around, we'll just reset it
  if (timer > millis()) timer = millis();
  // approximately every 2 seconds or so, print out the current stats
  if (millis() - timer > 2000) {
    timer = millis(); // reset the timer
    //show availible data in serial monitor
    //Serial.print(GPS.read());
    Serial.print("\nTime: ");
    Serial.print(GPS.hour, DEC);
    Serial.print(':');
    Serial.print(GPS.minute, DEC);
    Serial.print(':');
    Serial.print(GPS.seconds, DEC);
    Serial.print('.');
    Serial.println(GPS.milliseconds);
    Serial.print("Date: ");
    Serial.print(GPS.day, DEC);
    Serial.print('/');
    Serial.print(GPS.month, DEC);
    Serial.print("/20");
    Serial.println(GPS.year, DEC);
    Serial.print("Fix: ");
    Serial.print((int)GPS.fix);
    Serial.print("\nQuality: ");
    Serial.println((int)GPS.fixquality);
    Serial.println();
    Serial.print("Location: ");
    Serial.print(GPS.latitude, 4);
    Serial.print(GPS.lat);
    Serial.print(", ");
    Serial.print(GPS.longitude, 4);
    Serial.println(GPS.lon);
    Serial.print("Speed (knots): ");
    Serial.println(GPS.speed);
    Serial.print("Angle: ");
    Serial.println(GPS.angle);
    Serial.print("Altitude: ");
    Serial.println(GPS.altitude);
    Serial.print("Satellites: ");
    Serial.println((int)GPS.satellites);
    Serial.println();
    //log to sd card
    GPSData.print(GPS.read());
    GPSData.print("Location: ");
    GPSData.print(GPS.latitude, 4);
    GPSData.print(GPS.lat);
    GPSData.print(", ");
    GPSData.print(GPS.longitude, 4);
    GPSData.println(GPS.lon);
    GPSData.print("Speed (knots): ");
    GPSData.println(GPS.speed);
    GPSData.print("Angle: ");
    GPSData.println(GPS.angle);
    GPSData.print("Altitude: ");
    GPSData.println(GPS.altitude);
    GPSData.print("Satellites: ");
    GPSData.println((int)GPS.satellites);
    GPSData.close();
  }
}
void readBME() {
  File BMEData = SD.open("BME.txt", FILE_WRITE);
  //temp. press. alt. and humi. as double
  double temperature = bme.readTemperature();
  double pressure = bme.readPressure();
  double alt = bme.readAltitude(SEALEVELPRESSURE_HPA);
  double humidity = bme.readHumidity();
  //view in serial monitor
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println(" *C");
  Serial.print("Pressure = ");
  Serial.print(pressure / 100.0F);
  Serial.println(" hPa");
  Serial.print("Approx. Altitude = ");
  Serial.print(alt);
  Serial.println(" m");
  Serial.print("Humidity = ");
  Serial.print(humidity);
  Serial.println(" %");
  Serial.println();
  //log to sd card
  BMEData.print(temperature);
  BMEData.print(",");
  BMEData.print(pressure / 100.0F);
  BMEData.print(",");
  BMEData.print(alt);
  BMEData.print(",");
  BMEData.print(humidity);
  BMEData.close();
}


//????????
/*
  Xbee.print(BMEData);
  Xbee.print(BNOData);
  Xbee.print(GPSData);
*/
