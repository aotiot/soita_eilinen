/*
 * ============================================================================
 * Beatles "Yesterday" - ESP32 Audiosoitin
 * ============================================================================
 * 
 * KUVAUS:
 *   Soittaa Beatles-kappaleen "Yesterday" melodian käyttäen Mozzi-
 *   äänisyntetisaattorikirjastoa, ESP32-mikrokontrolleria ja MAX98357A
 *   I2S-audiovahvistinta.
 * 
 * TUETUT LAITTEET:
 *   - ESP32-S2 (1x I2S)
 *   - ESP32-S3 (2x I2S, suositeltu)
 *   - ESP32-C6 (1x I2S)
 *   - ESP32-C3 (1x I2S)
 * 
 * KYTKENNÄT:
 *   MAX98357A -> ESP32
 *   ─────────────────────
 *   LRC (LRCLK) -> GPIO 4
 *   BCLK        -> GPIO 5
 *   DIN         -> GPIO 6
 *   GND         -> GND
 *   VIN         -> 5V tai 3.3V
 * 
 * KAIUTIN:
 *   4-8 ohmin kaiutin MAX98357A:n + ja - liittimiin
 * 
 * TEKIJÄ: ESP32 + Mozzi + MAX98357A
 * MELODIA: The Beatles - "Yesterday" (Lennon/McCartney)
 * LISENSSI: MIT
 * 
 * ============================================================================
 */

// ============================================================================
// KIRJASTOT JA KONFIGURAATIO
// ============================================================================

#include <MozziGuts.h>           // Mozzi-syntetisaattorimoottorin ydin
#include <Oscil.h>               // Oskillaattoriluokka äänentuotantoon
#include <tables/sin2048_int8.h> // Sini-aaltomuoto (2048 näytettä)
#include <tables/saw2048_int8.h> // Saha-aaltomuoto (2048 näytettä)
#include <mozzi_midi.h>          // MIDI-nuottien käsittely
#include <Envelope.h>            // Envelope-generaattorien perusta
#include <ADSR.h>                // Attack-Decay-Sustain-Release envelope

// Audio-asetukset
#define AUDIO_MODE HIFI          // Korkealaatuinen audiotila
#define AUDIO_RATE 16384         // Näytteenottotaajuus: 16384 Hz

// I2S-pinnien määrittely ESP32-S2/S3/C6:lle
#define I2S_BCLK_PIN 5           // Bit Clock
#define I2S_LRCLK_PIN 4          // Left-Right Clock (Word Select)
#define I2S_DOUT_PIN 6           // Data Out

// ============================================================================
// AUDIO-OBJEKTIT
// ============================================================================

// Oskillaattori 1: Sini-aalto
// Käyttää 2048 näytteen sini-aaltomuotoa pehmeän äänen saamiseksi
Oscil<SIN2048_NUM_CELLS, AUDIO_RATE> aOscil(SIN2048_DATA);

// Oskillaattori 2: Saha-aalto
// Lisää kirkkautta ja täyteläisyyttä soundiin
Oscil<SAW2048_NUM_CELLS, AUDIO_RATE> aOscil2(SAW2048_DATA);

// ADSR-envelope nuottien dynamiikalle
// Attack-Decay-Sustain-Release verhokäyrä antaa nuoteille
// luonnollisen alku- ja loppukäyrän
ADSR <CONTROL_RATE, AUDIO_RATE> envelope;

// ============================================================================
// MIDI-NUOTTIMÄÄRITTELYT
// ============================================================================

// MIDI-numeroiden määrittely (keskimmäinen oktaavi)
#define C4  60  // C (do)
#define Cs4 61  // C# (cis)
#define D4  62  // D (re)
#define Ds4 63  // D# (dis)
#define E4  64  // E (mi)
#define F4  65  // F (fa)
#define Fs4 66  // F# (fis)
#define G4  67  // G (sol)
#define Gs4 68  // G# (gis)
#define A4  69  // A (la)
#define As4 70  // A# (ais)
#define B4  71  // B (ti)
#define C5  72  // C (ylempi oktaavi)
#define Cs5 73  // C#
#define D5  74  // D
#define Ds5 75  // D#
#define E5  76  // E
#define F5  77  // F
#define Fs5 78  // F#
#define G5  79  // G
#define REST 0  // Tauko (ei ääntä)

// ============================================================================
// MELODIADATA
// ============================================================================

/**
 * Note-rakenne:
 * Yksittäinen nuotti tai tauko melodiassa
 * 
 * @param pitch    MIDI-nuottinumero (0 = tauko, 60-79 = nuotit)
 * @param duration Nuotin kesto millisekunneissa
 */
