#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include <Servo.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HX711.h>
#include <ESP8266HTTPClient.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <InfluxDb.h>

// WiFi credentials
const char* ssid = "Laptop"; // Replace with  WiFi SSID
const char* password = "12345678"; // Replace with  WiFi password

// Web server
ESP8266WebServer server(80);

// InfluxDB settings
#define INFLUXDB_URL "http://192.168.137.1:8086" //  InfluxDB Cloud URL
#define  INFLUXDB_TOKEN "9VKpy0UfbYGt6jLd1Aak-WxYdviaIzSZz-Wm_QXpyCST5_hedLb6-ozYD3tB5ZqCcWkc9fmyZdIoxCWvCMUNxQ==" // Replace with  InfluxDB token
#define  INFLUXDB_ORG "9cfb8df15597dda1" // Replace with  organization name
#define  INFLUXDB_BUCKET "IV Monitoring" // Replace with  bucket name

// InfluxDB client
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Point for writing data
Point sensorData("iv_monitor");

// Time zone info
  #define TZ_INFO "PST-8"

// Load cell
const int LOADCELL_DOUT_PIN = D7; // Connect HX711 data pin to D7
const int LOADCELL_SCK_PIN = D8;  // Connect HX711 clock pin to D8
//HX711_ADC scale(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
// HX711 instance
HX711 scale;

// DHT Sensor
#define DHTPIN D4 // Connect DHT22 Data pin to D4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Servo Motor
Servo servo;
int servoPin = D6; // Connect servo signal pin to D6

// I2C LED Display
LiquidCrystal_I2C lcd(0x27, 16, 2); // Update the address if necessary SDA: D2 SCL: D1

// EEPROM memory locations
#define EEPROM_VOLUME_ADDR 0 // Address to store total volume
#define EEPROM_TIME_ADDR 4   // Address to store total time
#define EEPROM_SERVO_ADDR 8  // Address to store last servo position

// Variables
float volume = 1000; // Total volume in mL
float totalTime = 8; // Total time in hours
float flowRate; // Flow rate in mL/hr
int lastServoPosition;
float weight = 0; // Measured weight from load cell
int dropCount = 0; // Initialize drop count
float previousWeight = 0.0;
unsigned long previousTime = 0;
float dropRate = 0.0; // in grams per second

// Functions
// void readLoadCell() {
//   scale.update(); // Update the HX711_ADC readings
//   weight = scale.getData(); // Get the current weight
// }

void sendToInfluxDB(float temperature, float humidity, float dropRate) {
  client.setInsecure();
  // Add fields
  sensorData.addField("temperature", temperature);
  sensorData.addField("humidity", humidity);
  //sensorData.addField("drop_count", dropCount);
  sensorData.addField("drop_rate", dropRate);

  // Write data to InfluxDB
  if (!client.writePoint(sensorData)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  } else {
    Serial.println("Data successfully written to InfluxDB.");
  }
}

void calculateFlowRate(float weight) {
  Serial.print(weight);
  flowRate = weight / totalTime; // Formula: mL/hr
  int angle = map(flowRate, 0, 500, 0, 180); // Map flow rate to servo angle
  servo.write(angle); // Move servo motor
  lastServoPosition = angle;

  // Save last servo position to EEPROM
  EEPROM.put(EEPROM_SERVO_ADDR, lastServoPosition);
  EEPROM.commit();
}

void displayOnLCD(float temperature, float humidity, int dropCount, String ipAddress) {
  lcd.clear();
  lcd.setCursor(0, 0);
  scrollWiFiStatus("IP:" + ipAddress);
  lcd.setCursor(0, 1);
  lcd.print("Temp:" + String(temperature) + "C");
  lcd.setCursor(7, 1);
  lcd.print("Hum:" + String(humidity) + "%");
  lcd.setCursor(0, 2);
  lcd.print("Drops: " + String(dropCount));
}

