#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <Wire.h>
#include <EEPROM.h>
#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <Stepper.h>

// ======================= CONFIGURATIONS =======================
#define SECRET_SSID "SF3"
#define SECRET_PASS "5625041967"

#define BTN_ACCEPT A0
#define BTN_UP A2
#define BTN_DOWN A1
#define BTN_CANCEL A3
#define MOTOR_SPEED 10
#define STEPS_PER_REV 2048

// ======================= NETWORK CONFIG =======================
char server[] = "128.199.7.31"; // VPS IP
int port = 3000;

// ======================= HARDWARE OBJECTS =======================
LiquidCrystal_I2C lcd(0x27, 20, 4);
RTC_DS3231 rtc;

// Stepper Motors (ULN2003 + 28BYJ-48)
Stepper motor1(STEPS_PER_REV, 2, 4, 3, 5);  // IN1, IN3, IN2, IN4
Stepper motor2(STEPS_PER_REV, 6, 8, 7, 9);  // IN1, IN3, IN2, IN4

String device_id = "pill_2";  // Device unique ID

char daysOfWeek[7][12] = {
  "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
};

char monthNames[13][12] = {
  " ","January","February","March","April","May","June","July","August",
  "September","October","November","December"
};

// ======================= FUNCTION DECLARATIONS =======================
void pollForSchedules();
void postDispensedPills();
void page_RootMenu();
void checkSchedules();
void motorMovement();
void motorMovement2();
void saveScheduleToEEPROM(int hour, int minute, int slot);
void clearEEPROM();
void displayNextPillTime();
bool allSchedulesPassed();

// ======================= STATE VARIABLES =======================
bool hasSchedule = false;
struct ScheduleData {
  int hour;
  int minute;
  int slot;
  unsigned long pill_id;
};
// =======================================================================================
//                                      SETUP
// =======================================================================================
void setup() {
  Serial.begin(9600);
  while (!Serial);

  lcd.init();
  lcd.backlight();

  pinMode(BTN_ACCEPT, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_CANCEL, INPUT_PULLUP);

  Wire.begin();

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    while (1);
  }

  // Initialize Steppers
  motor1.setSpeed(MOTOR_SPEED);
  motor2.setSpeed(MOTOR_SPEED);

  // WiFi Connection
  Serial.println("Connecting to WiFi...");
  int retries = 20;
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED && retries-- > 0) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
  } else {
    Serial.println("\nWiFi Failed.");
  }

  // Set RTC time to compile time (first upload only)
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Initialized");
  delay(1000);
  lcd.clear();
}

// =======================================================================================
//                                      MAIN LOOP
// =======================================================================================
void loop() {

    pollForSchedules();

  checkSchedules();
  page_RootMenu();

  delay(1000);
}

// =======================================================================================
//                                  POLL FOR SCHEDULES
// =======================================================================================
void pollForSchedules() {
  Serial.println("Polling for schedules...");

  WiFiClient wifi;
  HttpClient client = HttpClient(wifi, server, port);
  client.get("/api/wait-for-schedule?deviceId=" + device_id);

  int status = client.responseStatusCode();
  String body = client.responseBody();

  Serial.print("Status: ");
  Serial.println(status);
  Serial.print("Body: ");
  Serial.println(body);

  if (status == 200 && body.length() > 0) {
     StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
      Serial.println("Failed to parse schedule JSON");
      return;
    }

    JsonArray schedules = doc["schedules"];
    int scheduleCount = schedules.size();
    EEPROM.write(0, scheduleCount);

    clearEEPROM(); // remove old schedules

    for (JsonObject schedule : schedules) {
      Serial.println("Printing Schedules...");
      serializeJson(schedule, Serial);
      Serial.println();
      String dispenseTime = schedule["dispense_time"];
      int pillSlot = schedule["pill_slot"];
      unsigned long pill_id = schedule["pill_id"];
      Serial.println(pill_id);

      // Parse the "dispense_time" string → "20/10/2025; 09:27; pm"
      int firstSemi = dispenseTime.indexOf(';');
      int secondSemi = dispenseTime.indexOf(';', firstSemi + 1);

      String time = dispenseTime.substring(firstSemi + 1, secondSemi);
      String meridian = dispenseTime.substring(secondSemi + 1);

      time.trim();
      meridian.trim();

      // Split time (09:27) into hour and minute
      int colonPos = time.indexOf(':');
      int hour = time.substring(0, colonPos).toInt();
      int minute = time.substring(colonPos + 1).toInt();

      // Convert to 24-hour format
      if (meridian.equalsIgnoreCase("pm") && hour != 12) hour += 12;
      if (meridian.equalsIgnoreCase("am") && hour == 12) hour = 0;

      saveScheduleToEEPROM(hour, minute, pillSlot, pill_id);
    }

    hasSchedule = true;
    //Serial.println("Schedules saved. Polling paused until all times pass.");
  } else if (status == 204) {
    Serial.println("No schedules yet.");
  } else {
    Serial.println("Poll failed.");
  }

  client.stop();
}


