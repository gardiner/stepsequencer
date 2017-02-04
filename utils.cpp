#include "utils.h"


Delay::Delay(unsigned long wait) {
  this->wait = wait;
}


boolean Delay::is_due(unsigned long when) {
  if (when - this->last >= this->wait) {
    this->last = when;
    return true;
  } else {
    return false;
  }
}



Pot::Pot(int pin) {
    this->pin = pin;
}


int Pot::value() {
    this->values[this->value_index] = analogRead(this->pin);
    this->value_index = (this->value_index + 1) % POT_AVERAGE;

    //calculate average
    this->last = 0;
    for (int i = 0; i < POT_AVERAGE; i++) {
        this->last += this->values[i];
    }
    this->last /= POT_AVERAGE;

    return this->last;
}


int Pot::last_value() {
    return this->last;
}


int Pot::mapped_value(int mn, int mx) {
    return (int)round(1.0 * (this->last - POT_MIN) / (POT_MAX - POT_MIN) * (mx - mn)) + mn;
}



Button::Button(int pin) {
    this->pin = pin;
}


boolean Button::is_pressed() {
    int current = digitalRead(this->pin);
    this->was_pressed = (current == this->on) && (this->last == this->on);
    this->last = current;
    if (!this->was_pressed && (this->last == this->on)) {
        this->pressed_since = millis();
    }
    return (this->last == this->on);
}


boolean Button::is_long_press() {
    return this->was_pressed;
}


unsigned long Button::press_duration() {
    if (this->last == this->on) {
        return millis() - this->pressed_since;
    } else {
        return 0;
    }
}


int Button::last_value() {
    return this->last;
}

