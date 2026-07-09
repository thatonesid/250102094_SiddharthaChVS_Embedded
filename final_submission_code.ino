// FILES INCLUDED

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <time.h>

// RBAC BEGIN

struct Cango
{
  int id;
  String role;
};

Cango users[] = {
    {6971, "Owner"},
    {8751, "Manager1"},
    {8752, "Manager2"},
    {9821, "Admin1"}, 
    {9822, "Admin2"},
    {9823, "Admin3"},
    {4331, "Employee1"},
    {4332, "Employee2"},
    {4333, "Employee3"},
    {4334, "Employee4"},
    {4335, "Employee5"},};

const int NUM_USERS = sizeof(users) / sizeof(users[0]);
String currentRole; //Stores Role when entered code is correct.

//LOG 
struct Log{

String id;

String time;

};

//OFFLINE QUEUE MAINTENANCE 
Log queue[10];
int front = 0;
int rear = 0;
int count = 0;

// RBAC END



// KEYPAD BEGIN

const byte ROWS = 4; // four rows
const byte COLS = 3; // three columns
// define the symbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}};
byte rowPins[ROWS] = {32, 33, 25, 26}; // connect to the row pinouts of the keypad
byte colPins[COLS] = {19, 18, 5};      // connect to the column pinouts of the keypad

// initialize an instance of class NewKeypad
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// KEYPAD END


// LCD BEGIN

LiquidCrystal_I2C lcd(0x27, 20, 4);

// LCD END


// LED CLASS

class LED
{

  // Initialized at Start
  int ledPin;
  byte ledState;

public:
  LED(int pin)
  {
    ledPin = pin;
    pinMode(ledPin, OUTPUT);
    ledState = LOW;
  }

  void off()
  {
    ledState = LOW;
    digitalWrite(ledPin, ledState);
  }

  void on()
  {
    ledState = HIGH;
    digitalWrite(ledPin, ledState);
  }
};


//WIFI AND SERVER
const char* serverName = "https://httpbun.com"; //Where the JSON objects are Posted
const char* ssid = "Wokwi-GUEST"; //WiFi Name
const char* password = "";  //WiFi Password

JsonDocument doc;
bool uploaded; //This ensures that JSON object is uploaded only once.

unsigned long uploadTimer = 0;
const unsigned long UPLOAD_INTERVAL = 500; //Interval between each HTTP Post to ensure smooth flow.

//TIME
// Time configuration
const long GMT_OFFSET_SEC = 19800;   // IST = UTC +5:30
const int DAYLIGHT_OFFSET_SEC = 0;
const int DEL = 1000;
unsigned long timegetime = 0;

// Time strings
String formattedDate;   // YYYY-MM-DD HH:MM:SS


// STATE MACHINE.

enum
{
  EMPTY,
  CNC,
  CIC,
  CC,
  UPLOADING,
  LOCKOUT,
  EMERGENCY
} currentState;
String entered;
unsigned long LastTimeKeyEntered, LockoutStart, checkt;
int strikes;
bool isValid(String entered);
void runMachine(char key);
LED y(14);

// STATE MACHINE's FUNTCIONS

// MINI FUNCTIONS DECLARATIONS
// EMPTY-STATE
void cEmpty(char key, unsigned long timern);

// CNC-STATE
bool codeEntryTimedout(unsigned long timern);
void removeDigit();
void processKey(char key, unsigned long timern);
void displayCodeEntered();
bool isValid(String s);
void verifyCode(unsigned long timern);
void cCodeEntry(char key, unsigned long timern);

// CIC-STATE
void cIncorrect(unsigned long timern);

// CC-STATE
void enqueueLog(String s, String t);
bool dequeueLog(Log &log);
bool queueFull();
bool queueEmpty();
void flushQueue();
void cCorrect(unsigned long timern);


// LOCKOUT-STATE
void handleLockout(unsigned long timern);

// EMERGENCY-STATE
void enterEmergencyState();
void handleEmergency(unsigned long timern);

