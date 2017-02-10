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
byte base_note = 0; //base note for pad mode
byte playing_num = 0; //number of playing notes in pad mode
byte playing[16]; // currently playing notes in pad mode
byte channel = 0; //currently selected channel
byte channel_notes[8]; // selected notes for channels in step mode


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
    enable_mode(MODE_PADS);

    #ifdef DEBUG
    //console/debug
    Serial.begin(9600);
    #else
    //midi
    Serial.begin(31250);
    #endif
}


void loop() {
    now = millis();

    //keypad
    keypad.getKey();

    if ((now - last_ctrl) > DELAY_CTRL) {
        last_ctrl = now;

        //pots
        if (abs(pot1.value()-pot1_last) >= TOLERANCE_POT) {
            pot1_last = pot1.last_value();
            update_pot1(pot1.last_value(), pot1.mapped_value(0, 127));
        }

        if (abs(pot2.value()-pot2_last) >= TOLERANCE_POT) {
            pot2_last = pot2.last_value();
            update_pot2(pot2.last_value(), pot2.mapped_value(0, 127));
        }

        //ledkey buttons
        ledkey_buttons = ledkey.getButtons();
        if ((ledkey_buttons & 1) == 1) {
            enable_mode(MODE_PADS);
        } else if ((ledkey_buttons & 2) == 2) {
            enable_mode(MODE_STEP);
        } else if ((ledkey_buttons & 4) == 4) {
            enable_mode(MODE_KNOB);
        } else if ((ledkey_buttons & 8) == 8) {
            enable_mode(MODE_TONE);
        }
    }

    //display
    if ((now - last_disp) > DELAY_DISP) {
        last_disp = now;

        if (playing_num > 0) {
            digitalWrite(led_pin, HIGH);
            disp(midi_display('n', playing[playing_num]), 4);
        } else {
            digitalWrite(led_pin, LOW);
        }

        ledkey.setDisplayToString(ledkey_display);
        ledkey.setLEDs(ledkey_leds);
    }

    #ifdef DEBUG
    if ((now - last_debug) > DELAY_DEBUG) {
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
    #endif
}


void keypad_event(KeypadEvent key){
    switch (keypad.getState()) {
        case PRESSED:
            press_button(key);
            break;
        case RELEASED:
            release_button(key);
            break;
        case HOLD:
        case IDLE:
            break;
    }
}


void enable_mode(Mode new_mode) {
    if (playing_num > 0) {
        return;
    }

    mode = new_mode;

    switch (new_mode) {
        case MODE_PADS:
            disp("PAdS", 0);
            ledkey_leds = 1;
            break;
        case MODE_STEP:
            disp("StEP", 0);
            ledkey_leds = 2;
            break;
        case MODE_KNOB:
            disp("CtrL", 0);
            ledkey_leds = 4;
            break;
        case MODE_TONE:
            disp("TOnE", 0);
            ledkey_leds = 8;
            break;
    }
}


//mode dependent control actions

void update_pot1(int value, int mapped_value) {
    switch (mode) {
        case MODE_PADS:
            base_note = mapped_value;
            disp(midi_display('B', mapped_value), 4);
            break;
        case MODE_STEP:
            channel_notes[channel] = mapped_value;
            disp(midi_display('n', mapped_value), 4);
            break;
        case MODE_KNOB:
            break;
        case MODE_TONE:
            break;
    }
}


void update_pot2(int value, int mapped_value) {
    switch (mode) {
        case MODE_PADS:
            disp(midi_display('V', mapped_value), 4);
            break;
        case MODE_STEP:
            channel = (int)round(1.0 * mapped_value / 127 * 7); //map midi value to channel
            disp(midi_display('C', channel + 1), 4);
            break;
        case MODE_KNOB:
            disp(midi_display('K', mapped_value), 4);
            //midi_cc(73, analog2midi(mapped_value));
            break;
        case MODE_TONE:
            break;
    }
}


void press_button(KeypadEvent key) {
    switch (mode) {
        case MODE_PADS:
            playing_num++;
            playing[playing_num] = midi_limit(key + base_note);
            midi_note_on(playing[playing_num], 127);
            break;
        case MODE_STEP:
            break;
        case MODE_KNOB:
            break;
        case MODE_TONE:
            break;
    }
}


void release_button(KeypadEvent key) {
    switch (mode) {
        case MODE_PADS:
            midi_note_off(midi_limit(key + base_note));
            playing_num--;
            break;
        case MODE_STEP:
            break;
        case MODE_KNOB:
            break;
        case MODE_TONE:
            break;
    }
}


//display

void disp(String value, byte offset) {
    for (byte i = 0; i < 4; i++) {
        ledkey_display[i + offset] = value.charAt(i);
    }
}


//helpers, utilities

//limits the value to the range 0 <= value <= 127
int midi_limit(int value) {
    return (value > 127) ? 127 : (value < 0 ? 0 : value);
}


//plays the specified note at the specified velocity
void midi_note_on(int note, int velocity) {
    _midi_note(MIDI_NOTE_ON, note, velocity);
}


//stops playing the specified note
void midi_note_off(int note) {
    _midi_note(MIDI_NOTE_OFF, note, 0);
}


//sends the midi command to play or stop a note
void _midi_note(int command, int note, int velocity) {
    Serial.write(command); //send note on or note off command
    Serial.write(midi_limit(note)); //send pitch data
    Serial.write(midi_limit(velocity)); //send velocity data
}


//sends the midi command to set a cc value
void midi_cc(byte cc, byte value) {
    Serial.write(MIDI_CC);
    Serial.write(midi_limit(cc));
    Serial.write(midi_limit(value));
}


//formats a midi value for displaying on a 4 place display
String midi_display(char label, int number) {
    String value = String(number);
    if (value.length() > 3) {
        value = value.substring(0, 3);
    }
    while (value.length() < 3) {
        value = ' ' + value;
    }
    return label + value;
}

