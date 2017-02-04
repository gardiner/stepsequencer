/*
 * Midi pad and step sequencer.
 */
#include <Keypad.h>
#include <LedControl.h> //led matrix library
#include <TM1638.h> //led&key

#include "utils.h"


#define KEYPAD_ROWS 4
#define KEYPAD_COLS 4

#define MIDI_NOTE_ON 144 //144 = 10010000 in binary, note on command on channel 0
#define MIDI_NOTE_OFF 128 //128 = 10000000 in binary, note off command on channel 0
#define MIDI_CC 176 //176 = midi cc on channel 0

#define DELAY_CTRL 20
#define DELAY_DISP 100
#define DELAY_DEBUG 2000
#define TOLERANCE_POT 8

#define DEBUG true


//mode
enum Mode {
    MODE_PADS, //pad mode - keypad plays notes
    MODE_STEP, //step sequencer mode - keypad toggles steps
    MODE_KNOB, //knob mode - keypad toggles control channel for knob
    MODE_TONE, //pitch bend/mod wheel mode
};
Mode mode;

//keypad
byte keypad_col_pins[KEYPAD_COLS] = {9, 10, 11, 12};
byte keypad_row_pins[KEYPAD_ROWS] = {8, 7, 6, 5};
char keypad_keys[KEYPAD_ROWS][KEYPAD_COLS] = {
    {12,13,14,15},
    { 8, 9,10,11},
    { 4, 5, 6, 7},
    { 0, 1, 2, 3},
};
Keypad keypad = Keypad(makeKeymap(keypad_keys), keypad_row_pins, keypad_col_pins, KEYPAD_ROWS, KEYPAD_COLS);

//ledkey
byte ledkey_stb_pin = 4;
byte ledkey_clk_pin = 3;
byte ledkey_dio_pin = 2;
char ledkey_display[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
byte ledkey_buttons = 0;
word ledkey_leds = 0;
TM1638 ledkey = TM1638(ledkey_dio_pin, ledkey_clk_pin, ledkey_stb_pin, true, 0);

//ledmatrix
byte ledmatrix_din_pin = A2;
byte ledmatrix_cs_pin = A3;
byte ledmatrix_clk_pin = A4;
LedControl ledmatrix = LedControl(ledmatrix_din_pin, ledmatrix_clk_pin, ledmatrix_cs_pin, 2);

//pots
byte pot1_pin = A0;
int pot1_last;
Pot pot1 = Pot(pot1_pin);

byte pot2_pin = A1;
int pot2_last;
Pot pot2 = Pot(pot2_pin);

//led
int led_pin = 13;

//time
unsigned long now = 0;
unsigned long last_ctrl = 0;
unsigned long last_disp = 0;
unsigned long last_debug = 0;

//status
byte base_note = 0;
byte playing_num = 0;
byte playing[16];


void setup() {
    //keypad
    keypad.addEventListener(keypad_event); // Add an event listener for this keypad

    //displays
    ledmatrix.shutdown(0, false);
    ledmatrix.setIntensity(0, 0);
    ledmatrix.clearDisplay(0);

    ledmatrix.shutdown(1, false);
    ledmatrix.setIntensity(1, 0);
    ledmatrix.clearDisplay(1);

    //pots
    pinMode(pot1_pin, INPUT);
    pinMode(pot2_pin, INPUT);

    //led
    pinMode(led_pin, OUTPUT);

    //init mode
    mode = MODE_PADS;
    disp("PAdS", 0);
    ledkey_leds = 1;

    if (DEBUG) {
        Serial.begin(9600); //console/debug
    } else {
        Serial.begin(31250); //midi
    }

    delay(500);

    ledmatrix.setRow(0, 0, 1 | 4 | 16 | 64);
    ledmatrix.setRow(0, 1, 2 | 8 | 32 | 128);
    ledmatrix.setRow(1, 0, 1 | 4 | 16 | 64);
    ledmatrix.setRow(1, 1, 2 | 8 | 32 | 128);
}


void loop() {
    now = millis();

    //keypad
    char key = keypad.getKey();

    if ((now - last_ctrl) > DELAY_CTRL) {
        last_ctrl = now;

        //pots
        if (abs(pot1.value()-pot1_last) >= TOLERANCE_POT) {
            pot1_last = pot1.last_value();
            base_note = pot1.mapped_value(0, 127);
            disp(pad4(base_note), 4);
            ledkey_display[4] = 'B';
        }

        if (abs(pot2.value()-pot2_last) >= TOLERANCE_POT) {
            pot2_last = pot2.last_value();
            disp(pad4(pot2.mapped_value(0, 127)), 4);
            ledkey_display[4] = 'C';
            //midi_cc(73, analog2midi(pot2));
        }

        //ledkey buttons
        ledkey_buttons = ledkey.getButtons();
        if ((ledkey_buttons & 1) == 1) {
            mode = MODE_PADS;
            disp("PAdS", 0);
            ledkey_leds = 1;
        } else if ((ledkey_buttons & 2) == 2) {
            mode = MODE_STEP;
            disp("StEP", 0);
            ledkey_leds = 2;
        } else if ((ledkey_buttons & 4) == 4) {
            mode = MODE_KNOB;
            disp("CtrL", 0);
            ledkey_leds = 4;
        } else if ((ledkey_buttons & 8) == 8) {
            mode = MODE_TONE;
            disp("TOnE", 0);
            ledkey_leds = 8;
        }
    }

    //display
    if ((now - last_disp) > DELAY_DISP) {
        last_disp = now;

        if (playing_num > 0) {
            digitalWrite(led_pin, HIGH);
            disp(pad4(playing[playing_num]), 4);
            ledkey_display[4] = 'n';
        } else {
            digitalWrite(led_pin, LOW);
        }

        ledkey.setDisplayToString(ledkey_display);
        ledkey.setLEDs(ledkey_leds);
    }

    //debug
    if (DEBUG && ((now - last_debug) > DELAY_DEBUG)) {
        last_debug = now;

        Serial.print("Display: ");
        for (int c = 0; c < 8; c++) {
            Serial.print(ledkey_display[c]);
        }
        Serial.println();

        Serial.print("Buttons: ");
        Serial.println(ledkey_buttons);

        Serial.print("LEDs: ");
        Serial.println(ledkey_leds);
    }
}


void keypad_event(KeypadEvent key){
    switch (keypad.getState()) {
        case PRESSED:
            midi_note(MIDI_NOTE_ON, key + base_note, 127);
            playing_num ++;
            if (key + base_note < 128) {
                playing[playing_num] = key + base_note;
            } else {
                playing[playing_num] = 127;
            }

            break;
        case RELEASED:
            midi_note(MIDI_NOTE_OFF, key + base_note, 127);
            playing_num --;
            break;
        case HOLD:
            break;
        case IDLE:
            break;
    }
}



void midi_note(int command, int note, int velocity) {
    if (note > 127) {
        note = 127;
    }
    Serial.write(command); //send note on or note off command 
    Serial.write(note); //send pitch data
    Serial.write(velocity); //send velocity data
}

void midi_cc(byte cc, byte value) {
    if (cc > 127) {
        cc = 127;
    }
    if (value > 127) {
        value = 127;
    }
    Serial.write(MIDI_CC);
    Serial.write(cc);
    Serial.write(value);
}

String pad4(int number) {
    String value = String(number);
    if (value.length() > 4) {
        value = value.substring(0, 4);
    }
    while (value.length() < 4) {
        value = " " + value;
    }
    return value;
}

void disp(String value, byte offset) {
    for (byte i = 0; i < 4; i++) {
        ledkey_display[i + offset] = value.charAt(i);
    }
}
