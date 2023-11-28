#include "DHTesp.h" // Click here to get the library: http://librarymanager/All#DHTesp

#ifdef ESP32
#pragma message(THIS EXAMPLE IS FOR ESP8266 ONLY!)
#error Select ESP8266 board.
#endif

const int dhtPin = 14;

DHTesp dht;

// Input output
const int ledPin = 2; //in D4
const int buttonPin[3] = {0, 13, 12}; //in d3 d6 d7

// variable for voltage sensor
const int vsenPin = A0;
float adcVoltage = 0.0;
float inVoltage = 0.0;
float R1 = 30000.0;
float R2 = 7320.0;
float refVoltage = 3.12;
int adcValue = 0;

float suhu, kelembapan;

// setup the input and output and call it in void setup
void inoutSetup() {
  // setup io
  dht.setup(dhtPin, DHTesp::DHT11);
  pinMode(ledPin, OUTPUT);
  pinMode(vsenPin, INPUT);
  for (int i = 0; i < 3; i++) pinMode(buttonPin[i], INPUT_PULLUP);

  for (int i = 0; i < 10; i++) digitalWrite(ledPin, !digitalRead(ledPin)), delay(50);
}

// get the voltage power input
float getVIN() {
  adcValue = analogRead(vsenPin);
  adcVoltage = (adcValue * refVoltage) / 1024;
  inVoltage = adcVoltage * (R1 + R2) / R2;
  return inVoltage;
}

// handle the button 0 1 2 with debounce millis (avoiding double press)
const int long debounceButton = 200;
unsigned long lastButtonPressTime = 0;
boolean button(int ch) {
  unsigned long currentMillis = millis();
  if (currentMillis - lastButtonPressTime >= debounceButton) {
    if (!digitalRead(buttonPin[ch])) {
      lastButtonPressTime = currentMillis;
      Serial.print("btn ");
      Serial.println(ch);
      lcd.clear();
      return true;
    }
  }
  return false;
}

// handle the limit of the value
void buttonHandler(int &value, int maxLimit, int minLimit, int step) {
  if (button(1)) value += step;
  if (button(2)) value -= step;
  if (value > maxLimit) value = minLimit;
  if (value < minLimit) value = maxLimit;
}

// backlight variable and function
unsigned long lastBacklightTime = 0;
const unsigned long backlightTimeout = 15000;
bool backlightCondition;
bool isBacklight = true;
void updateBacklight() {
  unsigned long currentTime = millis();
  // turn of the backlight
  if (currentTime - lastBacklightTime >= backlightTimeout && isBacklight) {
    Serial.println("backlight is off");
    lcd.noBacklight();
    backlightCondition = false;
    isBacklight = false;
  }
  //if backlight is on set the parameter if button pressed
  else if (isBacklight) {
    if (button(1)) Serial.println("setParam T"), setParam(tholdSuhu, "Temperature", 25, 40);
    if (button(2)) Serial.println("setParam H"), setParam(tholdKelembapan, "Humidity", 50, 80);
    if (button(0)) Serial.println("setParam I"), setParam(sendingInterval, "Interval", 1, 60);
  }
  // backlight is on when message coming or button is pressed
  // bool backlightCOndition is true when the new message is coming
  else if (backlightCondition || button(0) || button(1) || button(2)) {
    Serial.println("Backlight is on");
    lcd.backlight();
    isBacklight = true;
    lastBacklightTime = currentTime;
  } 
}

