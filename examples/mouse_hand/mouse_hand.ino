#include <Seeed_Arduino_SSCMA.h>
#include "hidkeyboard.h"
#include "hidmouse.h"

#define RESET_NUM 3
#define CLICK_TARGET 0
#define MOVE_TARGET 1
#define SCROLL_TARGET 2

SSCMA Infer;
HIDkeyboard keyboard;
HIDmouse mouse;

int X = 0, Y = 0;
int8_t mx, my;
bool clicked = false;
int reset_mouse = RESET_NUM;

class MyHIDCallbacks : public HIDCallbacks {
  void onData(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    Serial.printf("ID: %d, type: %d, size: %d\n", report_id, (int)report_type, bufsize);
    for (size_t i = 0; i < bufsize; i++) {
      Serial.printf("%d\n", buffer[i]);
    }
  }
};

void setup() {
  // put your setup code here, to run once:
  Infer.begin();
  Serial.begin(9600);
  mouse.begin();
  keyboard.begin();
  keyboard.setCallbacks(new MyHIDCallbacks());
}

void move_mouse(int mx, int my) {
  mouse.move(mx, my);
}


void loop() {
  if (!Infer.invoke()) {
    Serial.println("INVOKE SUCCESS:");
    if (Infer.boxes().size() == 0) {
      if (reset_mouse <= 0) {
        X = 0;
        Y = 0;
      } else {
        reset_mouse -= 1;
      }
    }

    for (int i = 0; i < Infer.boxes().size() && i < 1; i++) {
      reset_mouse = RESET_NUM;
      if (Infer.boxes()[i].target == MOVE_TARGET) {
        if (X == 0 && Y == 0) {

          X = Infer.boxes()[i].x;
          Y = Infer.boxes()[i].y;
        } else {
          mx = Infer.boxes()[i].x - X;
          my = Infer.boxes()[i].y - Y;
          X = Infer.boxes()[i].x;
          Y = Infer.boxes()[i].y;
          move_mouse(-mx * 3, my * 3);
          clicked = false;
        }
      } else if (Infer.boxes()[i].target == CLICK_TARGET && !clicked) {
        mouse.pressLeft();
        clicked = true;

      } else if (Infer.boxes()[i].target == SCROLL_TARGET) {
        mouse.scrollDown(1);
        clicked = false;
      }
    }

  } else {
    Serial.println("INVOKE FAILE!");
  }
}
