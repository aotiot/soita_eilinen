/*
 * ESP32-S2/S3/C6 + MAX98357A + Mozzi - Beatles "Yesterday"
 * 
 * Kytkennät ESP32-S2/S3/C6:
 * MAX98357A -> ESP32
 * LRC (LRCLK) -> GPIO 4
 * BCLK       -> GPIO 5
 * DIN        -> GPIO 6
 * GND        -> GND
 * VIN        -> 5V (tai 3.3V)
 * 
 * Huom: ESP32-S2:ssa on vain yksi I2S-periferia
 *       ESP32-S3:ssa on kaksi I2S-periferiaa
 *       ESP32-C6:ssa on yksi I2S-periferia
 */

#include <MozziGuts.h>
#include <Oscil.h>
#include <tables/sin2048_int8.h>
#include <tables/saw2048_int8.h>
#include <mozzi_midi.h>
#include <Envelope.h>
#include <ADSR.h>

// Audio konfiguraatio
#define AUDIO_MODE HIFI
#define AUDIO_RATE 16384

// I2S pin määrittelyt ESP32-S2/S3/C6:lle
#define I2S_BCLK_PIN 5
#define I2S_LRCLK_PIN 4
#define I2S_DOUT_PIN 6

// Oskillaattorit
Oscil<SIN2048_NUM_CELLS, AUDIO_RATE> aOscil(SIN2048_DATA);
Oscil<SAW2048_NUM_CELLS, AUDIO_RATE> aOscil2(SAW2048_DATA);

// ADSR envelope
ADSR <CONTROL_RATE, AUDIO_RATE> envelope;

// Nuotit (MIDI-numeroina)
#define C4  60
#define Cs4 61
#define D4  62
#define Ds4 63
#define E4  64
#define F4  65
#define Fs4 66
#define G4  67
#define Gs4 68
#define A4  69
#define As4 70
#define B4  71
#define C5  72
#define Cs5 73
#define D5  74
#define Ds5 75
#define E5  76
#define F5  77
#define Fs5 78
#define G5  79
#define REST 0

// "Yesterday" melodia
struct Note {
  byte pitch;
  unsigned int duration; // millisekunteina
};

Note melody[] = {
  // "Yes-ter-day"
  {F4, 300},
  {E4, 300},
  {D4, 600},
  
  // "all my trou-bles seemed so"
  {D4, 200},
  {E4, 200},
  {F4, 200},
  {A4, 300},
  {G4, 300},
  {F4, 600},
  
  // "far a-way"
  {E4, 300},
  {D4, 300},
  {C4, 600},
  
  // "Now it looks as though they're"
  {C4, 200},
  {D4, 200},
  {E4, 200},
  {F4, 300},
  {E4, 300},
  {D4, 600},
  
  // "here to stay"
  {D4, 300},
  {E4, 300},
  {F4, 600},
  
  // "Oh I be-lieve in"
  {REST, 200},
  {A4, 300},
  {G4, 300},
  {F4, 400},
  
  // "yes-ter-day"
  {F4, 300},
  {E4, 300},
  {D4, 800},
  
  // Toinen säkeistö
  // "Sud-den-ly"
  {REST, 400},
  {F4, 300},
  {E4, 300},
  {D4, 600},
  
  // "I'm not half the man I"
  {D4, 200},
  {E4, 200},
  {F4, 200},
  {A4, 300},
  {G4, 300},
  {F4, 600},
  
  // "used to be"
  {E4, 300},
  {D4, 300},
  {C4, 600},
  
  // "There's a sha-dow han-ging"
  {C4, 200},
  {D4, 200},
  {E4, 200},
  {F4, 300},
  {E4, 300},
  {D4, 600},
  
  // "o-ver me"
  {D4, 300},
  {E4, 300},
  {F4, 600},
  
  // "Oh yes-ter-day came"
  {REST, 200},
  {A4, 300},
  {G4, 300},
  {F4, 400},
  
  // "sud-den-ly"
  {F4, 300},
  {E4, 300},
  {D4, 800},
  
  // Kertosäe
  // "Why she had to go I don't"
  {REST, 400},
  {C5, 300},
  {A4, 300},
  {G4, 300},
  {F4, 300},
  {E4, 600},
  
  // "know, she would-n't say"
  {D4, 300},
  {E4, 300},
  {F4, 300},
  {G4, 300},
  {A4, 800},
  
  // "I said some-thing wrong now I"
  {REST, 200},
  {C5, 300},
  {A4, 300},
  {G4, 300},
  {F4, 300},
  {E4, 600},
  
  // "long for yes-ter-day"
  {D4, 300},
  {E4, 300},
  {F4, 300},
  {E4, 300},
  {D4, 1200},
  
  // Lopetus
  {REST, 1000}
};

