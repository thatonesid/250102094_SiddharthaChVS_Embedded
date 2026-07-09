// FILES INCLUDED

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

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

// STATE MACHINE.

enum
{
  EMPTY,
  CNC,
  CIC,
  CC,
  LOCKOUT,
  EMERGENCY
} currentState;
String entered;
unsigned long LastTimeKeyEntered, LockoutStart, checkt;
int strikes;
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
void cCorrect(unsigned long timern);


// LOCKOUT-STATE
void handleLockout(unsigned long timern);

// EMERGENCY-STATE
void enterEmergencyState();
void handleEmergency(unsigned long timern);


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
    entered = "";

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
    y.off();
    if (timern - checkt < 2000)
        return;

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("ENTER PASSCODE:");
    checkt = timern;
    currentState = EMPTY;
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
  entered = "";
  currentRole = "";

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

  case LOCKOUT:
    handleLockout(timern);
    break;

  case EMERGENCY:
    handleEmergency(timern);
    break;
  }
}