void scrollWiFiStatus(String message) {
  for (int i = 0; i < message.length(); i++) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(message.substring(i));
    delay(200); // Allow time for background tasks
    yield();    // Prevent WDT reset during scrolling
  }
}
// Add a new variable to store the servo angle
int servoAngle = 90; // Default servo angle (90 degrees)

// Function to handle servo control via the web app
void handleServoControl() {
  if (server.hasArg("angle")) {
    servoAngle = server.arg("angle").toInt(); // Get the desired angle from the web app
    servoAngle = constrain(servoAngle, 0, 180); // Ensure the angle is within valid range
    servo.write(servoAngle); // Move the servo to the specified angle

    // Send a response back to the web app
    String html = "<html><body>";
    html += "<h1>Servo Control</h1>";
    html += "<p>Servo moved to angle: " + String(servoAngle) + " degrees</p>";
    html += "<a href='/'>Go Back</a>";
    html += "</body></html>";
    server.send(200, "text/html", html);

    // Log the action to the Serial Monitor
    Serial.print("Servo moved to angle: ");
    Serial.println(servoAngle);
  } else {
    // If no angle is provided, show an error message
    String html = "<html><body>";
    html += "<h1>Servo Control</h1>";
    html += "<p style='color:red;'>Error: No angle provided!</p>";
    html += "<a href='/'>Go Back</a>";
    html += "</body></html>";
    server.send(400, "text/html", html);
  }
}

// Modify the web app to include a servo control form
void handleWebRequests() {
  String html = "<html><body>"
                "<h1>IV Monitor Control</h1>"
                "<form method='GET'>"
                "Total Volume (mL): <input name='volume' value='" + String(volume) + "'><br>"
                "Total Time (hrs): <input name='totalTime' value='" + String(totalTime) + "'><br>"
                "<input type='submit' value='Update'>"
                "</form>"
                "<p>Flow Rate: " + String(flowRate) + " mL/hr</p>"
                "<h2>Servo Control</h2>"
                "<input type='range' id='servoSlider' min='0' max='180' value='" + String(lastServoPosition) + "' oninput='updateServo(this.value)'>"
                "<span id='servoValue'>" + String(lastServoPosition) + "</span>°"
                "<p id='notification'></p>"
                "<script>"
                "function updateServo(angle) {"
                "  document.getElementById('servoValue').innerText = angle;"
                "  document.getElementById('notification').innerText = 'Updating servo motor...';"
                "  document.getElementById('servoSlider').disabled = true;"
                "  alert('Servo motor is being updated. Please wait.');" // Add pop-up message
                "  fetch('/updateServo?angle=' + angle)"
                "    .then(response => {"
                "      if (response.ok) {"
                "        document.getElementById('notification').innerText = 'Servo angle updated to ' + angle + ' degrees';"
                "      } else {"
                "        throw new Error('Network response was not ok');"
                "      }"
                "    })"
                "    .catch(error => {"
                "      console.error('There was a problem with the fetch operation:', error);"
                "      document.getElementById('notification').innerText = 'Failed to update servo angle';"
                "    })"
                "    .finally(() => {"
                "      document.getElementById('servoSlider').disabled = false;" // Re-enable the slider
                "    });"
                "}"
                "</script>"
                "</body></html>";

  server.send(200, "text/html", html);
}

void handleServoUpdate() {
  if (server.hasArg("angle")) {
    int servoAngle = server.arg("angle").toInt();
    servoAngle = constrain(servoAngle, 0, 180); // Ensure the angle is within valid range
    servo.write(servoAngle); // Move the servo to the specified angle
    lastServoPosition = servoAngle;

    // Save last servo position to EEPROM
    EEPROM.put(EEPROM_SERVO_ADDR, lastServoPosition);
    EEPROM.commit();

    Serial.print("Servo moved to angle: ");
    Serial.println(servoAngle);
  }
  server.send(200, "text/plain", "OK"); // Send a proper response to avoid timeout
}

