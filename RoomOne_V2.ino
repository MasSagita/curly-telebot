/*
   TELEGRAM BOT Notification and FIREBASE Database

   Using WEMOS D1 Mini ESP8266 and send the DHT11 sensor value to telegram bot and firebase database

   2023
*/


#include <EEPROM.h>

//https://github.com/cotestatnt/AsyncTelegram2
#include <AsyncTelegram2.h>

// Timezone definition
#include <time.h>
#define MYTZ "WIB-7"
char formattedTime[64];

// esp8266
#include <ESP8266WiFi.h>
BearSSL::WiFiClientSecure client;
BearSSL::Session   session;
BearSSL::X509List  certificate(telegram_cert);

AsyncTelegram2 myBot(client);
const char* ssid  =  "Legiun";
const char* pass  =  "sembada33";
const char* token =  "6944744024:AAEMtwNXJ4vI_SvoofBfvPSMuawOm2xRgGc";
int64_t userid = 6006420445;

const char TEXT_HELP[] =
  "Available Commands:\n"
  "/showsetting - Show Device Setting\n"
  "/info  - Show device info.\n"
  "/dht11 - Show reading value from sensor DHT11.\n"
  "/power - Show power input"
  "/help  - Show available commands";

#include <FirebaseESP8266.h>
#define FIREBASE_HOST "farhan-skripsi-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "rhIMDRqbBMWSuYUwUx684VXyyjQvziYebn4iHWw1"
FirebaseData firebaseData;

#include <LiquidCrystal_I2C.h>  //lcd i2c
LiquidCrystal_I2C lcd(0x27, 16, 2); //lcd di alamat 0x27

// setup variable
int tholdSuhu = 0;
int tholdKelembapan = 0;
int sendingInterval = 0;

void setParam(int setting, const char *paramName, int dLimit, int uLimit);

#include "inout.h"

void inoutSetup();
//static bool readDHT(float *temp, float *hum);
float getVIN();
boolean button(int ch);
void buttonHandler(int &value, int maxLimit, int minLimit, int step);
void updateBacklight(); // setting parameter ada di dalam fungsi ini

//sending variabel alert
bool unplugAdaptor = false;
bool plugAdaptor = false;

// sending based on interval
unsigned long prevSendToFirebase = 0;
unsigned long prevSendingMillis = 0;
bool isSendingNotif = false;

//variable status suhu
String kondisiTemp = " ";
String kondisiHumi = " ";
String kondisiPower = " ";
bool isSendingTemp = false, isSendingHumi = false, isSendingPower = false;

float prevTemp;
float prevHumi;

//change according what the device is
const char thisDevice[] = "DEVICE3_ROOM3 ";
const char thisR[] = "R3";
const char thisD[] = "D3";
const int thisRoomNum = 3;

