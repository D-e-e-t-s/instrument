#include <Arduino.h>
#include "FS.h"
#include "USB.h"
#include "USBMIDI.h"
#include "esp32-hal-touch.h"


USBMIDI MIDI;


const int keyPins[] = {1, 2, 3, 4, 5, 6, 7}; 
const int NUM_KEYS = sizeof(keyPins) / sizeof(keyPins[0]);

//Merry ground of life (has all the main notes but check the music sheet to play)
const int noteMapping[] = {
  60, // Key 1 = C4  
  62, // Key 2 = D4  
  63, // Key 3 = Eb4 
  66, // Key 4 = F#4  
  67, // Key 5 = G4  
  69, // Key 6 = A4 
  70  // Key 7 = Bb4  
};

// The ESP32-S3-Zero schematic connects TOUCH1-TOUCH7 to GPIO1-GPIO7.
bool lastKeyState[NUM_KEYS] = {false};
bool lastRawKeyState[NUM_KEYS] = {false};
unsigned long lastDebounceTime[NUM_KEYS] = {0};
uint32_t touchBaseline[NUM_KEYS] = {0};
const unsigned long DEBOUNCE_DELAY = 25;       // 25ms bounce filter threshold

void setup() {
  Serial.begin(115200);

  MIDI.begin();
  USB.begin();

  Serial.println("Calibrating touch pads; keep them untouched.");
  for (int i = 0; i < NUM_KEYS; i++) {
    uint64_t total = 0;
    for (int sample = 0; sample < 32; sample++) {
      total += touchRead(keyPins[i]);
      delay(5);
    }
    touchBaseline[i] = total / 32;
    Serial.printf("Touch pad %d baseline: %lu\n", i + 1,
                  static_cast<unsigned long>(touchBaseline[i]));
  }
}

void loop() {
  unsigned long currentTime = millis();

  for (int i = 0; i < NUM_KEYS; i++) {
    uint32_t reading = touchRead(keyPins[i]);
    uint32_t pressDelta = touchBaseline[i] / 5;
    uint32_t releaseDelta = touchBaseline[i] / 10;
    if (pressDelta < 20) {
      pressDelta = 20;
    }
    if (releaseDelta < 10) {
      releaseDelta = 10;
    }
    bool rawReading = lastKeyState[i]
                          ? reading > touchBaseline[i] + releaseDelta
                          : reading > touchBaseline[i] + pressDelta;

    if (rawReading != lastRawKeyState[i]) {
      lastRawKeyState[i] = rawReading;
      lastDebounceTime[i] = currentTime;
    }

    if (rawReading != lastKeyState[i] &&
        (currentTime - lastDebounceTime[i]) >= DEBOUNCE_DELAY) {
      int note = noteMapping[i];

      if (rawReading) {
        MIDI.noteOn(note, 127, 1);
        Serial.printf("Key %d Pressed -> MIDI Note: %d\n", i + 1, note);
      } else {
        MIDI.noteOff(note, 0, 1);
        Serial.printf("Key %d Released -> MIDI Note: %d\n", i + 1, note);
      }

      lastKeyState[i] = rawReading;
    }
  }

  delay(4);
}