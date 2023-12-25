#include <Seeed_Arduino_SSCMA.h>
#include "hidkeyboard.h"

SSCMA Infer;
HIDkeyboard keyboard;

// Set your lock screen password here
#define PASSWD "Enter your unlock password here"
// Set the screen pause time when no one is around, unit: ms
#define SCREEN_TIME 10000
unsigned long times;
bool locked = false;


class MyHIDCallbacks : public HIDCallbacks {
  void onData(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    Serial.printf("ID: %d, type: %d, size: %d\n", report_id, (int)report_type, bufsize);
    for (size_t i = 0; i < bufsize; i++) {
      Serial.printf("%d\n", buffer[i]);
    }
  }
};


void lock_screen() {
  keyboard.sendPress(227, 0, true);
  delay(10);
  keyboard.sendRelease();
  locked = true;
}

void unlock_screen() {
  keyboard.sendString(PASSWD);
  keyboard.sendKey(HID_KEY_ENTER);
  delay(10);
  keyboard.sendRelease();
  locked = false;
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin();
  keyboard.begin();
  Infer.begin();
  keyboard.setCallbacks(new MyHIDCallbacks());
  times = millis();
}


void loop() {
  if (!Infer.invoke()) {
    if (Infer.boxes().size() > 0) {
      if (locked) {
        unlock_screen();
      }
      times = millis();
    } else {
      if ((millis() - times) > SCREEN_TIME && !locked) {
        lock_screen();
      }
    }

  } else {
    if ((millis() - times) > SCREEN_TIME && !locked) {
      lock_screen();
    }
  }
}