// notifikasi batere
String batteryStatus = "";

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("\n");
  String thisBoard = ARDUINO_BOARD;
  Serial.println(thisBoard);

  Serial.println(thisDevice);

  // eeprom and see what happen inside the memory
  EEPROM.begin(512);
  tholdSuhu = EEPROM.read(0);
  tholdKelembapan = EEPROM.read(1);
  sendingInterval = EEPROM.read(2);

  Serial.print(tholdSuhu), Serial.print("\t");
  Serial.print(tholdKelembapan), Serial.print("\t");
  Serial.print(sendingInterval), Serial.print("\n\n");

  lcd.init();

  inoutSetup();

  lcd.backlight();

  lcd.setCursor(2, 0), lcd.print(thisDevice);
  delay(1500), lcd.clear();

  //wifi configuration
  WiFi.setAutoConnect(true);
  WiFi.mode(WIFI_STA);

  // connects to the access point and check the connection
  WiFi.begin(ssid, pass);
  delay(250);

  Serial.print("WiFi Connecting");
  lcd.setCursor(0, 0), lcd.print(F("connect to:"));
  lcd.setCursor(0, 1), lcd.print(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    digitalWrite(ledPin, !digitalRead(ledPin));
    delay(50);
  }
  Serial.print("\nWiFi connect to: "), Serial.println(ssid);
  lcd.clear();
  lcd.setCursor(0, 0), lcd.print(F("WiFi AP OK"));
  delay(1000), lcd.clear();

  Serial.print("Firebase Database Connecting");
  lcd.setCursor(0, 0), lcd.print(F("Firebase DB"));
  lcd.setCursor(0, 1), lcd.print(F("Connecting..."));
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  while (!Firebase.ready()) {
    Serial.print(".");
    digitalWrite(ledPin, !digitalRead(ledPin));
    delay(100);
  }
  Serial.print("\nFirebase Host: "), Serial.println(FIREBASE_HOST);
  lcd.clear();
  lcd.setCursor(0, 0), lcd.print(F("Firebase OK"));
  delay(1000), lcd.clear();

  // Sync time with NTP, to check properly Telegram certificate
  configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");

  //Set certficate, session and some other base client properies
  client.setSession(&session);
  client.setTrustAnchors(&certificate);
  client.setBufferSizes(1024, 1024);

  // Set the Telegram bot properties
  myBot.setUpdateTime(1000);
  myBot.setTelegramToken(token);

  // Check if all things are ok
  Serial.print("TeleBot Connecting");
  lcd.setCursor(0, 0), lcd.print(F("Telegram Bot"));
  lcd.setCursor(0, 1), lcd.print(F("Connecting..."));
  //myBot.begin() ? Serial.println("OK") : Serial.println("NOK");
  while (!myBot.begin()) {
    Serial.print(".");
    digitalWrite(ledPin, !digitalRead(ledPin));
    delay(50);
  }
  Serial.print("\nConnected to bot: @: "), Serial.println(myBot.getBotName());
  lcd.clear();
  lcd.setCursor(0, 0), lcd.print(F("Telegram Bot OK"));
  lcd.setCursor(0, 1), lcd.print(F("@")), lcd.print(myBot.getBotName());
  delay(1000), lcd.clear();

  time_t now = time(nullptr);
  struct tm t = *localtime(&now);
  char welcomeMsg[64];
  strftime(welcomeMsg, sizeof(welcomeMsg), " Started at %X", &t);
  char reply[64];
  strcpy(reply, thisDevice);
  strcat(reply, welcomeMsg);

  myBot.sendTo(userid, reply);

  Serial.println(reply);
  delay (1500), lcd.clear();

  myBot.sendTo(userid, TEXT_HELP);
  delay (1500);
}

bool getValueDHT11 = false;
float currentTemp;
float currentHumi;

