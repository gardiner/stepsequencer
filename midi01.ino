/*
 * Midi pad and step sequencer.
 */
#include <Keypad.h>
//#include <LedControl.h> //led matrix library
#include <TM1638.h> //led&key


#define KEYPAD_ROWS 4
#define KEYPAD_COLS 4

#define MIDI_NOTE_ON 144 //144 = 10010000 in binary, note on command on channel 0
#define MIDI_NOTE_OFF 128 //128 = 10000000 in binary, note off command on channel 0
#define MIDI_CC 176 //176 = midi cc on channel 0

#define DELAY_CTRL 100
#define TOLERANCE_POT 6


enum Mode {
    MODE_PADS, //pad mode - keypad plays notes
    MODE_STEP, //step sequencer mode - keypad toggles steps
    MODE_KNOB, //knob mode - keypad toggles control channel for knob
};


byte keypad_row_pins[KEYPAD_ROWS] = {8, 7, 6, 5};
byte keypad_col_pins[KEYPAD_COLS] = {9, 10, 11, 12};
char keypad_keys[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'m','n','o','p'},
    {'i','j','k','l'},
    {'e','f','g','h'},
    {'a','b','c','d'},
};

Keypad keypad = Keypad(makeKeymap(keypad_keys), keypad_row_pins, keypad_col_pins, KEYPAD_ROWS, KEYPAD_COLS);


byte ledkey_stb_pin = 4;
byte ledkey_clk_pin = 3;
byte ledkey_dio_pin = 2;
char ledkey_display[8] = {0, 0, 0, 0, 0, 0, 0, 0};

TM1638 ledkey = TM1638(ledkey_dio_pin, ledkey_clk_pin, ledkey_stb_pin, true, 0);
//LedControl ledmatrix = LedControl(ledmatrix_din_pin, ledmatrix_clk_pin, ledmatrix_cs_pin, 2);


byte pot1_pin = A0;
byte pot2_pin = A1;
int pot1 = 0;
int pot1_last = 0;
int pot2 = 0;
int pot2_last = 0;


int led_pin = 13;


unsigned long now = 0;
unsigned long last_ctrl = 0;


void setup() {
    //keypad
    keypad.addEventListener(keypad_event); // Add an event listener for this keypad

    //ledkey

    //pots
    pinMode(pot1_pin, INPUT);
    pinMode(pot2_pin, INPUT);

    //led
    pinMode(led_pin, OUTPUT);


    //Serial.begin(9600); //console/debug
    Serial.begin(31250); //midi
}


void loop() {
    now = millis();

    //keypad
    char key = keypad.getKey();

    //pots
    if (now - last_ctrl > DELAY_CTRL) {
        last_ctrl = now;

        pot1 = analogRead(pot1_pin);
        if (abs(pot1-pot1_last) >= TOLERANCE_POT) {
            pot1_last = pot1;
            disp(pad4(analog2midi(pot1)), 0);
        }

        pot2 = analogRead(pot2_pin);
        if (abs(pot2-pot2_last) >= TOLERANCE_POT) {
            pot2_last = pot2;
            disp(pad4(analog2midi(pot2)), 4);
            midi_cc(73, analog2midi(pot2));
        }
    }
}


void keypad_event(KeypadEvent key){
    switch (keypad.getState()) {
        case PRESSED:
            midi_note(MIDI_NOTE_ON, key - 97 + 36, 127);
            break;
        case RELEASED:
            midi_note(MIDI_NOTE_OFF, key - 97 + 36, 127);
            break;
        case HOLD:
            break;
        case IDLE:
            break;
    }
}



void midi_note(int command, int note, int velocity) {
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


int analog2midi(int value) {
    return (int)round(1.0 * value * 127 / 1024);
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
    ledkey.setDisplayToString(ledkey_display);
}
