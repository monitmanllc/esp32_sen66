// SEN66 Sensor - WiFi POST
// Install library: https://github.com/Sensirion/arduino-i2c-sen66

#include <Wire.h>
#include <SensirionI2cSen66.h>
#include <WiFi.h>
#include <HTTPClient.h>

// WiFi credentials
const char* ssid = "SomeWifiNetwork";
const char* password = "mysuperlongpassword";

// Server endpoint
const char* serverUrl = "https://app.monitman.com/dashboard/receive.php?sensor=air_quality";

SensirionI2cSen66 sen66;

// Error handling
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0
static char errorMessage[64];
static int16_t error;

// Timing for 1-minute intervals
unsigned long lastPostTime = 0;
const unsigned long postInterval = 60000; // 60 seconds in milliseconds

void printSerialNumber() {
    int8_t serialNumber[32];
    uint16_t serialNumberSize = 32;
    
    error = sen66.getSerialNumber(serialNumber, serialNumberSize);
    if (error != NO_ERROR) {
        Serial.print("Error getting serial number: ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    
    Serial.print("Serial Number: ");
    Serial.println((char*)serialNumber);
}

void connectWiFi() {
    Serial.println("\nConnecting to WiFi...");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi connection failed!");
    }
}

void postDataToServer(float temp, float hum, uint16_t co2, float vocIndex, 
                      float noxIndex, float pm1, float pm25, float pm4, float pm10) {
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected. Attempting to reconnect...");
        connectWiFi();
        return;
    }
    
    HTTPClient http;
    
    // Initialize HTTP connection
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    
    // Create JSON payload
    String jsonData = "{";
    jsonData += "\"temperature\":" + String(temp, 1) + ",";
    jsonData += "\"humidity\":" + String(hum, 1) + ",";
    jsonData += "\"co2\":" + String(co2) + ",";
    jsonData += "\"voc_index\":" + String(vocIndex, 0) + ",";
    jsonData += "\"nox_index\":" + String(noxIndex, 0) + ",";
    jsonData += "\"pm1_0\":" + String(pm1, 1) + ",";
    jsonData += "\"pm2_5\":" + String(pm25, 1) + ",";
    jsonData += "\"pm4_0\":" + String(pm4, 1) + ",";
    jsonData += "\"pm10_0\":" + String(pm10, 1);
    jsonData += "}";
    
    Serial.println("\nSending data to server...");
    Serial.println("Payload: " + jsonData);
    
    // Send POST request
    int httpResponseCode = http.POST(jsonData);
    
    if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String response = http.getString();
        Serial.println("Response: " + response);
    } else {
        Serial.print("Error sending POST request: ");
        Serial.println(httpResponseCode);
    }
    
    http.end();
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(100);
    }
    
    Serial.println("SEN66 Sensor with WiFi POST");
    Serial.println("===========================");
    
    // Connect to WiFi
    connectWiFi();
    
    // Initialize I2C
    Wire.begin();
    sen66.begin(Wire, SEN66_I2C_ADDR_6B);
    
    // Reset sensor
    error = sen66.deviceReset();
    if (error != NO_ERROR) {
        Serial.print("Error resetting device: ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    delay(1200);  // Wait for sensor to reset
    
    // Print serial number
    printSerialNumber();
    
    // Start continuous measurement
    error = sen66.startContinuousMeasurement();
    if (error != NO_ERROR) {
        Serial.print("Error starting measurement: ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    
    Serial.println("\nStarting measurements...\n");
}

void loop() {
    // Sensor values
    float massConcentrationPm1p0;
    float massConcentrationPm2p5;
    float massConcentrationPm4p0;
    float massConcentrationPm10p0;
    float humidity;
    float temperature;
    float vocIndex;
    float noxIndex;
    uint16_t co2;
    
    // Read all measurements
    error = sen66.readMeasuredValues(
        massConcentrationPm1p0, 
        massConcentrationPm2p5, 
        massConcentrationPm4p0,
        massConcentrationPm10p0, 
        humidity, 
        temperature, 
        vocIndex, 
        noxIndex,
        co2
    );
    
    if (error != NO_ERROR) {
        Serial.print("Error reading values: ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        delay(1000);
        return;
    }
    
    // Print all values in a readable format
    Serial.println("─────────────────────────────────────");
    
    Serial.print("Temperature:    ");
    Serial.print(temperature, 1);
    Serial.println(" °C");
    
    Serial.print("Humidity:       ");
    Serial.print(humidity, 1);
    Serial.println(" %");
    
    Serial.print("CO2:            ");
    Serial.print(co2);
    Serial.println(" ppm");
    
    Serial.print("VOC Index:      ");
    Serial.println(vocIndex, 0);
    
    Serial.print("NOx Index:      ");
    Serial.println(noxIndex, 0);
    
    Serial.print("PM1.0:          ");
    Serial.print(massConcentrationPm1p0, 1);
    Serial.println(" µg/m³");
    
    Serial.print("PM2.5:          ");
    Serial.print(massConcentrationPm2p5, 1);
    Serial.println(" µg/m³");
    
    Serial.print("PM4.0:          ");
    Serial.print(massConcentrationPm4p0, 1);
    Serial.println(" µg/m³");
    
    Serial.print("PM10:           ");
    Serial.print(massConcentrationPm10p0, 1);
    Serial.println(" µg/m³");
    
    Serial.println();
    
    // Check if it's time to post data (every 60 seconds)
    unsigned long currentTime = millis();
    if (currentTime - lastPostTime >= postInterval) {
        postDataToServer(temperature, humidity, co2, vocIndex, noxIndex,
                        massConcentrationPm1p0, massConcentrationPm2p5, 
                        massConcentrationPm4p0, massConcentrationPm10p0);
        lastPostTime = currentTime;
    }
    
    // Wait before next reading
    delay(1000);
}
