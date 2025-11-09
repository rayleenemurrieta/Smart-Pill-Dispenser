#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <Wire.h>
#include <Stepper.h>

// ======================= CONFIGURATIONS =======================
#define MOTOR_SPEED 10
#define STEPS_PER_REV 2048

// ======================= HARDWARE OBJECTS =======================
LiquidCrystal_I2C lcd(0x27, 20, 4);
RTC_DS3231 rtc;

// Stepper Motors (ULN2003 + 28BYJ-48)
Stepper motor1(STEPS_PER_REV, 2, 4, 3, 5);  // IN1, IN3, IN2, IN4
Stepper motor2(STEPS_PER_REV, 6, 8, 7, 9);  // IN1, IN3, IN2, IN4

// ======================= TIME SETTINGS =======================
// Hardcoded dispense times (edit these for demo)
const int DISPENSE1_HOUR = 13;   // 1 PM
const int DISPENSE1_MINUTE = 17;  // 1:05 PM
const int DISPENSE2_HOUR = 13;   // 1 PM
const int DISPENSE2_MINUTE = 18;  // 1:06 PM

// ======================= UTILITY ARRAYS =======================
char daysOfWeek[7][12] = {
  "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
};

char monthNames[13][12] = {
  " ","January","February","March","April","May","June","July","August",
  "September","October","November","December"
};

// =======================================================================================
//                                      SETUP
// =======================================================================================
void setup() {
  Serial.begin(9600);
  while (!Serial);

  lcd.init();
  lcd.backlight();
  Wire.begin();

  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    lcd.print("RTC not found!");
    while (1);
  }

  // Uncomment once if RTC shows wrong date/time
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  motor1.setSpeed(MOTOR_SPEED);
  motor2.setSpeed(MOTOR_SPEED);

  showSplashScreen();
}

// =======================================================================================
//                                      LOOP
// =======================================================================================
void loop() {
  DateTime now = rtc.now();

  displayHomeScreen(now);

  // 1:05 PM → Motor 1
  if (now.hour() == DISPENSE1_HOUR && now.minute() == DISPENSE1_MINUTE) {
    dispenseMotor(1);
    waitForNextMinute(now);
  }

  // 1:06 PM → Motor 2
  if (now.hour() == DISPENSE2_HOUR && now.minute() == DISPENSE2_MINUTE) {
    dispenseMotor(2);
    waitForNextMinute(now);
  }

  delay(500);
}

// =======================================================================================
//                                      FUNCTIONS
// =======================================================================================

void showSplashScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Pill Dispenser");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(1500);
  lcd.clear();
}

void displayHomeScreen(DateTime now) {
  static unsigned long lastUpdate = 0;
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

  lcd.setCursor(0, 2);
  lcd.print("Pill 1: ");
  lcd.print(formatTime(DISPENSE1_HOUR, DISPENSE1_MINUTE));

  lcd.setCursor(0, 3);
  lcd.print("Pill 2: ");
  lcd.print(formatTime(DISPENSE2_HOUR, DISPENSE2_MINUTE));
}

String formatTime(int hour24, int minute) {
  int hour12 = hour24 % 12;
  if (hour12 == 0) hour12 = 12;
  String ampm = (hour24 >= 12) ? "PM" : "AM";
  char buffer[8];
  sprintf(buffer, "%d:%02d%s", hour12, minute, ampm.c_str());
  return String(buffer);
}

void dispenseMotor(int slot) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dispensing Slot ");
  lcd.print(slot);
  Serial.print("Dispensing slot ");
  Serial.println(slot);

  if (slot == 1) motor1.step(STEPS_PER_REV);
  else motor2.step(STEPS_PER_REV);

  lcd.setCursor(0, 2);
  lcd.print("Pill dispensed!");
  delay(1000);

  // Instantly return to main screen
  DateTime now = rtc.now();
  displayHomeScreen(now);
}

void waitForNextMinute(DateTime currentTime) {
  int currentMinute = currentTime.minute();
  while (rtc.now().minute() == currentMinute) {
    delay(500);
  }
}
