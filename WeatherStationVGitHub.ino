#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ThingSpeak.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Adafruit_BMP085_U.h>

//Pins & Settings
#define DHTPIN D4          
#define DHTTYPE DHT11      
DHT dht(DHTPIN, DHTTYPE);
#define LDRPIN A0          
#define LEDPIN D5          
Adafruit_BMP085_Unified bmp;
WiFiClient client;                //ThingSpeak
WiFiClientSecure clientSecure;    //GoogleForms
HTTPClient http;
int readingTime = 60000; //Time between readings. 60000 is 1 minute

//WiFi
unsigned long startAttemptTime = millis();
const unsigned long wifiTimeout = 10000;  //10 seconds

//IDs, Passwords etc.
const char *ssid = "SSID";              //Router
const char *password = "password";            //Router Password
const unsigned long thingID = 0000000;          //ThingSpeak4 Channel ID
const char *thingKey = "ABCDEFGHIJKLMNOP";      //ThingSpeak Write API Key

//Component Flags
bool dhtConnected = false;
bool bmpConnected = false;
bool ledConnected = false;
bool ldrConnected = false;

void setup() {
  Serial.begin(115200);
  Serial.println();
  
  //Set green LED to off
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, LOW);  

  //Check and initialize sensors, DHT, BMP, LDR, LED
  dht.begin();
  float testDHT = dht.readHumidity();
  if (!isnan(testDHT)) {
    dhtConnected = true;
    Serial.println("DHT detected.");
  } else {
    Serial.println("DHT not detected.");
  }

  if (bmp.begin()) {
    bmpConnected = true;
    Serial.println("BMP180 detected.");
  } else {
    Serial.println("BMP180 not detected.");
  }

  int ldrReading = analogRead(LDRPIN);
  if (ldrReading > 0 && ldrReading < 1023) { 
    ldrConnected = true;
    Serial.println("LDR detected.");
  } else {
    Serial.println("LDR not detected.");
  }
  
  //LED is not crucial for the weather station to operate, but is used as an indicator the device is uploading data
  digitalWrite(LEDPIN, HIGH);
  delay(100);
  if (digitalRead(LEDPIN) == HIGH) {
    ledConnected = true;
    Serial.println("G LED detected.");
  } else {
    Serial.println("G LED not detected.");
  }

  Serial.println();

  //Attempt to connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiTimeout) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n---\nConnected to WiFi.\n---");
  } else {
    Serial.println("\nWiFi connection timed out.\n WARNING PROCEEDING WITHOUT CONNECTIVITY.");
  }

  //Initialize ThingSpeak
  ThingSpeak.begin(client);
}

void loop() {
  //Set all values to below zero, after checking value, print to monitor for debugging purposes
  float temperature = -1;
  float humidity = -1;
  float pressure = -1;
  int light = -1;

  //Read DHT11
  if (dhtConnected) {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Warning: Failed to read from DHT sensor!");
    } else {
      Serial.print("DHT Humidity: ");
      Serial.print(humidity);
      Serial.println("%");
      Serial.print("DHT Temperature: ");
      Serial.print(temperature);
      Serial.println("°C");
    }
  }
  
  //Read BMP180
  if (bmpConnected) {
    sensors_event_t event;
    bmp.getEvent(&event);
    if (event.pressure) {
      pressure = event.pressure - 150;
      Serial.print("BMP Pressure: ");
      Serial.print(pressure);
      Serial.println(" hPa");
    } else {
      Serial.println("Warning: Failed to read from BMP sensor!");
    }
  }
  
  //Read LDR
  if (ldrConnected) {
    light = analogRead(LDRPIN);
    if (light == 0 || light == 1023) {
      Serial.println("Warning: LDR reading out of range. Check connection.");
      ldrConnected = false;
    } else {
      Serial.print("LDR Light Level: ");
      Serial.print(light);
      Serial.println(" lx");
    }
  }
  //Send data to ThingSpeak and GoogleSheets. LED remains lit until complete
  digitalWrite(LEDPIN, HIGH);  
  sendToThingSpeak(temperature, humidity, pressure, light);
  sendToGoogleSheets(temperature, humidity, pressure, light);
  digitalWrite(LEDPIN, LOW);
  Serial.println("---");
  delay(readingTime); //Send data every minute
}

void sendToThingSpeak(float temp, float hum, float press, int light) {
  ThingSpeak.setField(1, temp);
  ThingSpeak.setField(2, hum);
  ThingSpeak.setField(3, press);
  ThingSpeak.setField(4, light);
  
  int response = ThingSpeak.writeFields(thingID, thingKey);
  if (response == 200) {
    Serial.println("Data sent successfully to ThingSpeak.");
  } else {
    Serial.println("Failed to send data to ThingSpeak.");
  }
}

void sendToGoogleSheets(float temp, float hum, float press, int light) {
  //Construct the URL for Google Form submission
  String googleFormUrl = "https://docs.google.com/forms/d/e/1FAIpQLSeI3KZ0xKGpWWUF9plK1RHaKdUgrZHeN7KNaT9i-dx9GB2q4g/formResponse?";

  //Add to URL, with readings passed into function
  googleFormUrl += "entry.1830958942=" + String(temp) + "%C2%B0C";
  googleFormUrl += "&entry.842067926=" + String(hum) + "%25";
  googleFormUrl += "&entry.446719619=" + String(press) + "%20hPa";
  googleFormUrl += "&entry.611630028=" + String(light) + "%20lx";
  
  //Send GET request
  if (http.begin(clientSecure, googleFormUrl)) {  
    clientSecure.setInsecure();  
    
    int httpResponseCode = http.GET();  
    
    if (httpResponseCode > 0) {
      Serial.println("Data sent successfully to Google Sheets.");
    } else {
      Serial.println("Failed to send data to Google Sheets.");
    }
    http.end();  
  } else {
    Serial.println("Unable to connect to Google Forms.");
  }
}


