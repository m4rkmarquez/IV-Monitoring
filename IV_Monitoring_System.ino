//Wifi Module Library
#if defined(ESP32)
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
  #define DEVICE "ESP32"
  #elif defined(ESP8266)
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "ESP8266"
  #endif
  
  // WiFi AP SSID
  #define WIFI_SSID "Laptop"
  // WiFi password
  #define WIFI_PASSWORD "12345678"//"Dito-2024!1"

 //InfluxDB Library 
  #include <InfluxDbClient.h>
  #include <InfluxDbCloud.h>
  #include <InfluxDb.h>

  //Influxdb Connection Configuration
  //#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com" // Cloud Server Address
  //#define INFLUXDB_TOKEN "WtduCDsqZWVlAnIGnJCdpYa3XNwLts7T695OhR4-ex5WELlgyYJ9hcuKn2j586K2IZpapGINcG9yh_HW65xmtg==" // Server Token
  //#define INFLUXDB_ORG "3d4054da4fd13408"
  //#define INFLUXDB_BUCKET "IV Sensors"

  #define INFLUXDB_URL "http://legion740:8086"
  #define INFLUXDB_TOKEN "9VKpy0UfbYGt6jLd1Aak-WxYdviaIzSZz-Wm_QXpyCST5_hedLb6-ozYD3tB5ZqCcWkc9fmyZdIoxCWvCMUNxQ=="
  #define INFLUXDB_ORG "9cfb8df15597dda1"
  #define INFLUXDB_BUCKET "IV Sensors"

  
  // Time zone info
  #define TZ_INFO "PST-8"

  // Declare InfluxDB client instance with preconfigured InfluxCloud certificate
  InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
  
  // Declare Data point
  Point sensor("Sensors");

  /*pin addressing
    IC2 LCD Display: SCL: D1, SDA: D2
    DHT: D3
    HX711 Load Sensor - D4: DOUT, SCK: D5
    buttons: increase - D6, decrease: D7, D8: Store to ROM
  */
  #include <EEPROM.h>
  
  //DHT11
  #include "DHTesp.h"
  #define DHTpin D5    //D5 of NodeMCU is GPIO14
  DHTesp dht; //declare the library to be used
  
  //Load Sensor
  #include <Arduino.h>
  #include "HX711.h"

  // HX711 circuit wiring
  const int LOADCELL_DOUT_PIN = D6; // DT of HX711 to NodeMCU D6 pin
  const int LOADCELL_SCK_PIN = D7; // SCK of HX711 to NodeMCU D7 pin
  HX711 scale;

  //Liquid Crystal Library
  #include <Wire.h> 
  #include <LiquidCrystal_I2C.h>

  // set the LCD number of columns and rows
  int lcdColumns = 16;
  int lcdRows = 2;

  // set LCD address, number of columns and rows
  // if you don't know your display address, run an I2C scanner sketch
  LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  

  // buttons for IV configuration
  // Define pins for the buttons
  const int buttonDecrease = D5; // Pin for the "Decrease" button
  const int buttonIncrease = D6; // Pin for the "Increase" button
  const int buttonSave = D7;     // Pin for the "Save" button

  // Variables for button states and debounce
  int lastButtonDecreaseState = HIGH;
  int lastButtonIncreaseState = HIGH;
  int lastButtonSaveState = HIGH;
  unsigned long debounceDelay = 50;
  unsigned long lastDebounceTime = 0;

  // Variables for the number
  int storedNumber = 0;
  // Servo Library
  #include <Servo.h>
  Servo myservo;
  
  void setup() {
    // initialize LCD
    lcd.init();
    // turn on LCD backlight                      
    lcd.backlight();
    Serial.begin(115200); // Set baud rate to 115200
    lcd.setCursor(0,0);

    //Initialize the address assignment for sensors
    //DH11 Setup
    dht.setup(DHTpin, DHTesp::DHT11);
    //Servo motor as pinch valve
    myservo.attach(D4);

    
    //Load Sensor
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    // check if the sensor is ready
    if (scale.is_ready()) {
      Serial.println("HX711 is connected and ready.");
    } else {
      Serial.println("HX711 not found. Please check connections.");
      //while (1); // Halt execution if HX711 isn't ready
    }
  
    // Setup wifi
    WiFi.mode(WIFI_STA);
    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
    lcd.setCursor(0,0);
    lcd.print("Connecting to wifi");
    while (wifiMulti.run() != WL_CONNECTED) {
      lcd.setCursor(0,1);
      lcd.print(".");
      delay(100);
    }

    if (wifiMulti.run() == WL_CONNECTED) {
      lcd.print("wELCOME!");
      delay(1000);
      lcd.print("Initializing Sensors");
      lcd.println(); 

      IPAddress ip;
      ip = WiFi.localIP();
      lcd.setCursor(0,0);
      Serial.print("This is my ip: ");
      lcd.setCursor(0,1);
      Serial.println(ip);

      // Accurate time is necessary for certificate validation and writing in batches
      // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
      // Syncing progress and the time will be printed to Serial.
      timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

      // Check server connection
      if (client.validateConnection()) {
        lcd.print("Connected to InfluxDB: ");
        Serial.print("Connected to InfluxDB");
        lcd.println(client.getServerUrl());        
        //Add sensors to InfluxDB
        sensor.addTag("Devices","Sensors");
      } else {
        lcd.print("InfluxDB connection failed: ");
        lcd.println(client.getLastErrorMessage());
      }

    } else {
      lcd.setCursor(0,0);
      lcd.print("Connection Status");
      lcd.setCursor(0,1);
      Serial.print("Error!");
    }

    // buttons
    // Configure button pins as inputs with pull-up resistors
    pinMode(buttonDecrease, INPUT_PULLUP);
    pinMode(buttonIncrease, INPUT_PULLUP);
    pinMode(buttonSave, INPUT_PULLUP);

    // Start serial communication
    Serial.begin(9600);
    Serial.print("Hello World!");
    // Read the saved number from EEPROM
    storedNumber = EEPROM.read(0);
    Serial.print("Stored number loaded from EEPROM: ");
    Serial.println(storedNumber);
  }

