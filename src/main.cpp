#include <sstream>
#include <Arduino.h>
#include <EspMQTTClient.h>
#include <MyDateTime.h>
#include "Logger.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <InfluxDbClient.h>

// used functions
void WriteConnectionEstablished2Database(bool status);

boolean relaisStat;   // Actual state of the relay. 1 := On ; 0 := Off


#define SPLIT_TAB(x) split(x,'\t')

//#define TEST_MODE

//***************** Begin operation parameters *********************************
#ifdef TEST_MODE
  #define MQTT_DEBUG true

  #define TEMP_CHECK_PERIODE 1 * 1000 // 6 sec
  #define PUMP_ON_PERIODE_5M  5 * 1000 // 5 sec
  #define PUMP_OFF_PERIODE_15M 15 * 1000 // 15 sec
  #define PUMP_OFF_PERIODE_25M 25 * 1000 // 25 sec
  #define PUMP_OFF_PERIODE_55M 55  * 1000 // 55 sec
  #define PUMP_OFF_PERIODE_24H 24 * 60 * 1000 // 24 min
  #define TEMP_CHANGE_PERIOD 1 * 60 * 1000 // 60s
  #define TEMP_CHANGE_INTERVALL -1
  ulong _nextTempChangeCheckTime;
  #define CHECK_TEMP_CHANGE _nextTempChangeCheckTime < millis()
  #define SET_NEXT_TEMP_CHANGE_TIME _nextTempChangeCheckTime = millis() + TEMP_CHANGE_PERIOD
  float _currentTemp = -5;
  float _tempChangeIntervall = TEMP_CHANGE_INTERVALL;
  #define MIN_TEST_TEMP -10
  #define MAX_TEST_TEMP -3
#else
  #define MQTT_DEBUG false
  #define TEMP_CHECK_PERIODE 6 * 60 * 1000 // 6 Min
  #define PUMP_ON_PERIODE_5M  5 * 60 * 1000 // 5 Min
  #define PUMP_OFF_PERIODE_15M 15 * 60 * 1000 // 15 Min
  #define PUMP_OFF_PERIODE_25M 25 * 60 * 1000 // 25 Min
  #define PUMP_OFF_PERIODE_55M 55 * 60 * 1000 // 55 Min
  #define PUMP_OFF_PERIODE_24H 24 * 60 * 60 * 1000 // 24 hours
#endif

// Define the ports of the LED´s
#define LED_RED_PIN 32
#define LED_GREEN_PIN 33

ulong _tempCheckPeriode = TEMP_CHECK_PERIODE;

ulong _pumpOnPeriod5M = PUMP_ON_PERIODE_5M;
ulong _pumpOffPeriode15M = PUMP_OFF_PERIODE_15M;
ulong _pumpOffPeriode25M = PUMP_OFF_PERIODE_25M;
ulong _pumpOffPeriode55M = PUMP_OFF_PERIODE_55M;
ulong _pumpOffPeriode24H = PUMP_OFF_PERIODE_24H;

//***************** End operation parameters *********************************

#define ON true
#define OFF false

using namespace std;
// Name of this Module for logging and MQTT
#define MY_NAME "ESPFG"
// Baudrate of the serial interface to the computer
#define SerialDataBits0 115200

// Logger, send the data to the computer over serial and over the MQTT.
Logger logger;

//***************** Begin Relais *********************************
#define relaisPin 13
//***************** End Relais *********************************

//***************** Begin DS18B20 *********************************
// GPIO where the DS18B20 is connected to
const int oneWireBus = 5;     

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

float getTemperature(){
  #ifdef TEST_MODE
    return _currentTemp;
  #else
    sensors.requestTemperatures(); 
    return sensors.getTempCByIndex(0);
  #endif
}

//***************** End DS18B20 *********************************

//***************** Begin DateTime Setup *********************************
void setupDateTime() {
  // setup this after wifi connected
  // Set TimeZone to UTC
  DateTime.setTimeZone("UTC");
  // this method config ntp and wait for time sync
  // default timeout is 10 seconds
  DateTime.begin(/* timeout param */);
  if (!DateTime.isTimeValid()) {
    logger.printError("Failed to get time from server.");
  }
}
//***************** End DateTime Setup *********************************


//***************** Begin MQTT *********************************

#ifdef TEST_MODE
void setTemperatureHandler(const String &message){
  try
  {
    float newTemperature;
    istringstream(message.c_str()) >> newTemperature;
    _currentTemp = newTemperature;
    // Next autmatic temperature update is in 24h
    _nextTempChangeCheckTime = millis() + 24*60*60*1000; 
  }
  catch(const std::exception& e)
  {
    logger.printError("Failed set the temperature with this value: '%s'::  %s", message, e.what());
  }
}
#endif

