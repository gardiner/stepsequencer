/*
 * Midi pad and step sequencer.
 */
#include <Keypad.h> //keypad
#include <LedControl.h> //led matrix library
#include <TM1638.h> //led&key
#include <MIDI.h> //MIDI

#include "utils.h"


#define KEYPAD_ROWS 4
#define KEYPAD_COLS 4

#define BUTTON_PADS 0
#define BUTTON_STEP 1
#define BUTTON_KNOB 2
#define BUTTON_TONE 3
#define BUTTON_REWIND 4
#define BUTTON_PLAY 5
#define BUTTON_RECORD 6
#define BUTTON_STOP 7

#define DELAY_CTRL 20
#define DELAY_DEBUG 2000
#define PREVIEW_LENGTH 500
#define CHANNELLINE_LENGTH 500

#define TOLERANCE_POT 8

#define NO_NOTE 128

//comment the following line to disable debug mode
//#define DEBUG true

//MIDI
MIDI_CREATE_DEFAULT_INSTANCE();

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
byte led_pin = 13;

//time
unsigned long now = 0;
unsigned long last_ctrl = 0;
unsigned long last_debug = 0;
unsigned long last_preview = 0;
unsigned long last_channelline = 0;
unsigned long last_step = 0;

//status
byte base_note = 35; //base note for pad mode
byte playing[16] = {NO_NOTE}; // currently playing notes in pad mode
byte channel = 0; //currently selected channel
byte channel_notes[8] = {35, 36, 38, 40, 42, 44, 46, 55}; // selected notes for channels in step mode
byte preview_note = 0; //currently previewed note
byte channelline = 0; //currently displayed channel line
word steps[8] = {0, 33314, 2056, 0, 43690, 0, 0, 0}; //default step pattern
byte stepping = 0; //current stepping state - 0 stopped, 1 playing
byte step = 0; //current stepping position
unsigned int stepdelay = 0; //current stepping delay
boolean note_ons[128] = {false}; //currently sent note_ons


void setup() {
    //keypad
    keypad.addEventListener(keypad_event); // Add an event listener for this keypad

    //displays
    ledmatrix.shutdown(0, false);
    ledmatrix.setIntensity(0, 0);

    ledmatrix.shutdown(1, false);
    ledmatrix.setIntensity(1, 0);

    //pots
    pinMode(pot1_pin, INPUT);
    pinMode(pot2_pin, INPUT);

    //led
    pinMode(led_pin, OUTPUT);

    //init mode
    enable_mode(MODE_STEP);

    stepdelay = bpm2stepdelay(120);

    #ifdef DEBUG
    //console/debug
    Serial.begin(9600);
    #else
    //midi
    MIDI.begin(1);
    #endif
}


