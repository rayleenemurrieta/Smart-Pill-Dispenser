#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <BfButton.h>
#include <Wire.h>
#include <Encoder.h>
#include <Button.h>
#include <Adafruit_MotorShield.h>
#include "SparkFun_UHF_RFID_Reader.h"
#include <SPI.h>
#include <MFRC522.h>

#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>

#include <ArduinoJson.h>

#define ROOT_MENU_CNT  3
#define SUB_MENU1_CNT  3
#define SUB_MENU2_CNT  1
#define SUB_MENU3_CNT  1

#define SECRET_SSID "SF3"
#define SECRET_PASS "5625041967" 

#define RST_PIN 9        // Reset pin for RC522 module (RFID)
#define SS_PIN 10        // Slave Select pin for RC522 module (RFID)

//Wi-Fi

char server[] = "128.199.7.31"; // VPS IP
int port = 3000;

WiFiClient wifi;
HttpClient client = HttpClient(wifi, server, port);

//Adafruit Motor communicator

Adafruit_MotorShield AFMS = Adafruit_MotorShield();
Adafruit_StepperMotor *myMotor1 = AFMS.getStepper(2052, 1);
Adafruit_StepperMotor *myMotor2 = AFMS.getStepper(2052, 2);

// setup the emum with all the menu pages options
enum pageType {ROOT_MENU, SUB_MENU1, SUB_MENU2, SUB_MENU3};

// holds which page is currently selected
enum pageType currPage = ROOT_MENU;

// selected item pointer for the root menu
uint8_t root_Pos = 1;

// constants holding port addresses
const int BTN_ACCEPT = A0;
const int BTN_UP = A2;
const int BTN_DOWN = A1;
const int BTN_CANCEL = A3;


//Motor configurations
int MTR1 = A4;  //use the SCL and SDA functions to operate
int MTR2 = A5; //use the SCL and SDA functions to operate
int changeNum = 0;


//To initialize the LCD screen along with the ky-040
LiquidCrystal_I2C lcd(0x27, 20, 4); // I2C address 0x27, 20 columns and 4 rows
RTC_DS3231 rtc;

int btnPin = 6;  // GPIO #3 - Push button on encoder
int DT = 5;      // GPIO #4 - DT on encoder (Output B)
int CLK = 4;     // GPIO #5 - CLK on encoder (Output A)

int lcdColumns = 20;
int lcdRows = 4;

String device_id = "pill_2";               //device's product ID

DateTime NextScheduledTime;

BfButton btn(BfButton::STANDALONE_DIGITAL, btnPin, true, LOW);


//RFID code
MFRC522 rfid(10, 9);   // Create MFRC522 instance
//(SS_PIN, RST_PIN)

// define vars for testing menu
const int timeout = 10000;       //define timeout of 10 sec
char menuOption = 0;
long time0;

char daysOfWeek[7][12] = {
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday"
};

char monthNames[13][12] = {
  " ",
  "January",
  "February",
  "March",
  "April",
  "May",
  "June",
  "July",
  "August",
  "September",
  "October",
  "November",
  "December"
};

void postDispensedPills();