struct Note {
  byte pitch;              // MIDI-nuottinumero
  unsigned int duration;   // Kesto millisekunneissa
};

/**
 * "Yesterday" melodian nuotit
 * 
 * Tempo: ~90 BPM
 * Rakenne: Intro + Verse 1 + Verse 2 + Chorus
 * 
 * Nuottien kestot:
 * - 200ms  = 32-osat
 * - 300ms  = 16-osat
 * - 400ms  = Pisteelliset 16-osat
 * - 600ms  = 8-osat
 * - 800ms  = Pisteelliset 8-osat
 * - 1200ms = 4-osat
 */
Note melody[] = {
  // ──────────────────────────────────────────────────────────
  // SÄKEISTÖ 1: "Yesterday, all my troubles seemed so far away"
  // ──────────────────────────────────────────────────────────
  
  // "Yes-ter-day"
  {F4, 300},   // Yes-
  {E4, 300},   // ter-
  {D4, 600},   // day
  
  // "all my trou-bles seemed so"
  {D4, 200},   // all
  {E4, 200},   // my
  {F4, 200},   // trou-
  {A4, 300},   // bles
  {G4, 300},   // seemed
  {F4, 600},   // so
  
  // "far a-way"
  {E4, 300},   // far
  {D4, 300},   // a-
  {C4, 600},   // way
  
  // "Now it looks as though they're"
  {C4, 200},   // Now
  {D4, 200},   // it
  {E4, 200},   // looks
  {F4, 300},   // as
  {E4, 300},   // though
  {D4, 600},   // they're
  
  // "here to stay"
  {D4, 300},   // here
  {E4, 300},   // to
  {F4, 600},   // stay
  
  // "Oh I be-lieve in"
  {REST, 200}, // (hengähdystauko)
  {A4, 300},   // Oh
  {G4, 300},   // I
  {F4, 400},   // be-lieve
  
  // "yes-ter-day"
  {F4, 300},   // yes-
  {E4, 300},   // ter-
  {D4, 800},   // day (pitkä nuotti)
  
  // ──────────────────────────────────────────────────────────
  // SÄKEISTÖ 2: "Suddenly, I'm not half the man I used to be"
  // ──────────────────────────────────────────────────────────
  
  // "Sud-den-ly"
  {REST, 400}, // (tauko säkeistöjen välissä)
  {F4, 300},   // Sud-
  {E4, 300},   // den-
  {D4, 600},   // ly
  
  // "I'm not half the man I"
  {D4, 200},   // I'm
  {E4, 200},   // not
  {F4, 200},   // half
  {A4, 300},   // the
  {G4, 300},   // man
  {F4, 600},   // I
  
  // "used to be"
  {E4, 300},   // used
  {D4, 300},   // to
  {C4, 600},   // be
  
  // "There's a sha-dow han-ging"
  {C4, 200},   // There's
  {D4, 200},   // a
  {E4, 200},   // sha-
  {F4, 300},   // dow
  {E4, 300},   // han-
  {D4, 600},   // ging
  
  // "o-ver me"
  {D4, 300},   // o-
  {E4, 300},   // ver
  {F4, 600},   // me
  
  // "Oh yes-ter-day came"
  {REST, 200}, // (hengähdystauko)
  {A4, 300},   // Oh
  {G4, 300},   // yes-
  {F4, 400},   // ter-day
  
  // "sud-den-ly"
  {F4, 300},   // sud-
  {E4, 300},   // den-
  {D4, 800},   // ly (pitkä nuotti)
  
  // ──────────────────────────────────────────────────────────
  // KERTOSÄE: "Why she had to go I don't know"
  // ──────────────────────────────────────────────────────────
  
  // "Why she had to go I don't"
  {REST, 400}, // (tauko ennen kertosäettä)
  {C5, 300},   // Why
  {A4, 300},   // she
  {G4, 300},   // had
  {F4, 300},   // to
  {E4, 600},   // go
  
  // "know, she would-n't say"
  {D4, 300},   // I
  {E4, 300},   // don't
  {F4, 300},   // know,
  {G4, 300},   // she
  {A4, 800},   // would-n't say
  
  // "I said some-thing wrong now I"
  {REST, 200}, // (hengähdystauko)
  {C5, 300},   // I
  {A4, 300},   // said
  {G4, 300},   // some-
  {F4, 300},   // thing
  {E4, 600},   // wrong
  
  // "long for yes-ter-day"
  {D4, 300},   // now
  {E4, 300},   // I
  {F4, 300},   // long
  {E4, 300},   // for
  {D4, 1200},  // yes-ter-day (pitkä loppunuotti)
  
  // Lopetus
  {REST, 1000} // (tauko ennen uudelleensoittoa)
};