void loop() {
    now = millis();

    //MIDI
    MIDI.read();

    //keypad
    keypad.getKeys();

    if ((now - last_ctrl) > DELAY_CTRL) {
        last_ctrl = now;

        //ledkey buttons
        ledkey_buttons = ledkey.getButtons();
        if (is_pressed(BUTTON_PADS)) {
            enable_mode(MODE_PADS);
        } else if (is_pressed(BUTTON_STEP)) {
            enable_mode(MODE_STEP);
        } else if (is_pressed(BUTTON_KNOB)) {
            enable_mode(MODE_KNOB);
        } else if (is_pressed(BUTTON_TONE)) {
            enable_mode(MODE_TONE);
        } else if (is_pressed(BUTTON_REWIND)) {
            hide_stepline(step);
            step = 0;
            if (stepping == 1) {
                play_step(step);
                show_stepline(step);
                last_step = now;
            }
        } else if (is_pressed(BUTTON_PLAY)) {
            if (stepping == 0) {
                play_step(step);
                show_stepline(step);
                stepping = 1;
                last_step = now;
            }
        } else if (is_pressed(BUTTON_STOP)) {
            hide_stepline(step);
            all_stop();
        }

        //pots
        //checking pots after ledkey buttons because they can be used as modifiers
        if (abs(pot1.value()-pot1_last) >= TOLERANCE_POT) {
            pot1_last = pot1.last_value();
            update_pot1(pot1.last_value(), pot1.mapped_value(0, 127));
        }

        if (abs(pot2.value()-pot2_last) >= TOLERANCE_POT) {
            pot2_last = pot2.last_value();
            update_pot2(pot2.last_value(), pot2.mapped_value(0, 127));
        }
    }

    //stop note preview
    if (last_preview != 0 && now - last_preview > PREVIEW_LENGTH) {
        stop_preview();
    }

    //stepping
    if (stepping == 1 && now - last_step > stepdelay) {
        hide_stepline(step);
        last_step = now;
        step = (step + 1) % 16;
        play_step(step);
        show_stepline(step);
    }

    //display
    if (last_channelline != 0 && now - last_channelline > CHANNELLINE_LENGTH) {
        stop_channelline(channel);
    }


    #ifdef DEBUG
    if ((now - last_debug) > DELAY_DEBUG) {
        last_debug = now;

        Serial.print("Display: ");
        for (byte c = 0; c < 8; c++) {
            Serial.print(ledkey_display[c]);
        }
        Serial.println();

        Serial.print("Buttons: ");
        Serial.println(ledkey_buttons);

        Serial.print("LEDs: ");
        Serial.println(ledkey_leds);

        Serial.print("base_note: ");
        Serial.println(base_note);
        Serial.print("channel: ");
        Serial.println(channel);
        Serial.print("preview_note: ");
        Serial.println(preview_note);
        Serial.print("channelline: ");
        Serial.println(channelline);
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
    if (mode == new_mode) {
        return;
    }

    mode = new_mode;

    show_all_steps();

    switch (new_mode) {
        case MODE_PADS:
            disp("PAdS", 0);
            leds(1);
            break;
        case MODE_STEP:
            disp("StEP", 0);
            leds(2);
            break;
        case MODE_KNOB:
            disp("CtrL", 0);
            leds(4);
            break;
        case MODE_TONE:
            disp("TOnE", 0);
            leds(8);
            break;
    }
}


//mode dependent control actions

void update_pot1(int value, int mapped_value) {
    switch (mode) {
        case MODE_PADS:
            //changes the pad base note
            if (mapped_value != base_note) {
                base_note = mapped_value;
                start_preview(base_note);
                disp(midi_display('B', mapped_value), 4);
            }
            break;
        case MODE_STEP:
            //changes the note of the current channel
            if (mapped_value != channel_notes[channel]) {
                channel_notes[channel] = mapped_value;
                start_preview(channel_notes[channel]);
                disp(midi_display('n', mapped_value), 4);
            }
            break;
        case MODE_KNOB:
            break;
        case MODE_TONE:
            //mod wheel
            disp(midi_display('t', mapped_value), 4);
            //midi_cc(73, analog2midi(mapped_value));
            break;
    }
}


void update_pot2(int value, int mapped_value) {
    switch (mode) {
        case MODE_PADS:
            //disp(midi_display(' ', mapped_value), 4);
            break;
        case MODE_STEP:
            {
                if (is_pressed(BUTTON_STEP)) {
                    int bpm = (int)round(1.0 * value / 1024 * 300);
                    bpm = (bpm > 300) ? 300 : ((bpm < 6) ? 6 : bpm);
                    stepdelay = bpm2stepdelay(bpm);
                    disp(midi_display('S', bpm), 4);
                } else {
                    //changes the current channel
                    byte mapped = (int)round(1.0 * mapped_value / 127 * 7); //map midi value to channel
                    if (mapped != channel) {
                        channel = mapped;
                        start_channelline(channel);
                        start_preview(channel_notes[channel]);
                        disp(midi_display('C', channel + 1), 4);
                    }
                }
            }
            break;
        case MODE_KNOB:
            break;
        case MODE_TONE:
            //pitch bend
            disp(midi_display('P', mapped_value), 4);
            //midi_cc(73, analog2midi(mapped_value));
            break;
    }
}


void press_button(KeypadEvent key) {
    switch (mode) {
        case MODE_PADS:
            stop_note(key);
            play_note(key);
            break;
        case MODE_STEP:
            midi_note_on(channel_notes[channel], 127);
            if (is_step(channel, key)) {
                unset_step(channel, key);
            } else {
                set_step(channel, key);
            }
            break;
        case MODE_KNOB:
            break;
        case MODE_TONE:
            break;
    }
}


void release_button(KeypadEvent key) {
    //stopping to play a note should happen no matter which mode we're in
    stop_note(key);

    switch (mode) {
        case MODE_PADS:
            break;
        case MODE_STEP:
            midi_note_off(channel_notes[channel]);
            break;
        case MODE_KNOB:
            break;
        case MODE_TONE:
            break;
    }
}


//note preview

void start_preview(byte note) {
    stop_preview();
    preview_note = note;
    last_preview = now;
    midi_note_on(preview_note, 127);
}


void stop_preview() {
    last_preview = 0;
    midi_note_off(preview_note);
}


//pad mode

void play_note(char key) {
    byte code = (byte)key;
    playing[code] = midi_limit(key + base_note);
    midi_note_on(playing[code], 127);
    disp(midi_display('n', playing[code]), 4);
    digitalWrite(led_pin, HIGH);
}


void stop_note(char key) {
    byte code = (byte)key;
    if (playing[code] != NO_NOTE) {
        midi_note_off(playing[code]);
        playing[code] = NO_NOTE;
        digitalWrite(led_pin, LOW);
    }
}


//step sequencer

void set_step(byte channel, byte step) {
    steps[channel] |= (1 << (15-step));
    show_step(channel, step);
}


void unset_step(byte channel, byte step) {
    steps[channel] &= (65535 - (1 << (15-step)));
    show_step(channel, step);
}


boolean is_step(byte channel, byte step) {
    return (steps[channel] & (1 << (15-step))) == 0 ? false : true;
}


//display

void start_channelline(byte channel) {
    stop_channelline(channelline);
    last_channelline = now;
    channelline = channel;
    ledmatrix.setColumn(0, 7 - channelline, 255);
    ledmatrix.setColumn(1, 7 - channelline, 255);
}


void stop_channelline(byte channel) {
    byte step;
    if (last_channelline != 0) {
        last_channelline = 0;
        for (step=0; step<16; step++) {
            show_step(channel, step);
        }
    }
}


void show_stepline(byte step) {
    byte channel;
    for (channel=0; channel<8; channel++) {
        ledmatrix.setLed(address(step), row(step), column(channel), true);
    }
}


void hide_stepline(byte step) {
    byte channel;
    for (channel=0; channel<8; channel++) {
        show_step(channel, step);
        //reenable channelline if necessary
        if (last_channelline != 0 && channel == channelline) {
            ledmatrix.setLed(address(step), row(step), column(channel), true);
        }
    }
}


void disp(String value, byte offset) {
    for (byte i = 0; i < 4; i++) {
        ledkey_display[i + offset] = value.charAt(i);
    }
    ledkey.setDisplayToString(ledkey_display);
}


void leds(word value) {
    ledkey_leds = value;
    ledkey.setLEDs(ledkey_leds);
}


void show_all_steps() {
    byte channel, step;
    for (channel=0; channel<8; channel++) {
        for (step=0; step<16; step++) {
            show_step(channel, step);
        }
    }
}

void show_step(byte channel, byte step) {
    ledmatrix.setLed(address(step), row(step), column(channel), is_step(channel, step));
}

byte address(byte step) {
    return (step < 8) ? 0 : 1;
}

byte row(byte step) {
    return (step < 8) ? 7 - step : 15 - step;
}

byte column(byte channel) {
    return 7 - channel;
}


//helpers, utilities

//checks if the specified ledkey button is pressed. button indices are zero-based.
bool is_pressed(byte index) {
    return (ledkey_buttons & (1 << index)) > 0;
}


//calculates delay between steps depending on bpm
int bpm2stepdelay(int bpm) {
    //a minute has 60000 millis, each beat has 4 steps, so:
    return 60000 / (bpm * 4);
}


void play_step(byte step) {
    byte channel;
    for (channel=0; channel<8; channel++) {
        if (is_step(channel, step)) {
            midi_note_off(channel_notes[channel]);
            midi_note_on(channel_notes[channel], 127);
        }
    }
}


//limits the value to the range 0 <= value <= 127
byte midi_limit(byte value) {
    return (value > 127) ? 127 : (value < 0 ? 0 : value);
}


//plays the specified note at the specified velocity
void midi_note_on(byte note, byte velocity) {
    note_ons[note] = true;
    MIDI.sendNoteOn(note, velocity, 1);
}


//stops playing the specified note
void midi_note_off(byte note) {
    if (note_ons[note]) {
        MIDI.sendNoteOn(note, 0, 1);
        note_ons[note] = false;
    }
}


//stops all notes
void all_stop() {
    byte i;

    stepping = 0;

    for (i=0; i<128; i++) {
        midi_note_off(i);
    }
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

