#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal_I2C.h>

// --- RTC and LCD initialization ---
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Button and buzzer pins ---
#define BUTTON_HOUR A0      // Button to increase hours
#define BUTTON_MINUTE A1    // Button to increase minutes
#define BUTTON_MODE A2      // Button to change mode
#define BUZZER_PIN 7        // Buzzer output pin

// --- Operating modes ---
enum Mode {
  MODE_CLOCK,
  MODE_SET_ALARM,
  MODE_SET_TIME,
  MODE_POMODORO,
};
Mode mode = MODE_CLOCK;

// --- Alarm variables ---
int alarmHour = 7;
int alarmMinute = 0;

// --- Time setting variables ---
int setHour = 0;
int setMinute = 0;

// --- Pomodoro variables ---
unsigned long pomodoroStartTime = 0;
bool pomodoroWorkPhase = true;   // true = work, false = break
bool pomodoroRunning = false;

// --- Debounce ---
unsigned long lastDebounceTime = 0;
const int debounceDelay = 200;

void playAlarmSound() {
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 2000, 100);
    delay(500);
    noTone(BUZZER_PIN);
    delay(500);
    tone(BUZZER_PIN, 2000, 100);
    delay(500);
    noTone(BUZZER_PIN);
    delay(500);
  }
}

void playClickSound() {
  tone(BUZZER_PIN, 1000, 500);
  delay(100);
  noTone(BUZZER_PIN);
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  rtc.begin();
  // To reset the time, uncomment the next line:
  // rtc.adjust(DateTime(2025, 7, 8, 14, 13, 0));

  pinMode(BUTTON_HOUR, INPUT_PULLUP);
  pinMode(BUTTON_MINUTE, INPUT_PULLUP);
  pinMode(BUTTON_MODE, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
}

void loop() {
  DateTime now = rtc.now();

  // If in CLOCK mode and current time matches alarm time — trigger alarm
  if (mode == MODE_CLOCK) {
    if (now.hour() == alarmHour && now.minute() == alarmMinute && now.second() == 0) {
      playAlarmSound();
    }
  }

  handleButtons();

  switch (mode) {
    case MODE_CLOCK:
      updateClockDisplay();
      break;
    case MODE_SET_ALARM:
      updateAlarmSetting();
      break;
    case MODE_SET_TIME:
      updateTimeSetting();
      break;
    case MODE_POMODORO:
      updatePomodoro();
      break;
  }
}

void handleButtons() {
  if (millis() - lastDebounceTime > debounceDelay) {
    if (digitalRead(BUTTON_HOUR) == LOW) {
      handleHourButton();
      lastDebounceTime = millis();
    } else if (digitalRead(BUTTON_MINUTE) == LOW) {
      handleMinuteButton();
      lastDebounceTime = millis();
    } else if (digitalRead(BUTTON_MODE) == LOW) {
      handleModeButton();
      lastDebounceTime = millis();
    }
  }
}

void handleHourButton() {
  if (mode == MODE_SET_ALARM) {
    alarmHour = (alarmHour + 1) % 24;
  } else if (mode == MODE_SET_TIME) {
    setHour = (setHour + 1) % 24;
  }
  playClickSound();
}

void handleMinuteButton() {
  if (mode == MODE_SET_ALARM) {
    alarmMinute = (alarmMinute + 1) % 60;
  } else if (mode == MODE_SET_TIME) {
    setMinute = (setMinute + 1) % 60;
  }
  playClickSound();
}

void handleModeButton() {
  // If exiting time setting mode — update RTC
  if (mode == MODE_SET_TIME) {
    DateTime now = rtc.now();
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), setHour, setMinute, 0));
    Serial.print("RTC updated: ");
    Serial.print(setHour);
    Serial.print(":");
    Serial.println(setMinute);
  }

  // Switch to the next mode
  mode = static_cast<Mode>((mode + 1) % 4);

  // If entering time setting mode — load current time
  if (mode == MODE_SET_TIME) {
    DateTime now = rtc.now();
    setHour = now.hour();
    setMinute = now.minute();
  }

  // If entering Pomodoro mode — start timer
  if (mode == MODE_POMODORO) {
    pomodoroStartTime = millis();
    pomodoroWorkPhase = true;
    pomodoroRunning = true;
  } else {
    pomodoroRunning = false;
  }

  lcd.clear();
  playClickSound();
}

void updateClockDisplay() {
  DateTime now = rtc.now();
  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  printTwoDigits(now.hour());
  lcd.print(":");
  printTwoDigits(now.minute());
  lcd.print(":");
  printTwoDigits(now.second());

  lcd.setCursor(0, 1);
  lcd.print("Alarm: ");
  printTwoDigits(alarmHour);
  lcd.print(":");
  printTwoDigits(alarmMinute);
  lcd.print("         ");
}

void updateAlarmSetting() {
  lcd.setCursor(0, 0);
  lcd.print("Set Alarm Time ");
  lcd.setCursor(0, 1);
  printTwoDigits(alarmHour);
  lcd.print(":");
  printTwoDigits(alarmMinute);
  lcd.print("        ");
}

void updateTimeSetting() {
  lcd.setCursor(0, 0);
  lcd.print("Set Clock Time ");
  lcd.setCursor(0, 1);
  printTwoDigits(setHour);
  lcd.print(":");
  printTwoDigits(setMinute);
  lcd.print("        ");
}

void updatePomodoro() {
  if (!pomodoroRunning) return;

  unsigned long currentTime = millis();
  unsigned long elapsed = (currentTime - pomodoroStartTime) / 1000; // in seconds

  // Pomodoro phase durations
  const int WORK_DURATION = 25 * 60;
  const int BREAK_DURATION = 5 * 60;

  int duration = pomodoroWorkPhase ? WORK_DURATION : BREAK_DURATION;

  if (elapsed >= duration) {
    // Switch phase
    pomodoroWorkPhase = !pomodoroWorkPhase;
    pomodoroStartTime = currentTime;

    // Sound notification
    tone(BUZZER_PIN, 1000, 700);
  }

  int remaining = duration - elapsed;
  int minutes = remaining / 60;
  int seconds = remaining % 60;

  lcd.setCursor(0, 0);
  if (pomodoroWorkPhase) {
    lcd.print("Pomodoro: Work   ");
  } else {
    lcd.print("Pomodoro: Break  ");
  }

  lcd.setCursor(0, 1);
  printTwoDigits(minutes);
  lcd.print(":");
  printTwoDigits(seconds);
  lcd.print("        ");
}

void printTwoDigits(int number) {
  if (number < 10) lcd.print("0");
  lcd.print(number);
}