EspMQTTClient mqttClient(
  "PothornWelle", // WLAN SID
  "Sguea@rbr", // WLAN PW
  "192.168.2.20",   // MQTT Broker server ip on OpenHab PI
  "",             // Can be omitted if not needed
  "",             // Can be omitted if not needed
  MY_NAME         // Client name that uniquely identify your device
);

// This function is called once everything is connected (Wifi and MQTT)
// WARNING : YOU MUST IMPLEMENT IT IF YOU USE EspMQTTClient
void onConnectionEstablished() {
  setupDateTime();
  if(DateTime.isTimeValid() == false){
    logger.printError("Date/Time is not valid, restart the board...");
    ESP.restart();
  }

#ifdef TEST_MODE
  string myName = MY_NAME;
  string topic = myName + "/SetTemperature";
  mqttClient.subscribe(topic.c_str(), setTemperatureHandler);
#endif

  logger.printMessage("Current UTC time: %s\n",DateTime.toISOString().c_str());

  // LED red off and LED green on
  digitalWrite (LED_RED_PIN, LOW);	
  digitalWrite (LED_GREEN_PIN, HIGH);	
  
  // Write the connection established count into the influx DB
  WriteConnectionEstablished2Database(relaisStat);
}


void PublishRelaisStatus(bool relaisStat){
  if(mqttClient.isConnected()){
    string topic = "Switch/Pump/Groundwater/";
    topic += MY_NAME;
    string payload = "";
    if(relaisStat == ON){
      payload += "ON";
    }else{
      payload += "OFF";
    }

    mqttClient.publish(topic.c_str(),payload.c_str(), true);
  }
}

void PublishTemperatur(float temperatureC){
  if(mqttClient.isConnected()){
    char buff[80];
    snprintf(buff, 80, "%s\t%f", FormatTime(DateTime.getTime()),temperatureC);
    string topic = "Temperature/";
    topic += MY_NAME;
    mqttClient.publish(topic.c_str(), buff);
  }
}
//***************** End MQTT *********************************


//***************** Begin InfluxDB *********************************
// InfluxDB server url, e.g. http://192.168.1.48:8086 (don't use localhost, always server name or ip address)
#define INFLUXDB_URL "http://192.168.2.20:8086"
// InfluxDB database name
//#define INFLUXDB_DB_NAME "weather"
#define INFLUXDB_DB_NAME "mytest"
// The name of the measurement
#define INFLUXDB_TEMPERATUR_MEASUREMENT_NAME "historical_weather"
#define INFLUXDB_PUMP_SWITCH_MEASUREMENT_NAME "switch_status"
// Single InfluxDB instance
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_DB_NAME);

void setupInfluxDb(){
    // Set the timestamp with the precision of second
  client.setWriteOptions(WriteOptions().writePrecision(WritePrecision::S));
}

void WriteTemperature2Database(float temperature){
    if(mqttClient.isConnected()){
      // Define data point with a measurement
      Point pointDevice(INFLUXDB_TEMPERATUR_MEASUREMENT_NAME);
      // Set tags
      pointDevice.addTag("device_id", "aa46cde4-67a7-460a-afc5-a5541930555a");
      pointDevice.addTag("location", "Pumpenhaus");
      pointDevice.addTag("region", "Am Pothorn");
      pointDevice.addTag("sensor_type", "Temperature");
      pointDevice.addTag("valid", "T");
      // set the temperature
      pointDevice.addField("value", temperature);
      // Write data
      client.writePoint(pointDevice);
    }
}


void WritePumpSwitchStatus2Database(bool status){
    if(mqttClient.isConnected()){
      // Define data point with a measurement
      Point pointDevice(INFLUXDB_PUMP_SWITCH_MEASUREMENT_NAME);
      // Set tags
      pointDevice.addTag("device_id", "aa46cde4-67a7-460a-afc5-a5541930555a");
      pointDevice.addTag("location", "Pumpenhaus");
      pointDevice.addTag("region", "Am Pothorn");
      pointDevice.addTag("switch", "GroundwaterPump");
      pointDevice.addField("ConnectionEstablishedCount", mqttClient.getConnectionEstablishedCount());
      // set the switch status
      pointDevice.addField("value", (status == true?1:0));
      // Write data
      client.writePoint(pointDevice);
    }
}

void WriteConnectionEstablished2Database(bool status){
    if(mqttClient.isConnected()){
      // Define data point with a measurement
      Point pointDevice(INFLUXDB_PUMP_SWITCH_MEASUREMENT_NAME);
      // Set tags
      pointDevice.addTag("device_id", "aa46cde4-67a7-460a-afc5-a5541930555a");
      pointDevice.addTag("location", "Pumpenhaus");
      pointDevice.addTag("region", "Am Pothorn");
      pointDevice.addTag("switch", "GroundwaterPump");
      // set the switch status
      pointDevice.addField("ConnectionEstablishedCount", mqttClient.getConnectionEstablishedCount());
      // set the switch status
      pointDevice.addField("value", (status == true?1:0));
      // Write data
      client.writePoint(pointDevice);
    }
}
//***************** End InfluxDB *********************************