// =======================================================================================
//                                SAVE TO EEPROM
// =======================================================================================
/* void saveScheduleToEEPROM(int hour, int minute, int slot, unsigned long pill_id) {
  Serial.println(pill_id);
  int index = EEPROM.read(0);
  int baseAddr = 1 + index * 5;
  EEPROM.write(baseAddr, hour);
  EEPROM.write(baseAddr + 1, minute);
  EEPROM.write(baseAddr + 2, slot);
  EEPROM.write(baseAddr + 3, pill_id);
  EEPROM.write(0, index + 1);

  Serial.print("Saved schedule ");
  Serial.print(hour);
  Serial.print(":");
  if (minute < 10) Serial.print('0');
  Serial.println(minute);
}*/

void saveScheduleToEEPROM(int hour, int minute, int slot, unsigned long pill_id) {
  int index = EEPROM.read(0);
  int baseAddr = 1 + index * sizeof(ScheduleData);

  ScheduleData data;
  data.hour = hour;
  data.minute = minute;
  data.slot = slot;
  data.pill_id = pill_id;

  EEPROM.put(baseAddr, data);
  EEPROM.write(0, index + 1);

  Serial.print("Saved schedule ");
  Serial.print(hour);
  Serial.print(":");
  if (minute < 10) Serial.print('0');
  Serial.println(minute);
}


// =======================================================================================
//                                CHECK SCHEDULES
// =======================================================================================
void checkSchedules() {
  DateTime now = rtc.now();
  int scheduleCount = EEPROM.read(0);

  for (int i = 0; i < scheduleCount; i++) {
    int baseAddr = 1 + i * sizeof(ScheduleData);

    ScheduleData data;
    EEPROM.get(baseAddr, data);

    if (now.hour() == data.hour && now.minute() == data.minute) {
      Serial.print("Dispensing pills from slot ");
      Serial.println(data.slot);

      if (data.slot == 1) motorMovement();
      else if (data.slot == 2) motorMovement2();

      postDispensedPills(data.hour, data.minute, data.slot, data.pill_id);
      delay(60000);
    }
  }
}


/*void checkSchedules() {
  DateTime now = rtc.now();
  int scheduleCount = EEPROM.read(0);

  for (int i = 0; i < scheduleCount; i++) {
    int baseAddr = 1 + i * 5;
    int hour = EEPROM.read(baseAddr);
    int minute = EEPROM.read(baseAddr + 1);
    int slot = EEPROM.read(baseAddr + 2);
    unsigned long pill_id = EEPROM.read(baseAddr + 3);

    if (now.hour() == hour && now.minute() == minute) {
      Serial.print("Dispensing pills from slot ");
      Serial.println(slot);

      if (slot == 1) motorMovement();
      else if (slot == 2) motorMovement2();

      postDispensedPills(hour, minute, slot, pill_id);
      delay(60000); // Wait one minute before next dispense
    }
  }
}*/

// =======================================================================================
//                                CLEAR EEPROM
// =======================================================================================
void clearEEPROM() {
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }
  Serial.println("EEPROM cleared.");
}

// =======================================================================================
//                            CHECK IF ALL SCHEDULES PASSED
// =======================================================================================
bool allSchedulesPassed() {
  DateTime now = rtc.now();
  int scheduleCount = EEPROM.read(0);
  if (scheduleCount == 0) return true;

  int latestHour = 0, latestMinute = 0;
  for (int i = 0; i < scheduleCount; i++) {
    int baseAddr = 1 + i * 5;
    int hour = EEPROM.read(baseAddr);
    int minute = EEPROM.read(baseAddr + 1);
    if ((hour > latestHour) || (hour == latestHour && minute > latestMinute)) {
      latestHour = hour;
      latestMinute = minute;
    }
  }

  return (now.hour() > latestHour) || (now.hour() == latestHour && now.minute() > latestMinute);
}

