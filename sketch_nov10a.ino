#include <WiFi.h>
#include <FirebaseESP32.h>
#include <PZEM004Tv30.h>

// --- 1. CONFIGURATION ---
#define WIFI_SSID "Gests"
#define WIFI_PASSWORD "sabeha@4141"

#define DATABASE_URL "https://smart-energy-meter-10025-default-rtdb.asia-southeast1.firebasedatabase.app"
#define DATABASE_SECRET "AIzaSyDPTxORuoVVwGiWI25BI4AOiTKIq0QI-44"

// --- 2. PINS ---
#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
// Onboard LED (Usually GPIO 2 on ESP32)
#define LED_PIN 2  

#if !defined(PZEM_RX_PIN) && !defined(PZEM_TX_PIN)
#define PZEM_SERIAL Serial2
#endif

PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN); //read voltage,current,power,frequency,power factor

FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;

unsigned long lastRead = 0;
const float overloadLimit = 3000.0; 

// LED Variables
unsigned long lastBlink = 0;
int ledState = LOW;

void setup() {
  Serial.begin(115200);
  
  // Init LED
  pinMode(LED_PIN, OUTPUT);

  // WIFI
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  
  // Blocking loop for initial connection (Fast Blink)
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); 
    delay(100); // Fast blink while connecting
    Serial.print(".");
  }
  Serial.println("\nConnected!");

  // FIREBASE Configuration 
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  config.timeout.serverResponse = 10 * 1000;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  
  // --- TASK 1: LED BLINK LOGIC (Non-Blocking) ---
  long blinkInterval = 1000; // Default: Slow Blink (1 second) if Connected

  // If WiFi is lost, Blink FAST (0.1 second)
  if (WiFi.status() != WL_CONNECTED) {
    blinkInterval = 100; 
  }

  if (millis() - lastBlink > blinkInterval) {
    lastBlink = millis();
    ledState = !ledState; // Toggle state
    digitalWrite(LED_PIN, ledState);
  }

  // --- TASK 2: SENSOR READING ---
  if (millis() - lastRead > 3000) {
    lastRead = millis();

    float voltage = pzem.voltage(); //Volt
    float current = pzem.current(); //Amp
    float power = pzem.power(); //Watt
    float energy = pzem.energy(); //KWh
    float frequency = pzem.frequency(); //hertz
    float pf = pzem.pf();

    if (isnan(voltage)) {
        Serial.println("Error reading PZEM!");
        Firebase.setString(firebaseData, "/energy/status", "Sensor Error");
    } else {
        Serial.print("Power: "); Serial.print(power); Serial.println("W");

        // Send Data
        Firebase.setFloat(firebaseData, "/energy/voltage", voltage);
        Firebase.setFloat(firebaseData, "/energy/current", current);
        Firebase.setFloat(firebaseData, "/energy/power", power);   
        Firebase.setFloat(firebaseData, "/energy/energy", energy);
        Firebase.setFloat(firebaseData, "/energy/frequency", frequency);
        Firebase.setFloat(firebaseData, "/energy/pf", pf);
        
        // Heartbeat
        Firebase.setTimestamp(firebaseData, "/energy/last_seen");

        // Overload
        if (power > overloadLimit) {
           Firebase.setBool(firebaseData, "/energy/overload", true);
        } else {
           Firebase.setBool(firebaseData, "/energy/overload", false);
        }
    }
  }
}