void loop() {
  // put your main code here, to run repeatedly:
  //digitalWrite(ledPin, !digitalRead(ledPin));

  formatTime();

  // blink the led for 50ms and display the time
  static uint32_t displayTF = millis();
  if (millis() - displayTF < 50) digitalWrite(ledPin, HIGH);
  if (millis() - displayTF > 50) digitalWrite(ledPin, LOW);
  if (millis() - displayTF > 1000) {
    currentTemp = dht.getTemperature();
    currentHumi = dht.getHumidity();
    lcd.setCursor(4, 0), lcd.print(formattedTime);
    Serial.print(currentTemp), Serial.print("°C \t");
    Serial.print(currentHumi), Serial.println("%");
    displayTF = millis();
  }

  prevTemp  = currentTemp;
  prevHumi  = currentHumi;

  //display the device and room
  lcd.setCursor(0, 0), lcd.print(thisD);
  lcd.setCursor(14, 0), lcd.print(thisR);

  //  //display the dht11 and the voltage input
  lcd.setCursor(0, 1), lcd.print(prevTemp, 1);
  lcd.print((char)223), lcd.print(F("C "));
  lcd.print(prevHumi, 0), lcd.print(F("% "));
  lcd.print(getVIN(), 2), lcd.print(F("V"));

  // Check incoming messages and keep Telegram server connection alive
  TBMessage msg;

  // ruangan > dari threshold suhu
  if (prevTemp > tholdSuhu && !isSendingTemp && prevTemp != 0) {
    kondisiTemp = "Too HOT!\n";
    String reply;
    reply += thisDevice;
    reply += kondisiTemp;
    myBot.sendTo(userid, reply);
    Serial.println(reply);
    isSendingTemp = true;
  }
  if (prevTemp < tholdSuhu && prevTemp != 0) {
    kondisiTemp = "Suhu Normal\n";
    isSendingTemp = false;
  }

  // ruangan terlalu kering
  if (prevHumi < tholdKelembapan && !isSendingTemp && prevHumi != 0) {
    kondisiHumi = "Too Dry!\n";
    String reply;
    reply += thisDevice;
    reply += kondisiTemp;
    myBot.sendTo(userid, reply);
    Serial.println(kondisiHumi);
    isSendingHumi = false;
  }
  if (prevHumi > tholdKelembapan && prevHumi != 0) {
    kondisiHumi = "Kelembapan Normal\n";
    isSendingHumi = false;
  }

  // sending power alert
  sendAlert();
  
  // send to firebase only every 5 seconds
  if (millis() - prevSendToFirebase >= 5 * 1000) {
    int randomNum = random(0, 100);
    Serial.println("Send to Firebase DB Onyl!");
    bool suhuSukses = Firebase.setFloat(firebaseData, "/Suhu" + String(thisRoomNum), prevTemp);
    bool kelembapanSukses = Firebase.setInt(firebaseData, "/Kelembapan" + String(thisRoomNum), prevHumi);
    bool teganganSukses = Firebase.setFloat(firebaseData, "/Tegangan" + String(thisRoomNum), getVIN());
    bool randomNumber = Firebase.setInt(firebaseData, "/Number" , randomNum);
    
    if (!suhuSukses || !kelembapanSukses || !teganganSukses || !randomNumber) Serial.println(F("FBD Error!"));
    else {
      Serial.print(randomNum);
      Serial.print(" | ");
      Serial.println("Update FirebaseDB");
    }
    digitalWrite(ledPin, HIGH);
    delay(150);
    digitalWrite(ledPin, LOW);
    prevSendToFirebase = millis();
  }

  // interval seconds = value * 1000
  // interval minute = value * 60 * 1000
  
  if (millis() - prevSendingMillis >= sendingInterval * 60000 && !isSendingNotif) {
    Serial.println("\nsend to bot:");
    String reply;
    reply += thisDevice;
    reply += "\n";
    if (prevTemp > tholdSuhu) reply += kondisiTemp;
    if (prevHumi < tholdKelembapan) reply += kondisiHumi;
    reply += "DHT11: ";
    reply += (float) prevTemp , 2;
    reply += "°C | ";
    reply += (int) prevHumi;
    reply += "% ";
    reply += "\nVin: ";
    reply += (float) getVIN();
    reply += "\nSending at ";
    reply += formattedTime;
    Serial.println(reply);
    digitalWrite(ledPin, HIGH);
    delay(25);
    sendFirebase();
    delay(25);
    digitalWrite(ledPin, LOW);
    myBot.sendTo(userid, reply);
    isSendingNotif = true;
    prevSendingMillis = millis();
  }
  if (millis() - prevSendingMillis < sendingInterval * 60000) isSendingNotif = false;

  // check if new message is coming from user
  if (myBot.getNewMessage(msg)) {
    backlightCondition = true;
    Serial.print(msg.sender.username);
    Serial.print(" send this: ");
    Serial.println(msg.text);

    String msgText = msg.text;

    if (msgText.equals("/help")) myBot.sendMessage(msg, TEXT_HELP);

    else if (msgText.equals("/dht11")) {
      String reply;
      reply += thisDevice;
      reply += " DHT11: ";
      reply += (float) prevTemp , 2;
      reply += "°C | ";
      reply += (int) prevHumi;
      reply += "%";
      myBot.sendMessage(msg, reply);
    }

    else if (msgText.equals("/info")) {
      String reply;
      reply += thisDevice;
      reply += "\n";
      reply += kondisiTemp;
      reply += kondisiHumi;
      reply += (float) getVIN();
      reply += "V ";
      reply += kondisiPower;
      myBot.sendMessage(msg, reply);
    }

    else if (msgText.equals("/showsetting")) {
      String reply;
      reply += thisDevice;
      reply += "is:/n";

      reply += "- will send notification when temperature is higher than ";
      reply += tholdSuhu;
      reply += "°C/n";

      reply += "- will send notification when humidity is bellow than ";
      reply += tholdKelembapan;
      reply += "%/n";

      reply += "- will send notification in every ";
      reply += sendingInterval;
      reply += " Minutes/n";

      myBot.sendMessage(msg, reply);
    }

    else if (msgText.equals("/power")) {
      String reply;
      reply += thisDevice;
      reply += "is ";
      reply += kondisiPower;
      reply += (float) getVIN();
      myBot.sendMessage(msg, reply);
    }

    else {
      String reply = msg.text;
      reply += " not found!\n";
      reply += "Try /help or /dht11 or /info";
      myBot.sendMessage(msg, reply);
    }
  }

  // handle the backlight and parameter settings
  updateBacklight(); // and set parameter
  //  delay(50);
}