//UPLOADING-ONLINE FUNCTIONS
void getTime();
bool executePost(String entered, String time);
void uploadLog();

// MAIN FUNCTION
void runMachine(char key);

// EMERGENCY RESET:
volatile bool emergencyTriggered = false;
const int emergencyPin = 2;

void emergencyISR()
{
  emergencyTriggered = true;
}

void checkEmergency()
{
  if (!emergencyTriggered)
    return;

  emergencyTriggered = false;
  enterEmergencyState();
}

void enterEmergencyState()
{
  currentState = EMERGENCY;
  checkt = millis();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("EMERGENCY");
  lcd.setCursor(0, 1);
  lcd.print("OVERRIDE");
}



// SETUP
void setup()
{
  lcd.init();
  lcd.backlight();

  currentState = EMPTY;
  strikes = 0;
  entered = "";

  LastTimeKeyEntered = 0;
  LockoutStart = 0;
  checkt = 0;

  y.off();

  pinMode(emergencyPin, INPUT_PULLUP);

  attachInterrupt(
      digitalPinToInterrupt(emergencyPin),
      emergencyISR,
      FALLING);

  Serial.begin(115200);
  WiFi.begin(ssid, password,6);
  Serial.println("Connecting");

  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC,
           "pool.ntp.org",
           "time.google.com");

  struct tm timeinfo;

  while (!getLocalTime(&timeinfo)) {
      if(millis() - timegetime > DEL){
        Serial.println("Waiting for NTP...");
        lcd.setCursor(0, 0);
        lcd.print("LOADING..");
        timegetime = millis();
      }
  }

  Serial.println("Time synchronized!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ENTER PASSCODE:");

}

// LOOP
void loop()
{

  checkEmergency();
  char key = customKeypad.getKey();
  runMachine(key);
  flushQueue();

}

// MINI FUNCTIONS' DEFINITIONS

// WHEN STATE IS EMPTY
void cEmpty(char key, unsigned long timern)
{
  if (key >= '0' && key <= '9')
  {
    entered = String(key);
    LastTimeKeyEntered = timern;
    y.on();
    currentState = CNC;
  }
}

// WHEN STATE IS CODE IS NOT COMPLETE
void displayCodeEntered()
{

  String stars = "";

  for (int i = 0; i < entered.length(); i++)
    stars += '*';

  lcd.setCursor(0, 1);
  lcd.print(stars);
}

void cCodeEntry(char key, unsigned long timern)
{

  y.on();

  if (codeEntryTimedout(timern))
    return;

  processKey(key, timern);

  if (entered.length() == 0)
  {
    currentState = EMPTY;
    return;
  }

  displayCodeEntered();

  if (entered.length() == 4)
    verifyCode(timern);
}

bool codeEntryTimedout(unsigned long timern)
{

  if (timern - LastTimeKeyEntered < 7000)
    return false;

  entered = "";
  currentState = EMPTY;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ENTER PASSCODE:");

  return true;
}

void removeDigit()
{

  if (entered.length() == 0)
    return;

  lcd.setCursor(entered.length() - 1, 1);
  lcd.print(' ');

  entered.remove(entered.length() - 1);
}

void processKey(char key, unsigned long timern)
{

  if (!key)
    return;

  LastTimeKeyEntered = timern;

  if (key == '*')
  {
    removeDigit();
    return;
  }

  if (key >= '0' && key <= '9')
    entered += key;
}

bool isValid(String entered)
{

  int code = entered.toInt();

  for (int i = 0; i < NUM_USERS; i++)
  {
    if (users[i].id == code)
    {
      currentRole = users[i].role;
      return true;
    }
  }

  currentRole = "";
  return false;
}

void verifyCode(unsigned long timern)
{

  checkt = timern;

  if (isValid(entered))
  {

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(currentRole);

    lcd.setCursor(0, 1);
    lcd.print("ACCESS GRANTED");
    strikes = 0;

    currentState = CC;
  }
  else
  {

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Incorrect");

    lcd.setCursor(0, 1);

    if (strikes + 1 < 3)
      lcd.print("Try Again");
    else
      lcd.print("LOCKED 15 SEC");

    currentState = CIC;
  }
}