// =======================================================================================
// ||                                    SETUP                                          ||
// =======================================================================================
void setup() {
  lcd.init(); // Initialize the LCD I2C display
  lcd.backlight();

  // init the serial port to be used as a display return
  Serial.begin(9600);

  //Wi-Fi Configurations
  while (WiFi.begin(SECRET_SSID, SECRET_PASS) != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Connected to WiFi");

  while (!Serial) delay(10);
    Serial.println("STARTING SETUP");

    WiFi.begin(SECRET_SSID, SECRET_PASS);
    int retries = 20;
    while (WiFi.status() != WL_CONNECTED && retries-- > 0) {
      Serial.println("Connecting...");
      delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi Connected");
    } else {
      Serial.println("WiFi Failed");
    }
    Serial.println("End of setup");

  // setup the basic I/O's
  pinMode(BTN_ACCEPT, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_CANCEL, INPUT_PULLUP);

  //Motor configurations
  pinMode(MTR1, INPUT_PULLUP);
  pinMode(MTR2, INPUT_PULLUP);

  //RFID configurations
  SPI.begin();           // Init SPI bus
  rfid.PCD_Init();       // Init RFID module

  // SETUP RTC MODULE
  if (!rtc.begin()) {
    //Serial.println("RTC module is NOT found");
    Serial.flush();
    while (1);
  }
  // Setup Adafruit Motor
  while (!Serial);

  if (!AFMS.begin()) {         // create with the default frequency 1.6KHz
  // if (!AFMS.begin(1000)) {  // OR with a different frequency, say 1KHz
    Serial.println("Could not find Motor Shield. Check wiring.");
    while (1);
  }
  //Serial.println("Motor Shield found.");

  myMotor1->setSpeed(20); 
  myMotor2->setSpeed(20); 

  // Automatically sets the RTC to the date & time on PC this sketch was compiled
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  
  
}


// =======================================================================================
// ||                                  MAIN LOOP                                        ||
// =======================================================================================
void loop() { 
  Serial.println("In Loop");

    pollForSchedules();
  delay(5000); // Adjust frequency of polling as needed

  //postDispensedPills();

  bool hasRun = false;



  while (pollForSchedules) {
    page_RootMenu();
  }
}

// =======================================================================================
// ||                               SCHEDULES                                   ||
// =======================================================================================
  void pollForSchedules() {
  //Serial.println("/api/wait-for-schedule?deviceId=" + device_id);
  Serial.println("Polling for schedules...");
  client.get("/api/wait-for-schedule?deviceId=" + device_id);
  //String endpoint = "/api/wait-for-schedule?deviceId=" + device_id;
  //Serial.println(endpoint);
  //client.get(endpoint);
  int status = client.responseStatusCode();
  if (status == 200) {
    String body = client.responseBody();
    Serial.println("Got schedules:");
    //-------------Will have to set the pill time settings within this function-----------
    Serial.println(body); // You can parse using ArduinoJson if needed
  } else if (status == 204) {
    Serial.println("No schedules yet.");
  } else {
    Serial.print("Polling failed, status: ");
    Serial.println(status);
  }
}           //LCD FUNCTION ADD ON**************

// =======================================================================================
// ||                               DISPENSER                                           ||
// =======================================================================================
void postDispensedPills() {
//{"schedules":[{"pill_id":376112,"dispense_time":"14/08/2025; 09:31; pm","pill_slot":1},{"pill_id":985157,"dispense_time":"14/08/2025; 09:31; pm","pill_slot":1}]}
  // Construct JSON body
  String device_id = "pill_2";
  String pill_id = "608867";
  String dispense_time = "14/08/2025; 09:31; pm";

  // Create a JSON document
    StaticJsonDocument<256> doc;

    // Fill in the data
    doc["device_id"] = device_id;
    doc["pill_id"] = pill_id;
    doc["dispense_time"] = dispense_time;

    // Serialize to string
    String jsonBody;
    serializeJson(doc, jsonBody);
    client.beginRequest();
    client.post("/api/pill-dispensed");
    client.sendHeader("Content-Type", "application/json");
    client.sendHeader("Content-Length", jsonBody.length());
    client.beginBody();
    client.print(jsonBody);
    client.endRequest();

    int statusCode = client.responseStatusCode();
    Serial.print("HTTP Status Code: ");
    Serial.println(statusCode);

    // Print response
    while (client.available()) {
      char c = client.read();
      Serial.print(c);
    }
  
}

// =======================================================================================
// ||                               PAGE - ROOT MENU                                    ||
// =======================================================================================
void page_RootMenu(void) {

  //flag for updating the display
  boolean updateDisplay = true;
  
  // tracks when entered top of loop
  uint32_t loopStartMs;

  //tracks button states
  boolean btn_Up_WasDown = false;
  boolean btn_Down_WasDown = false;
  boolean btn_Accept_WasDown = false;
  
  //inner loop
  while (true){

    // capture start time
    loopStartMs = millis();

    // print the display
    if (updateDisplay){
      
      // clear the update flag
      updateDisplay = false;

      DateTime now = rtc.now();

      char dateBuffer[12];

      //clear the display
      clearScreen();
      lcd.clear();
      //menu title
      /* Serial.println(F("[ MAIN MENU ]"));

      //print a divider line
      printDivider();

      // print the items
      //printSelected(1, root_Pos); Serial.println(F("Settings"));
      //printSelected(2, root_Pos); Serial.println(F("Sub Menu Two"));
      //printSelected(3, root_Pos); Serial.println(F("Sub Menu Three"));
      //Serial.println();
      //Serial.println();
      
      //print a divider line
      printDivider();
*/
      lcd.setCursor(0, 0);
      lcd.print(monthNames[now.month()]);
      lcd.print(' ');
      sprintf(dateBuffer,"%02u %04u ",now.day(),now.year());
      lcd.print(dateBuffer);

      lcd.setCursor(0, 1);
      lcd.print(daysOfWeek[now.dayOfTheWeek()]);
      lcd.print(' ');
      sprintf(dateBuffer,"%02u:%02u ",now.twelveHour(),now.minute());
      lcd.print(dateBuffer);

      lcd.setCursor(0, 3);
      lcd.print(F("Next pill at hr:min"));
      
      //lcd.setCursor(0,3);
      //printLCD(1, root_Pos); lcd.print(F("Main Menu"));

    }

    // capture the button down states
   /* if (btnIsDown(BTN_UP)) {btn_Up_WasDown = true;}
    if (btnIsDown(BTN_DOWN)) {btn_Down_WasDown = true;}
    if (btnIsDown(BTN_ACCEPT)) {btn_Accept_WasDown = true;}

    //move the pointer down
    if (btn_Down_WasDown && btnIsUp(BTN_DOWN)){
      if (root_Pos == ROOT_MENU_CNT) {root_Pos = 1;} else {root_Pos++;}
      updateDisplay = true;
      btn_Down_WasDown = false;
    }

    //move the pointer Up
    if (btn_Up_WasDown && btnIsUp(BTN_UP)){
      if (root_Pos == 1) {root_Pos = ROOT_MENU_CNT;} else {root_Pos--;}
      updateDisplay = true;
      btn_Up_WasDown = false;
    }

    //move to the selected page
    if (btn_Accept_WasDown && btnIsUp(BTN_ACCEPT)){
      switch (root_Pos) {
        case 1: currPage = SUB_MENU1; return;
        case 2: currPage = SUB_MENU2; return;
        case 3: currPage = SUB_MENU3; return;
      }
    }*/
 
    // keep a specific pace
    while (millis() - loopStartMs < 25) {delay(2);}
    
  }
  
}


// =======================================================================================
// ||                               PAGE - SUB MENU1                                    ||
// =======================================================================================
void page_SubMenu1(void) {
  
  //flag for updating the display
  boolean updateDisplay = true;
  
  // tracks when entered top of loop
  uint32_t loopStartMs;

  //tracks button states
  boolean btn_Up_WasDown = false;
  boolean btn_Down_WasDown = false;
  boolean btn_Cancel_WasDown = false;

  //Motor configurations
  
  
  // selected item pointer
  uint8_t sub_Pos = 1;

  //inner loop
  while (true){

    // capture start time
    loopStartMs = millis();

    // print the display
    if (updateDisplay){
      
      // clear the update flag
      updateDisplay = false;

      //clear the display
      clearScreen();

      //menu title
      Serial.println(F("[ SUB MENU #1 ]"));
      lcd.clear();

      //print a divider line
      printDivider();

      // print the items
      printSelected(1, sub_Pos); Serial.println(F("Dispense pill 1"));
      printSelected(2, sub_Pos); Serial.println(F("Dispense pill 2"));
      printSelected(3, sub_Pos); Serial.println(F("Enter date/time"));
      //printSelected(4, sub_Pos); Serial.println(F("Enter time"));
      Serial.println();

      printLCD(1, sub_Pos); lcd.print(F("Dispense pill 1"));

      lcd.setCursor(0, 1);
      printLCD(2, sub_Pos); lcd.print(F("Dispense pill 2"));
      lcd.setCursor(0, 2);
      printLCD(3, sub_Pos); lcd.print(F("Enter date/time"));



      printDivider();
      
    }

    // capture the button down states
    if (btnIsDown(BTN_UP)) {btn_Up_WasDown = true;}
    if (btnIsDown(BTN_DOWN)) {btn_Down_WasDown = true;}
    if (btnIsDown(BTN_CANCEL)) {btn_Cancel_WasDown = true;}

    //move the pointer down
    if (btn_Down_WasDown && btnIsUp(BTN_DOWN)){
      if (sub_Pos == SUB_MENU1_CNT) {sub_Pos = 1;} else {sub_Pos++;}
      updateDisplay = true;
      btn_Down_WasDown = false;
    }

    //move the pointer Up
    if (btn_Up_WasDown && btnIsUp(BTN_UP)){
      if (sub_Pos == 1) {sub_Pos = SUB_MENU1_CNT;} else {sub_Pos--;}
      updateDisplay = true;
      btn_Up_WasDown = false;
    }

    //move to the go to the next menu
    if (btn_Cancel_WasDown && btnIsUp(BTN_CANCEL)){
      currPage = SUB_MENU2; return;}

    // keep a specific pace
    while (millis() - loopStartMs < 25) {delay(2);}
    
  }
  
}


// ======================================================================================
//                                MOTOR CONFIGURATIONS                                 ||
// ======================================================================================
void motorMovement() {
  pinMode(MTR1, INPUT_PULLUP);
  delay(1000);
  myMotor1->step(2052, FORWARD, SINGLE);
  changeNum += 1;
  delay(2000);
  pinMode(MTR1, INPUT_PULLUP);
 // postDispensedPills();
}

void motorMovement2() {
  pinMode(MTR2, INPUT_PULLUP);
  delay(1000);
  myMotor2->step(2052, FORWARD, SINGLE);
  changeNum += 1;
  delay(2000);
  pinMode(MTR2, INPUT_PULLUP);
 // postDispensedPills();
}

// =======================================================================================
// ||                               PAGE - SUB MENU2                                    ||
// =======================================================================================
void page_SubMenu2(void) {
  
  //flag for updating the display
  boolean updateDisplay = true;
  
  // tracks when entered top of loop
  uint32_t loopStartMs;

  //tracks button states
  boolean btn_Up_WasDown = false;
  boolean btn_Down_WasDown = false;
  boolean btn_Cancel_WasDown = false;
  
  // selected item pointer
  uint8_t sub_Pos = 1;

  //inner loop
  while (true){

    // capture start time
    loopStartMs = millis();

    // print the display
    if (updateDisplay){
      
      // clear the update flag
      updateDisplay = false;

      //clear the display
      clearScreen();

      //menu title
      Serial.println(F("[ SUB MENU #2 ]"));
      lcd.clear();

      //print a divider line
      printDivider();

      // print the items
      Serial.println();
      Serial.println(F("Dispensing pills..."));
      Serial.println();
      printSelected(1, sub_Pos); Serial.println(F("Next pills?"));
      Serial.println();

      lcd.print(F("     "));

      lcd.setCursor(0, 1);
      lcd.print(F("Scan Security Tag..."));
      lcd.setCursor(0, 2);
      lcd.print("   ");
      lcd.setCursor(0,3);
      printLCD(1, sub_Pos); lcd.print(F("Next pills?"));
      
      //print a divider line
      printDivider();

  
  }
      //Check RFID in this submenu
      //checkRFID();

    // capture the button down states
    if (btnIsDown(BTN_UP)) {btn_Up_WasDown = true;}
    if (btnIsDown(BTN_DOWN)) {btn_Down_WasDown = true;}
    if (btnIsDown(BTN_CANCEL)) {btn_Cancel_WasDown = true;}

    //move the pointer down
    if (btn_Down_WasDown && btnIsUp(BTN_DOWN)){
      if (sub_Pos == SUB_MENU2_CNT) {sub_Pos = 1;} else {sub_Pos++;}
      updateDisplay = true;
      btn_Down_WasDown = false;
    }

    //move the pointer Up
    if (btn_Up_WasDown && btnIsUp(BTN_UP)){
      if (sub_Pos == 1) {sub_Pos = SUB_MENU2_CNT;} else {sub_Pos--;}
      updateDisplay = true;
      btn_Up_WasDown = false;
    }

    //move to the go to the root menu
    if (btn_Cancel_WasDown && btnIsUp(BTN_CANCEL)){currPage = SUB_MENU3; return;}

    // keep a specific pace
    while (millis() - loopStartMs < 25) {delay(2);}
    
  }
  
}
// =======================================================================================
// ||                               PAGE - SUB MENU3                                    ||
// =======================================================================================
void page_SubMenu3(void) {
  
  //flag for updating the display
  boolean updateDisplay = true;
  
  // tracks when entered top of loop
  uint32_t loopStartMs;

  //tracks button states
  boolean btn_Up_WasDown = false;
  boolean btn_Down_WasDown = false;
  boolean btn_Cancel_WasDown = false;
  
  // selected item pointer
  uint8_t sub_Pos = 1;

  //inner loop
  while (true){

    // capture start time
    loopStartMs = millis();

    // print the display
    if (updateDisplay){
      
      // clear the update flag
      updateDisplay = false;

      //clear the display
      clearScreen();

      //menu title
      Serial.println(F("[ SUB MENU #3 ]"));
      lcd.clear();

      //print a divider line
      printDivider();

      // print the items
      Serial.println();
      Serial.println(F("Dispensing pills..."));
      Serial.println();
      printSelected(1, sub_Pos); Serial.println(F("Main Menu"));
      Serial.println();

      lcd.print(F("     "));

      lcd.setCursor(0, 1);
      lcd.print(F("Scan Security Tag..."));
      lcd.setCursor(0, 2);
      lcd.print("   ");
      lcd.setCursor(0,3);
      printLCD(1, sub_Pos); lcd.print(F("Main Menu"));
      
      //print a divider line
      printDivider();

    }

    checkRFID2();

    // capture the button down states
    if (btnIsDown(BTN_UP)) {btn_Up_WasDown = true;}
    if (btnIsDown(BTN_DOWN)) {btn_Down_WasDown = true;}
    if (btnIsDown(BTN_CANCEL)) {btn_Cancel_WasDown = true;}

    //move the pointer down
    if (btn_Down_WasDown && btnIsUp(BTN_DOWN)){
      if (sub_Pos == SUB_MENU3_CNT) {sub_Pos = 1;} else {sub_Pos++;}
      updateDisplay = true;
      btn_Down_WasDown = false;
    }

    //move the pointer Up
    if (btn_Up_WasDown && btnIsUp(BTN_UP)){
      if (sub_Pos == 1) {sub_Pos = SUB_MENU3_CNT;} else {sub_Pos--;}
      updateDisplay = true;
      btn_Up_WasDown = false;
    }

    //move to the go to the root menu
    if (btn_Cancel_WasDown && btnIsUp(BTN_CANCEL)){currPage = ROOT_MENU; return;}

    // keep a specific pace
    while (millis() - loopStartMs < 25) {delay(2);}
    
  }
  
}

// =======================================================================================
// ||                                  TOOLS - DISPLAY                                  ||
// =======================================================================================

void printSelected(uint8_t p1, uint8_t p2){
  if(p1 == p2){
    Serial.print(F("--> "));
  }
  else {
    Serial.print(F("    "));
  }
}

void printLCD(uint8_t p1, uint8_t p2) {
  if(p1 == p2) {
    lcd.print(F("--> "));
  }
  else {
    lcd.print(F("   "));
  }
}

void clearScreen(void){
  for (uint8_t i = 0; i < 100; i++) {Serial.println();}
}

void printDivider(void){
  for (uint8_t i = 0; i < 40; i++) {Serial.print("-");}
  Serial.println();
}
      

// =======================================================================================
// ||                             TOOLS - BUTTON PRESSING                               ||
// =======================================================================================

boolean btnIsDown(int btn){
  return digitalRead(btn) == LOW && digitalRead(btn) == LOW;
}

boolean btnIsUp(int btn){
  return digitalRead(btn) == HIGH && digitalRead(btn) == HIGH;
}


// =======================================================================================
// ||                                 RFID CONFIGURATIONS                               ||
// =======================================================================================

void checkRFID() {
  // Check if a new card is present and read the card
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    //Serial.println("RFID tag detected.");
    
    // Run motor movement function when a tag is detected
    motorMovement();


    // Halt PICC and stop encryption
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  } 

}

void checkRFID2() {
  // Check if a new card is present and read the card
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    Serial.println("RFID tag detected.");
    
    // Run motor movement function when a tag is detected
    motorMovement2();

    // Halt PICC and stop encryption
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}