// =======================================================================================
//                              DISPLAY: ROOT MENU (LCD)
// =======================================================================================
void page_RootMenu(void) {
  static unsigned long lastUpdate = 0;
  DateTime now = rtc.now();

  if (millis() - lastUpdate < 1000) return;
  lastUpdate = millis();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(monthNames[now.month()]);
  lcd.print(' ');
  lcd.print(now.day());
  lcd.print(", ");
  lcd.print(now.year());

  lcd.setCursor(0, 1);
  lcd.print(daysOfWeek[now.dayOfTheWeek()]);
  lcd.print(' ');
  int displayHour = now.twelveHour();
  if (displayHour == 0) displayHour = 12;
  lcd.print(displayHour);
  lcd.print(':');
  if (now.minute() < 10) lcd.print('0');
  lcd.print(now.minute());
  lcd.print(now.isPM() ? " PM" : " AM");

  lcd.setCursor(0, 3);
  lcd.print("Next pill at:");
  displayNextPillTime();
}

// =======================================================================================
//                           DISPLAY NEXT UPCOMING TIME
// =======================================================================================
void displayNextPillTime() {
  DateTime now = rtc.now();
  int scheduleCount = EEPROM.read(0);  // how many schedules are stored
  int nextHour = -1, nextMinute = -1;

  // Iterate through all schedules stored in EEPROM
  for (int i = 0; i < scheduleCount; i++) {
    int baseAddr = 1 + i * sizeof(ScheduleData);  // compute correct address for each schedule

    ScheduleData data;
    EEPROM.get(baseAddr, data);  // read one full schedule (hour, minute, slot, pill_id)

    // Find the next upcoming time
    if ((data.hour > now.hour()) || 
        (data.hour == now.hour() && data.minute > now.minute())) {
      nextHour = data.hour;
      nextMinute = data.minute;
      break;
    }
  }

  // --- Display on LCD ---
  lcd.setCursor(14, 3);
  if (nextHour != -1) {
    int displayHour = nextHour % 12;
    if (displayHour == 0) displayHour = 12;
    lcd.print(displayHour);
    lcd.print(':');
    if (nextMinute < 10) lcd.print('0');
    lcd.print(nextMinute);
    lcd.print(nextHour >= 12 ? "PM" : "AM");
  } else {
    lcd.print("--:--");
  }
}


/*void displayNextPillTime() {
  DateTime now = rtc.now();
  int scheduleCount = EEPROM.read(0);
  int nextHour = -1, nextMinute = -1;

  for (int i = 0; i < scheduleCount; i++) {
    int baseAddr = 1 + i * 5;
    int hour = EEPROM.read(baseAddr);
    int minute = EEPROM.read(baseAddr + 1);

    if ((hour > now.hour()) || (hour == now.hour() && minute > now.minute())) {
      nextHour = hour;
      nextMinute = minute;
      break;
    }
  }

  lcd.setCursor(14, 3);
  if (nextHour != -1) {
    int displayHour = nextHour % 12;
    if (displayHour == 0) displayHour = 12;
    lcd.print(displayHour);
    lcd.print(':');
    if (nextMinute < 10) lcd.print('0');
    lcd.print(nextMinute);
    lcd.print(nextHour >= 12 ? "PM" : "AM");
  } else {
    lcd.print("--:--");
  }
}*/

// =======================================================================================
//                                MOTOR MOVEMENT (UPDATED)
// =======================================================================================
void motorMovement() {
  Serial.println("Running motor 1...");
  motor1.step(STEPS_PER_REV);
  delay(500);
}

void motorMovement2() {
  Serial.println("Running motor 2...");
  motor2.step(STEPS_PER_REV);
  delay(500);
}

// =======================================================================================
//                                POST DISPENSE EVENT
// =======================================================================================
void postDispensedPills(int hour, int minute, int slot, unsigned long pill_id) {
  WiFiClient wifi;
  HttpClient client(wifi, server, port);

  StaticJsonDocument<256> doc;
  doc["device_id"] = device_id;
  doc["pill_id"] = pill_id;
  doc["dispense_time"] = String(hour) + ":" + String(minute);
  doc["pill_slot"] = slot;

  String jsonBody;
  serializeJson(doc, jsonBody);
  Serial.println(jsonBody);
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
  client.stop();
}