void scrollMessage(int row, String message, int delayTime, int lcdColumns) {
  for (int i=0; i < lcdColumns; i++) {
    message = " " + message;  
  } 
  message = message + " "; 
  for (int position = 0; position < message.length(); position++) {
    lcd.setCursor(0, row);
    lcd.print(message.substring(position, position + lcdColumns));
    delay(delayTime);
  }
}

void buttonInteraction() {

}
void loop() {
  client.setInsecure();
  // Clear fields for reusing the point. Tags will remain the same as set above.
  sensor.clearFields();

  // Store measured value into point
  // Report RSSI of currently connected network
  //sensor.addField("rssi", WiFi.RSSI());
  //Code for DHT11 Sensor

  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor. Please check connections.");
    lcd.setCursor(0,0);
    lcd.print("Error:");
    lcd.setCursor(0,1);
    lcd.print("DHT Sensor");
  } else {
    lcd.print(dht.getStatusString());
    lcd.print("\t");
    lcd.print(humidity, 1);
    //send data to influxdb
    sensor.addField("Humidity", humidity);
    sensor.addField("Temperature", temperature);
  }
  delay(2000); // Wait 2 seconds before next check

  //Load Sensor HX711
  if (scale.is_ready()) {
    long weight = scale.get_units(10);  
    lcd.print("Result: ");
    lcd.println(weight);
    delay(1000);
    sensor.addField("HX711", weight);
  } else {
    Serial.println("HX711 disconnected!");
    delay(2000);
  }

  // Check WiFi connection and reconnect if needed
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
    lcd.println("Wifi connection lost");
  }

  // Write point
  if (!client.writePoint(sensor)) {
    Serial.println("InfluxDB write failed: ");
    lcd.println(client.getLastErrorMessage());
  }
  
  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(sensor.toLineProtocol());
  delay(1000);

  //Servo Motor
  int val;
  while (Serial.available() > 0) {
    val = Serial.parseInt();
    if (val != 0) {
      Serial.println(val);
      myservo.write(val);
    }
  }
  delay(5);

  // buttons
  int decreaseState = digitalRead(buttonDecrease);
  int increaseState = digitalRead(buttonIncrease);
  int saveState = digitalRead(buttonSave);

  // Handle the "Decrease" button
  if (decreaseState == LOW && lastButtonDecreaseState == HIGH && millis() - lastDebounceTime > debounceDelay) {
    storedNumber--;
    Serial.print("Number decreased: ");
    Serial.println(storedNumber);
    lastDebounceTime = millis();
  }

  // Handle the "Increase" button
  if (increaseState == LOW && lastButtonIncreaseState == HIGH && millis() - lastDebounceTime > debounceDelay) {
    storedNumber++;
    Serial.print("Number increased: ");
    Serial.println(storedNumber);
    lastDebounceTime = millis();
  }

  // Handle the "Save" button
  if (saveState == LOW && lastButtonSaveState == HIGH && millis() - lastDebounceTime > debounceDelay) {
    EEPROM.write(0, storedNumber);
    Serial.print("Number saved to EEPROM: ");
    Serial.println(storedNumber);
    lastDebounceTime = millis();
  }

  // Update button states
  lastButtonDecreaseState = decreaseState;
  lastButtonIncreaseState = increaseState;
  lastButtonSaveState = saveState;

  delay(50); // Short delay to reduce CPU usage
}