// Melodian pituus (nuottien lukumäärä)
const int melodyLength = sizeof(melody) / sizeof(melody[0]);

// ============================================================================
// SOITTIMEN TILAMUUTTUJAT
// ============================================================================

int currentNote = 0;              // Nykyisen nuotin indeksi
unsigned long noteStartTime = 0;  // Nuotin aloitusaika (millisekunteina)
bool notePlaying = false;         // Soitetaanko nuottia juuri nyt?

// ============================================================================
// ALUSTUS
// ============================================================================

/**
 * setup() - Ohjelman alustus
 * 
 * Suoritetaan kerran ohjelman käynnistyessä.
 * Alustaa sarjaliikenneyhteyden, tunnistaa laitteen, käynnistää
 * Mozzi-audiosysteemin ja aloittaa melodian soiton.
 */
void setup() {
  // Käynnistä sarjaliikenneyhteys (115200 baudia)
  Serial.begin(115200);
  delay(1000); // Odota että sarjaliikenne ehtii käynnistyä
  
  // ──────────────────────────────────────────────────────────
  // Tunnista ESP32-laitteen tyyppi
  // ──────────────────────────────────────────────────────────
  Serial.println("\n=================================");
  
  #if defined(CONFIG_IDF_TARGET_ESP32S2)
    Serial.println("Laite: ESP32-S2");
    Serial.println("I2S-perifeeriät: 1");
  #elif defined(CONFIG_IDF_TARGET_ESP32S3)
    Serial.println("Laite: ESP32-S3");
    Serial.println("I2S-perifeeriät: 2 (suositeltu)");
  #elif defined(CONFIG_IDF_TARGET_ESP32C6)
    Serial.println("Laite: ESP32-C6");
    Serial.println("I2S-perifeeriät: 1");
  #elif defined(CONFIG_IDF_TARGET_ESP32C3)
    Serial.println("Laite: ESP32-C3");
    Serial.println("I2S-perifeeriät: 1");
  #else
    Serial.println("Laite: ESP32 (alkuperäinen)");
    Serial.println("Huom: Tämä koodi on optimoitu S2/S3/C6:lle");
  #endif
  
  Serial.println("=================================");
  
  // ──────────────────────────────────────────────────────────
  // Tulosta kytkentäohjeet
  // ──────────────────────────────────────────────────────────
  Serial.println("\nBeatles - Yesterday");
  Serial.println("\nKytkennät MAX98357A:");
  Serial.println("LRC (LRCLK) -> GPIO 4");
  Serial.println("BCLK        -> GPIO 5");
  Serial.println("DIN         -> GPIO 6");
  Serial.println("GND         -> GND");
  Serial.println("VIN         -> 5V tai 3.3V");
  Serial.println("=================================\n");
  
  // ──────────────────────────────────────────────────────────
  // Käynnistä Mozzi-audiosysteemi
  // ──────────────────────────────────────────────────────────
  startMozzi(CONTROL_RATE);
  
  // ──────────────────────────────────────────────────────────
  // Konfiguroi ADSR-envelope
  // ──────────────────────────────────────────────────────────
  // Aseta Attack ja Sustain -tasot (0-255)
  envelope.setADLevels(255, 200);  // Attack: täysi, Sustain: 78%
  
  // Aseta ajat (millisekunteina):
  // Attack:  20ms  - nopea nousu
  // Decay:   50ms  - nopea lasku sustain-tasolle
  // Sustain: 300ms - pitkä tasainen vaihe
  // Release: 100ms - keskipitkä häipyminen
  envelope.setTimes(20, 50, 300, 100);
  
  Serial.println("Soitto alkaa...\n");
  
  // ──────────────────────────────────────────────────────────
  // Aloita ensimmäinen nuotti
  // ──────────────────────────────────────────────────────────
  playNote(0);
}

// ============================================================================
// NUOTIN SOITTO
// ============================================================================

/**
 * playNote() - Soita yksittäinen nuotti tai tauko
 * 
 * Asettaa oskillaattoreiden taajuudet MIDI-nuotin mukaan,
 * käynnistää ADSR-envelopen ja tulostaa nuotin nimen.
 * 
 * @param noteIndex Soitettavan nuotin indeksi melody[]-taulukossa
 */