// function to send the variable in to firebase database call this based on interval sending
void sendFirebase() {

  String dev = "1";
  time_t now = time(nullptr);
  struct tm *ptm = localtime (&now);

  int jam   = ptm->tm_hour;
  int hari  = ptm-> tm_mday;
  int bulan = ptm->tm_mon + 1;
  int tahun = ptm->tm_year + 1900;

  bool suhuSukses = Firebase.setFloat(firebaseData, "/" + String(thisDevice) + "/suhu", prevTemp);
  bool kelembapanSukses = Firebase.setInt(firebaseData, "/" + String(thisDevice) + "/kelembapan", prevHumi);
  bool teganganSukses = Firebase.setFloat(firebaseData, "/" + String(thisDevice) + "/tegangan", getVIN());

  String dmyJam = String(hari) + "-" + String(bulan) + "-" + String(tahun) + "/" + String(jam);

  // kirim dan pisah sensor dht11 dan sensor tegangan dengan jam tanggal bulan dan tahun
  String dmySuhu = "/" + String(thisDevice) + "/Suhu/" + dmyJam;
  Firebase.setFloat(firebaseData, dmySuhu, prevTemp);

  String dmyKelembapan = "/" + String(thisDevice) + "/Kelembapan/" + dmyJam;
  Firebase.setFloat(firebaseData, dmyKelembapan, prevHumi);

  String dmyTegangan = "/" + String(thisDevice) + "/Tegangan/" + dmyJam;
  Firebase.setFloat(firebaseData, dmyTegangan, getVIN());

//  // Kirim data langsung diluar
//  bool suhuToFBD = Firebase.setFloat(firebaseData, "/" + "suhu_" + String(dev), prevTemp);
//  bool kelembapanToFBD = Firebase.setInt(firebaseData, "/" + "kelembapan_" + String(dev), prevHumi);
//  bool teganganToFBD = Firebase.setFloat(firebaseData, "/" + "tegangan_" + String(dev), getVIN());
//  
//  if (!suhuToFBD || !kelembapanToFBD || !teganganToFBD) Serial.println(F("Send Error!"));
  if (!suhuSukses || !kelembapanSukses || !teganganSukses) Serial.println(F("FBD Error!"));
  else Serial.println("Update FirebaseDB");
}

