#ifndef utils_h
#define utils_h

#define POT_MIN 0
#define POT_MAX 1023
#define POT_AVERAGE 20


#include <Arduino.h>


class Delay {
  protected:
    unsigned long wait = 0;
    unsigned long last = 0;
  public:
    Delay(unsigned long wait);
    boolean is_due(unsigned long when);
};


class Pot {
  protected:
    int pin;
    int last;
    int value_index = 0;
    int values[POT_AVERAGE];
  public:
    Pot(int pin);
    int value();
    int last_value();
    int mapped_value(int min, int max);
};


class Button {
  protected:
    int pin;
    int last;
    int on = LOW;
    boolean was_pressed = false;
    unsigned long pressed_since;
  public:
    Button(int pin);
    boolean is_pressed();
    boolean is_long_press();
    unsigned long press_duration();
    int last_value();
};


#endif //utils_h