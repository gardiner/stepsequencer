/*
 * Switches the LED on with button
 */
#include <Keypad.h>

#define ROWS 4
#define COLS 4

#define NOTE_ON 144 //144 = 10010000 in binary, note on command
#define NOTE_OFF 128 //128 = 10000000 in binary, note off command


char keys[ROWS][COLS] = {
    {'a','b','c','d'},
    {'e','f','g','h'},
    {'i','j','k','l'},
    {'m','n','o','p'},
};
byte rowPins[ROWS] = {8, 7, 6, 5};
byte colPins[COLS] = {9, 10, 11, 12}; 
Keypad kpd = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

int led = 13;


void setup() {
    pinMode(led, OUTPUT);
    kpd.addEventListener(keypad_event); // Add an event listener for this keypad
    //Serial.begin(9600); //console/debug
    Serial.begin(31250); //midi
}


void loop() {
    char key = kpd.getKey();
    if (key) {
        //Serial.println(key);
        if (key == 'a') {
            digitalWrite(led, HIGH);
        } else {
            digitalWrite(led, LOW);
        }
    }
}


void keypad_event(KeypadEvent key){
    switch (kpd.getState()) {
        case PRESSED:
            midi_send(NOTE_ON, key - 97 + 36, 127);
            break;
        case RELEASED:
            midi_send(NOTE_OFF, key - 97 + 36, 127);
            break;
        case HOLD:
            break;
    }
}



void midi_send(int command, int note, int velocity) {
    Serial.write(command); //send note on or note off command 
    Serial.write(note); //send pitch data
    Serial.write(velocity); //send velocity data
}