// WHEN STATE IS CODE ENTERRED IS INCORRECT

void cIncorrect(unsigned long timern)
{
  
  y.off();

  if (timern - checkt < 2000)
    return;

  strikes++;
  entered = "";

  if (strikes < 3)
  {

    currentState = EMPTY;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ENTER PASSCODE:");
  }
  else
  {

    LockoutStart = timern;
    currentState = LOCKOUT;
  }
}

void cCorrect(unsigned long timern)
{
    if (timern - checkt < 1500)
        return;

    checkt = timern;
    currentState = UPLOADING;
}

// WHEN STATE IS LOCKOUT
void handleLockout(unsigned long timern)
{

  if (timern - LockoutStart < 15000)
    return;

  strikes = 0;
  entered = "";

  currentState = EMPTY;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ENTER PASSCODE:");

  y.off();
}

// EMERGENCY STATE FUNCTION

void handleEmergency(unsigned long timern)
{

  
  if (timern - checkt < 2000)
    return;

  currentState = EMPTY;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ENTER PASSCODE:");
}

// MAIN FUNCTION 
void runMachine(char key)
{

  unsigned long timern = millis();

  switch (currentState)
  {
  case EMPTY:
    cEmpty(key, timern);
    break;

  case CNC:
    cCodeEntry(key, timern);
    break;

  case CIC:
    cIncorrect(timern);
    break;

  case CC:
    cCorrect(timern);
    break;
  
  case UPLOADING:
    uploadLog();
    break;

  case LOCKOUT:
    handleLockout(timern);
    break;

  case EMERGENCY:
    handleEmergency(timern);
    break;
  }
}

void getTime() {

    struct tm timeinfo;

    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }

    char buffer[20];

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    formattedDate = String(buffer);
    
}


bool executePost(String id, String time) {

    JsonDocument doc;

    doc["id"] = id;
    doc["time"] = time;

    String payload;
    serializeJson(doc, payload);

    HTTPClient http;

    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(payload);

    Serial.print("HTTP Response Code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode == 200) {

        Serial.println("POST Successful");
        Serial.print("ID: ");
        Serial.print(id);
        Serial.print(", DATE-TIME: ");
        Serial.println(time);

        http.end();
        return true;
    }

    Serial.print("POST Failed : ");
    Serial.println(httpResponseCode);

    http.end();
    return false;
}

bool queueFull() {
    return count == 10;
}

bool queueEmpty() {
    return count == 0;
}

void enqueueLog(String id, String time) {

    if (queueFull()) {
        Serial.println("Queue Full! Log Discarded.");
        return;
    }

    queue[rear].id = id;
    queue[rear].time = time;

    rear = (rear + 1) % 10;
    count++;

    Serial.print("Stored Log. Queue Size: ");
    Serial.println(count);
}

bool dequeueLog(Log &entry) {

    if (queueEmpty())
        return false;

    entry = queue[front];

    front = (front + 1) % 10;
    count--;

    return true;
}

void flushQueue() {

    if (WiFi.status() != WL_CONNECTED || queueEmpty())
        return;

    if (millis() - uploadTimer < UPLOAD_INTERVAL)
        return;

    uploadTimer = millis();

    Log logEntry = queue[front];

    if (executePost(logEntry.id, logEntry.time)) {

        dequeueLog(logEntry);

        Serial.println("Queued Log Uploaded");
    }
}

void uploadLog()
{
    getTime();

    if (WiFi.status() == WL_CONNECTED) {

        if (!executePost(entered, formattedDate))
            enqueueLog(entered, formattedDate);

    }
    else {
        enqueueLog(entered, formattedDate);
    }

    entered = "";

    lcd.clear();
    lcd.print("ENTER PASSCODE:");

    currentState = EMPTY;
}