const int melodyLength = sizeof(melody) / sizeof(melody[0]);
int currentNote = 0;
unsigned long noteStartTime = 0;
bool notePlaying = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Tulosta laitteen tyyppi
  Serial.println("\n=================================");
  #if defined(CONFIG_IDF_TARGET_ESP32S2)
    Serial.println("Laite: ESP32-S2");
  #elif defined(CONFIG_IDF_TARGET_ESP32S3)
    Serial.println("Laite: ESP32-S3");
  #elif defined(CONFIG_IDF_TARGET_ESP32C6)
    Serial.println("Laite: ESP32-C6");
  #elif defined(CONFIG_IDF_TARGET_ESP32C3)
    Serial.println("Laite: ESP32-C3");
  #else
    Serial.println("Laite: ESP32 (standardi)");
  #endif
  Serial.println("=================================");
  
  Serial.println("\nBeatles - Yesterday");
  Serial.println("\nKytkennät MAX98357A:");
  Serial.println("LRC (LRCLK) -> GPIO 4");
  Serial.println("BCLK        -> GPIO 5");
  Serial.println("DIN         -> GPIO 6");
  Serial.println("GND         -> GND");
  Serial.println("VIN         -> 5V tai 3.3V");
  Serial.println("=================================\n");
  
  // Käynnistä Mozzi
  startMozzi(CONTROL_RATE);
  
  // ADSR asetukset (attack, decay, sustain level, release millisekunteina)
  envelope.setADLevels(255, 200);
  envelope.setTimes(20, 50, 300, 100);
  
  Serial.println("Soitto alkaa...\n");
  
  // Aloita ensimmäinen nuotti
  playNote(0);
}

void playNote(int noteIndex) {
  if (noteIndex >= melodyLength) {
    // Melodia päättynyt, aloita alusta
    currentNote = 0;
    noteIndex = 0;
    Serial.println("\n=== Melodia päättyi, aloitetaan alusta ===\n");
    delay(2000);
  }
  
  Note note = melody[noteIndex];
  
  if (note.pitch == REST) {
    // Tauko
    notePlaying = false;
    Serial.print("REST ");
  } else {
    // Soita nuotti
    float freq = mtof(note.pitch);
    aOscil.setFreq(freq);
    aOscil2.setFreq(freq * 1.01); // Hieman detune stereoefektiä varten
    
    envelope.noteOn();
    notePlaying = true;
    
    // Tulosta nuotti
    Serial.print(getNoteNameFromMidi(note.pitch));
    Serial.print(" ");
  }
  
  Serial.print("(");
  Serial.print(note.duration);
  Serial.print("ms) ");
  
  if ((noteIndex + 1) % 8 == 0) {
    Serial.println(); // Rivinvaihto joka 8. nuotin jälkeen
  }
  
  noteStartTime = millis();
  currentNote = noteIndex;
}

String getNoteNameFromMidi(byte midiNote) {
  const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  int octave = (midiNote / 12) - 1;
  int noteIndex = midiNote % 12;
  return String(noteNames[noteIndex]) + String(octave);
}

void updateControl() {
  // Tarkista pitääkö siirtyä seuraavaan nuottiin
  unsigned long currentTime = millis();
  Note currentNoteData = melody[currentNote];
  
  if (currentTime - noteStartTime >= currentNoteData.duration) {
    if (notePlaying) {
      envelope.noteOff();
    }
    // Siirry seuraavaan nuottiin
    playNote(currentNote + 1);
  }
  
  envelope.update();
}

AudioOutput_t updateAudio() {
  if (notePlaying) {
    int sample1 = aOscil.next();
    int sample2 = aOscil2.next();
    
    // Miksaa kaksi oskillaattoria
    int mixed = (sample1 + sample2) >> 1;
    
    // Käytä envelopea
    int output = (mixed * (int)envelope.next()) >> 8;
    
    return MonoOutput::from8Bit(output);
  } else {
    return MonoOutput::from8Bit(0);
  }
}

void loop() {
  audioHook();
}