void playNote(int noteIndex) {
  // ──────────────────────────────────────────────────────────
  // Tarkista onko melodia päättynyt
  // ──────────────────────────────────────────────────────────
  if (noteIndex >= melodyLength) {
    // Aloita alusta
    currentNote = 0;
    noteIndex = 0;
    Serial.println("\n=== Melodia päättyi, aloitetaan alusta ===\n");
    delay(2000); // 2 sekunnin tauko ennen uudelleensoittoa
  }
  
  // Hae nykyinen nuotti
  Note note = melody[noteIndex];
  
  // ──────────────────────────────────────────────────────────
  // Käsittele tauko tai nuotti
  // ──────────────────────────────────────────────────────────
  if (note.pitch == REST) {
    // ════════════════════════════════════════════════════════
    // TAUKO
    // ════════════════════════════════════════════════════════
    notePlaying = false;
    Serial.print("REST ");
    
  } else {
    // ════════════════════════════════════════════════════════
    // NUOTTI
    // ════════════════════════════════════════════════════════
    
    // Muunna MIDI-numero taajuudeksi (Hz)
    float freq = mtof(note.pitch);
    
    // Aseta oskillaattori 1 (sini-aalto)
    aOscil.setFreq(freq);
    
    // Aseta oskillaattori 2 (saha-aalto)
    // 1% korkeampi taajuus luo detune-efektin
    aOscil2.setFreq(freq * 1.01);
    
    // Käynnistä ADSR-envelope (nousu alkaa)
    envelope.noteOn();
    
    notePlaying = true;
    
    // Tulosta nuotin nimi Serial Monitoriin
    Serial.print(getNoteNameFromMidi(note.pitch));
    Serial.print(" ");
  }
  
  // ──────────────────────────────────────────────────────────
  // Tulosta nuotin kesto
  // ──────────────────────────────────────────────────────────
  Serial.print("(");
  Serial.print(note.duration);
  Serial.print("ms) ");
  
  // Rivinvaihto joka 8. nuotin jälkeen luettavuuden parantamiseksi
  if ((noteIndex + 1) % 8 == 0) {
    Serial.println();
  }
  
  // ──────────────────────────────────────────────────────────
  // Päivitä tilamuuttujat
  // ──────────────────────────────────────────────────────────
  noteStartTime = millis();
  currentNote = noteIndex;
}

// ============================================================================
// APUFUNKTIOT
// ============================================================================

/**
 * getNoteNameFromMidi() - Muunna MIDI-numero nuotin nimeksi
 * 
 * Muuntaa MIDI-nuottinumeron ihmisluettavaan muotoon (esim. 60 -> "C4")
 * 
 * @param midiNote MIDI-nuottinumero (0-127)
 * @return String Nuotin nimi ja oktaavi (esim. "C4", "F#5")
 */
String getNoteNameFromMidi(byte midiNote) {
  // Nuottien nimet (12 puolisäveltä)
  const char* noteNames[] = {
    "C", "C#", "D", "D#", "E", "F", 
    "F#", "G", "G#", "A", "A#", "B"
  };
  
  // Laske oktaavi (MIDI-numero 60 = C4)
  int octave = (midiNote / 12) - 1;
  
  // Laske nuotti oktaavin sisällä (0-11)
  int noteIndex = midiNote % 12;
  
  // Yhdistä nuotin nimi ja oktaavi
  return String(noteNames[noteIndex]) + String(octave);
}

// ============================================================================
// KONTROLLIPÄIVITYS (suoritetaan 64 Hz taajuudella)
// ============================================================================

/**
 * updateControl() - Musiikin logiikan päivitys
 * 
 * Mozzin automaattisesti kutsuma funktio, joka suoritetaan
 * kontrollinopeudella (CONTROL_RATE = 64 Hz).
 * 
 * Hoitaa:
 * - Nuottien vaihdon oikeaan aikaan
 * - ADSR-envelopen päivityksen
 * - Nuottien lopetuksen (note off)
 */
void updateControl() {
  // ──────────────────────────────────────────────────────────
  // Tarkista pitääkö siirtyä seuraavaan nuottiin
  // ──────────────────────────────────────────────────────────
  unsigned long currentTime = millis();
  Note currentNoteData = melody[currentNote];
  
  // Onko nykyinen nuotti soitettu loppuun?
  if (currentTime - noteStartTime >= currentNoteData.duration) {
    
    // Jos nuotti oli soiva, lopeta se
    if (notePlaying) {
      envelope.noteOff(); // ADSR:n release-vaihe alkaa
    }
    
    // Siirry seuraavaan nuottiin
    playNote(currentNote + 1);
  }
  
  // ──────────────────────────────────────────────────────────
  // Päivitä ADSR-envelope
  // ──────────────────────────────────────────────────────────
  // Laskee envelopen nykyisen tason (0-255)
  envelope.update();
}