void setup() {
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  // Serial Communication
  Serial.begin(115200);
  Serial.println("Starting IV Monitor...");

  // EEPROM Initialization
  EEPROM.begin(512);
  EEPROM.get(EEPROM_VOLUME_ADDR, volume);
  EEPROM.get(EEPROM_TIME_ADDR, totalTime);
  EEPROM.get(EEPROM_SERVO_ADDR, lastServoPosition);

  // WiFi Connection
  lcd.init(); // Initialize the LCD with 16 columns and 2 rows
  lcd.backlight();
  scrollWiFiStatus("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); // Allow the system to handle background tasks
    yield();    // Yield control to prevent WDT reset
  }
  scrollWiFiStatus("WiFi Connected");
  String ipAddress = WiFi.localIP().toString();
  delay(500);

  // Web Server Setup
  server.on("/", handleWebRequests);
  server.on("/servo", handleServoControl); // Add route for servo control
  server.on("/updateServo", handleServoUpdate); // Add route for real-time servo updates
  server.begin();

  // DHT Sensor Initialization
  dht.begin();

  // Servo Motor Initialization
  servo.attach(servoPin);
  servo.write(lastServoPosition);

  // HX711_ADC Load Cell Initialization
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(2280.f); // Allow time for HX711 to stabilize
  // scale.setCalFactor(1.2); // Set calibration factor; adjust as needed
  scale.tare(); // Reset the scale to 0
  // Check if the load cell is ready
  if (!scale.is_ready()) {
    // Display error on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Load Cell Error!");
    lcd.setCursor(0, 1);
    lcd.print("Check Wiring");

    // Log error to Serial Monitor
    Serial.println("Error: Load cell not ready. Check wiring and connections.");
  }

  calculateFlowRate(scale.get_units(10));

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi IP:" + ipAddress);
  Serial.print("WiFi IP: " + ipAddress);

  // Set tags
  sensorData.addTag("location", "ward");
}

void loop() {
    // Clear fields for reusing the point. Tags will remain the same as set above.
  sensorData.clearFields();
  server.handleClient();
  yield(); // Yield control to prevent WDT reset

  // Read sensor values
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  //readLoadCell(); // Read load cell weight
  String ipAddress = WiFi.localIP().toString();
  yield(); // Yield control after sensor readings

  // Check if sensors are available
  if (isnan(temperature) || isnan(humidity)) {
    // Display error on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error!");
    lcd.setCursor(0, 1);
    lcd.print("Check DHT Sensor");

    // Log error to Serial Monitor
    Serial.println("========== IV Monitor Data ==========");
    Serial.println("Error: DHT sensor not available!");
    Serial.println("=====================================");
    delay(2000); // Wait before retrying
    return; // Skip the rest of the loop
  }

  if (!scale.is_ready()) {
    // Display error on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error!");
    lcd.setCursor(0, 1);
    lcd.print("Check Load Cell");

    // Log error to Serial Monitor
    Serial.println("========== IV Monitor Data ==========");
    Serial.println("Error: Load cell not available!");
    Serial.println("=====================================");
    delay(2000); // Wait before retrying
    //return; // Skip the rest of the loop
  } else {
     weight = scale.get_units(10); // Average of 10 readings

        unsigned long currentTime = millis();
        if (currentTime - previousTime >= 1000) { // Calculate drop rate every second
            dropRate = (previousWeight - weight) / ((currentTime - previousTime) / 1000.0);
            previousWeight = weight;
            previousTime = currentTime;

            Serial.print("Current Weight: ");
            Serial.print(weight, 2);
            Serial.print(" g, Drop Rate: ");
            Serial.print(dropRate, 2);
            Serial.println(" g/s");
        }
  }

  // Send data to InfluxDB
  sendToInfluxDB(temperature, humidity, dropRate);

  // Display data on LCD
  displayOnLCD(temperature, humidity, dropRate, ipAddress);
}