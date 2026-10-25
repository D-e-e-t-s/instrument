#include <Arduino.h>
#include "FS.h"
#include "USB.h"
#include "USBMIDI.h"
#include "esp32-hal-touch.h"
#include <Adafruit_NeoPixel.h>


USBMIDI MIDI;

const int LED_DATA_PIN = 8; // Connect the first LED's DIN here.
const int NUM_LEDS = 3;
Adafruit_NeoPixel leds(NUM_LEDS, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);

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

const uint32_t keyColors[NUM_KEYS] = {
  0xFF2400, // Red
  0xFF8000, // Orange
  0xFFD600, // Yellow
  0x00FF00, // Green
  0x6A1B9A, // Violet
  0x32527B, // Blue
  0xC0C0C0  // Grey
};

bool lastKeyState[NUM_KEYS] = {false};
bool lastRawKeyState[NUM_KEYS] = {false};
unsigned long lastDebounceTime[NUM_KEYS] = {0};
uint32_t touchBaseline[NUM_KEYS] = {0};
const unsigned long DEBOUNCE_DELAY = 25;      
int activeLedKey = -1;
int displayedLedKey = -2;

void updateLedFeedback() {
  if (activeLedKey == displayedLedKey) {
    return;
  }

  uint32_t color = activeLedKey >= 0 ? keyColors[activeLedKey] : 0;
  for (int i = 0; i < NUM_LEDS; i++) {
    leds.setPixelColor(i, color);
  }
  leds.show();
  displayedLedKey = activeLedKey;
}


void setup() {
  Serial.begin(115200);

  leds.begin();
  leds.setBrightness(40);
  leds.clear();
  leds.show();

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
        activeLedKey = i;
        Serial.printf("Key %d Pressed -> MIDI Note: %d\n", i + 1, note);
      } else {
        MIDI.noteOff(note, 0, 1);
        if (activeLedKey == i) {
          activeLedKey = -1;
          for (int key = 0; key < NUM_KEYS; key++) {
            if (lastKeyState[key] && key != i) {
              activeLedKey = key;
              break;
            }
          }
        }
        Serial.printf("Key %d Released -> MIDI Note: %d\n", i + 1, note);
      }

      lastKeyState[i] = rawReading;
    }
  }

  updateLedFeedback();
  delay(4);
}