// ============================================================================
// AUDIOPÄIVITYS (suoritetaan 16384 Hz taajuudella)
// ============================================================================

/**
 * updateAudio() - Generoi yksittäinen audionäyte
 * 
 * Mozzin automaattisesti kutsuma funktio, joka suoritetaan
 * audionäytteenottotaajuudella (AUDIO_RATE = 16384 Hz).
 * 
 * Hoitaa:
 * - Oskillaattoreiden näytteiden generoinnin
 * - Kahden oskillaattorin miksaamisen
 * - ADSR-envelopen soveltamisen
 * - Näytteen palauttamisen I2S:lle
 * 
 * @return AudioOutput_t Audionäyte I2S-lähdölle
 */
AudioOutput_t updateAudio() {
  
  if (notePlaying) {
    // ════════════════════════════════════════════════════════
    // NUOTTI SOIVA - Generoi audionäyte
    // ════════════════════════════════════════════════════════
    
    // Hae seuraava näyte oskillaattori 1:ltä (sini-aalto)
    int sample1 = aOscil.next();
    
    // Hae seuraava näyte oskillaattori 2:lta (saha-aalto)
    int sample2 = aOscil2.next();
    
    // ──────────────────────────────────────────────────────────
    // Miksaa kaksi oskillaattoria yhteen
    // ──────────────────────────────────────────────────────────
    // Bit shift >> 1 on sama kuin jakaminen kahdella
    // Estää klippauksen kun kaksi signaalia lasketaan yhteen
    int mixed = (sample1 + sample2) >> 1;
    
    // ──────────────────────────────────────────────────────────
    // Sovella ADSR-envelope
    // ──────────────────────────────────────────────────────────
    // envelope.next() palauttaa arvon 0-255
    // Kerrotaan miksattu signaali envelopen tasolla
    // Jaetaan 256:lla (>> 8) palauttamaan alkuperäinen skaala
    int output = (mixed * (int)envelope.next()) >> 8;
    
    // Palauta mono-näyte I2S-lähdölle
    return MonoOutput::from8Bit(output);
    
  } else {
    // ════════════════════════════════════════════════════════
    // TAUKO - Palauta hiljaisuus
    // ════════════════════════════════════════════════════════
    return MonoOutput::from8Bit(0);
  }
}

// ============================================================================
// PÄÄSILMUKKA
// ============================================================================

/**
 * loop() - Arduino-pääsilmukka
 * 
 * Suoritetaan loputtomasti ohjelman käynnistyttyä.
 * Kutsuu Mozzin audioHook()-funktiota, joka hoitaa:
 * - updateControl()-funktion kutsumisen 64 Hz
 * - updateAudio()-funktion kutsumisen 16384 Hz
 * - I2S-puskurien täytön
 */
void loop() {
  audioHook(); // Mozzin sisäinen audionhallinta
}

// ============================================================================
// KOODIN LOPPU
// ============================================================================

/*
 * HUOMIOITA:
 * 
 * 1. ADSR-ENVELOPE:
 *    Attack-Decay-Sustain-Release antaa nuoteille luonnollisen
 *    dynamiikan. Ilman sitä nuotit alkaisivat ja loppuisivat äkillisesti.
 * 
 * 2. DETUNE-EFEKTI:
 *    Toinen oskillaattori on 1% korkeammalla taajuudella. Tämä luo
 *    chorus-tyyppisen efektin, joka tekee soundista täyteläisemmän.
 * 
 * 3. KAHDEN TASON PÄIVITYS:
 *    - updateControl() (64 Hz): Musiikin logiikka
 *    - updateAudio() (16384 Hz): Audionäytteiden generointi
 *    Tämä pitää prosessorikuorman kohtuullisena.
 * 
 * 4. I2S-PROTOKOLLA:
 *    ESP32 lähettää digitaalisen audion MAX98357A:lle, joka
 *    muuntaa sen analogiseksi signaaliksi kaiuttimelle.
 * 
 * 5. MIDI-JÄRJESTELMÄ:
 *    MIDI-numeroiden käyttö tekee nuoteista helppolukuisia.
 *    mtof()-funktio muuntaa MIDI-numeron taajuudeksi (Hz).
 */