void setup() {
  // LED PINs as output
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  // Red LED on
  digitalWrite (LED_RED_PIN, HIGH);

// Note the format for setting a serial port is as follows: Serial.begin(baud-rate, protocol, RX pin, TX pin);
  Serial.begin(115200);

  logger.begin((char *)MY_NAME, &mqttClient);

  mqttClient.enableDebuggingMessages(MQTT_DEBUG); // Enable/disable debugging messages sent to serial output
  mqttClient.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overrited with enableHTTPWebUpdater("user", "password").

  // Start the DS18B20 sensor
  sensors.begin();

  setupInfluxDb();
  pinMode(relaisPin, OUTPUT);
  relaisStat = OFF;
  digitalWrite(relaisPin, relaisStat);
}

ulong _nextTempCheckTime;
#define CHECK_TEMP _nextTempCheckTime < millis()
#define SET_NEXT_TEMP_CHECK_TIME _nextTempCheckTime = millis() + _tempCheckPeriode

ulong _currentOnPeriode = _pumpOnPeriod5M;
ulong _currentOffPeriode = _pumpOffPeriode24H;

ulong _nextPumpSwitchTime;

#define REACH_NEXT_PUMP_SWITCH_TIME _nextPumpSwitchTime < millis()
#define SET_NEXT_PUMP_ON_TIME _nextPumpSwitchTime = millis() + _currentOffPeriode
#define SET_NEXT_PUMP_OFF_TIME _nextPumpSwitchTime = millis() + _currentOnPeriode

void loop() {
  #ifdef TEST_MODE    
    if(CHECK_TEMP_CHANGE){
      _currentTemp += _tempChangeIntervall;
      if(_currentTemp >= MAX_TEST_TEMP || _currentTemp <= MIN_TEST_TEMP){
        _tempChangeIntervall *= -1;
      }
      SET_NEXT_TEMP_CHANGE_TIME;
    }
  #endif

  if(CHECK_TEMP){
    // Read the temperature in ºC
    float temperatureC = getTemperature();

    logger.printMessage("Temperature: %2.1f ºC", temperatureC);
    // Publish to the MQTT
    PublishTemperatur(temperatureC);
    // Write into the InfluxDB
    WriteTemperature2Database(temperatureC);

    ulong nextOffPeriode = _currentOffPeriode;
    // Set the pump off periode
    if(temperatureC <= -10){
      if(_currentOffPeriode != _pumpOffPeriode15M) nextOffPeriode = _pumpOffPeriode15M;
    }else if(temperatureC <= -5){
      if(_currentOffPeriode != _pumpOffPeriode25M) nextOffPeriode = _pumpOffPeriode25M;
    }else if(temperatureC <= 3){
      if(_currentOffPeriode != _pumpOffPeriode55M) nextOffPeriode = _pumpOffPeriode55M;
    }else if(temperatureC > 3){
      if(_currentOffPeriode != _pumpOffPeriode24H) nextOffPeriode = _pumpOffPeriode24H;
    }

    if(_currentOffPeriode != nextOffPeriode){
      _currentOffPeriode = nextOffPeriode;
      if(relaisStat == OFF){
        SET_NEXT_PUMP_ON_TIME;
      }
    }

    // Check if the pump switch time is reached
    if(REACH_NEXT_PUMP_SWITCH_TIME){
      if(relaisStat == OFF){
        relaisStat = ON;
        SET_NEXT_PUMP_OFF_TIME;
      }else{
        relaisStat = OFF;
        SET_NEXT_PUMP_ON_TIME;
      }

      digitalWrite(relaisPin, relaisStat);

      PublishRelaisStatus(relaisStat);
      WritePumpSwitchStatus2Database(relaisStat);
    }

#ifdef TEST_MODE
      Serial.printf("Temp: %f\tPump Switch: %s\tCurrent Off Period: %i\tNext Pump Switch offset: %i in sec.\r\n", temperatureC, (relaisStat==OFF?"OFF":"ON"), _currentOffPeriode/1000, (_nextPumpSwitchTime -millis())/1000);
#endif      
    SET_NEXT_TEMP_CHECK_TIME;
  }

  if(mqttClient.isConnected() == false){
    if(mqttClient.isWifiConnected() == false){
      Serial.println("Wifi not connected...");
    }else if(mqttClient.isMqttConnected() == false){
      Serial.println("MQTT not connected...");
    }
  }

  mqttClient.loop();

  if(mqttClient.isConnected() == false){
    // LED red On and LED green off
    digitalWrite (LED_RED_PIN, HIGH);	
    digitalWrite (LED_GREEN_PIN, LOW);	
  }else{
    // LED red off and LED green on
    digitalWrite (LED_RED_PIN, LOW);	
    digitalWrite (LED_GREEN_PIN, HIGH);	
  }

  delay(1000);
}