// send the alert when the power is plugged or unplugged
void sendAlert() {
  // adaptor is plugged
  if (getVIN() > 4.0 && !plugAdaptor) {
    backlightCondition = true;
    kondisiPower = "powered by Adaptor\n";
    batteryStatus = "Adaptor";
    String reply;
    reply += thisDevice;
    reply += kondisiPower;
    reply += "Vin: ";
    reply += (float) getVIN();
    reply += " at ";
    reply += formattedTime;
    Serial.println(reply);
    myBot.sendTo(userid, reply);

    bool sendStatusDevice = Firebase.setString(firebaseData, "/devStatus" + String(thisRoomNum), batteryStatus);
    if (!sendStatusDevice) Serial.println(firebaseData.errorReason().c_str());
    else Serial.println("Sending OK");
    
    plugAdaptor = true;
    unplugAdaptor = false;
  }

  // if adaptor is unplugged
  if (getVIN() < 4.0 && !unplugAdaptor) {
    backlightCondition = true;
    kondisiPower = "powered by Battery\n";
    batteryStatus = "Battery";
    String reply;
    reply += thisDevice;
    reply += kondisiPower;
    reply += "Vin: ";
    reply += (float) getVIN();
    reply += " at ";
    reply += formattedTime;
    Serial.println(reply);
    myBot.sendTo(userid, reply);

    bool sendStatusDevice = Firebase.setString(firebaseData, "/devStatus" + String(thisRoomNum), batteryStatus);
    if (!sendStatusDevice) Serial.println(firebaseData.errorReason().c_str());
    else Serial.println("Sending OK");
    
    unplugAdaptor = true;
    plugAdaptor = false;
  }
}

//parameter settings for interval sending, temp and humii thershold
void setParam(int setting, const char *paramName, int dLimit, int uLimit) {
  lcd.clear();
  lcd.setCursor(0, 0), lcd.print(F("Go To Setting.."));
  digitalWrite(ledPin, HIGH);
  delay(1000);
  //myBot.sendTo(userid, "DEVICE2 is on Setting");
  digitalWrite(ledPin, LOW);

  // initial setting value in eeprom based on parameter names
  String unit;
  if (strcmp(paramName, "Temperature") == 0) {
    setting = EEPROM.read(0);
    unit = " C";
  }
  else if (strcmp(paramName, "Humidity") == 0) {
    setting = EEPROM.read(1);
    unit = " %";
  }
  else if (strcmp(paramName, "Interval") == 0) {
    setting = EEPROM.read(2);
    unit = " Min";
  }
  delay(1000);
  lcd.clear();
  while (1) {
    lcd.setCursor(0, 0), lcd.print(F("Set ")), lcd.print(paramName);
    lcd.setCursor(0, 1), lcd.print(F(">"));
    lcd.setCursor(1, 1), lcd.print(setting), lcd.print(unit);

    //function to handle value of the seting variable
    buttonHandler(setting, uLimit, dLimit, 1);

    // saved exit if button pressed
    if (button(0)) {
      lcd.clear();
      lcd.setCursor(0, 0), lcd.print(F("Saving ")), lcd.print(paramName);
      String reply;
      reply += thisDevice;
      reply += paramName;
      if (unit == " Min") reply += " sending";
      else {
        reply += " threshold";
      }
      reply += " is ";
      reply += setting;
      reply += unit;
      TBMessage msg;
      myBot.sendTo(userid, reply);
      delay(500);
      //write to eeprom based on parameters name and save to right variable
      if (strcmp(paramName, "Temperature") == 0)   tholdSuhu = setting,       EEPROM.write(0, setting);
      else if (strcmp(paramName, "Humidity") == 0) tholdKelembapan = setting, EEPROM.write(1, setting);
      else if (strcmp(paramName, "Interval") == 0) sendingInterval = setting, EEPROM.write(2, setting);
      EEPROM.commit();
      lcd.setCursor(0, 1), lcd.print(F("Saved."));
      delay(1000);
      lcd.clear();
      break;
    }
    delay(10);
  }
}

// get the formatted time and store to formattedTime variable
void formatTime() {
  time_t now = time(nullptr);
  struct tm t = *localtime(&now);
  //char formattedTime[64];
  strftime(formattedTime, sizeof(formattedTime), "%X", &t);
  //return String(msgBuffer);
}
