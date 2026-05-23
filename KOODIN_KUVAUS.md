# Koodin kuvaus - Beatles "Yesterday" ESP32-soitin

## Yleiskatsaus

Ohjelma on ESP32-pohjainen äänisyntetisaattori, joka soittaa Beatles-kappaleen "Yesterday" melodian. Se käyttää Mozzi-kirjastoa äänensynteesiin ja MAX98357A I2S-vahvistinta audiotoistoon.

## Arkkitehtuuri

### Komponenttihierarkia

```
Arduino Framework
    ↓
Mozzi Library
    ↓
├─ Audio Synthesis (updateAudio @ 16384 Hz)
├─ Control Logic (updateControl @ 64 Hz)
└─ I2S Output Driver
    ↓
MAX98357A DAC + Amplifier
    ↓
Speaker (4-8Ω)
```

### Datavirta

```
MIDI Note Data → mtof() → Frequency (Hz)
                              ↓
                    Oscillator 1 (Sin) ───┐
                    Oscillator 2 (Saw) ───┤ → Mixer
                                           ↓
                                    ADSR Envelope
                                           ↓
                                    8-bit Sample
                                           ↓
                                    I2S Protocol
                                           ↓
                                    MAX98357A
```

## Keskeiset osat

### 1. Oskillaattorit (Oscillators)

**Mikä:** Oskillaattorit generoivat perusääniaallon.

**Miten toimii:**
```cpp
Oscil<SIN2048_NUM_CELLS, AUDIO_RATE> aOscil(SIN2048_DATA);
```

Tämä luo oskillaattorin, joka:
- Käyttää 2048 näytteen sini-aaltotaulukkoa
- Lukee taulukkoa taajuuden määräämällä nopeudella
- Tuottaa 16384 näytettä sekunnissa

**Esimerkki:**
Jos taajuus on 440 Hz (A4), oskillaattori lukee sini-taulukkoa niin nopeasti, että se pyörähtää kokonaan läpi 440 kertaa sekunnissa.

**Miksi kaksi oskillaattoria?**
```cpp
aOscil.setFreq(freq);       // 440.0 Hz
aOscil2.setFreq(freq * 1.01); // 444.4 Hz
```

Toinen oskillaattori on 1% korkeammalla taajuudella. Tämä luo:
- **Beats-efektin:** Taajuusero aiheuttaa värähtelyn (4.4 Hz tässä tapauksessa)
- **Chorus-efektin:** Soundi kuulostaa täyteläisemmältä, kuin kaksi soitinta soittaisi yhtä aikaa
- **Stereomaisen vaikutelman:** Vaikka lähtö on mono, ääni tuntuu leveämmältä

### 2. ADSR-envelope

**Mikä:** Attack-Decay-Sustain-Release on verhokäyrä, joka määrää miten äänen voimakkuus muuttuu ajan funktiona.

**Vaiheet:**
```
Amplitude
    ↑
255 |     A
    |    /|\
200 |   / | \___S_____
    |  /  |D          \
  0 |_/_______________\R___→ Time
      20  50   300   100 (ms)
```

- **Attack (20ms):** Ääni nousee nopeasti täyteen voimakkuuteen (0→255)
- **Decay (50ms):** Ääni laskee sustain-tasolle (255→200)
- **Sustain (300ms):** Ääni pysyy tasaisena (200)
- **Release (100ms):** Ääni häipyy kun nuotti lopetetaan (200→0)

**Koodi:**
```cpp
envelope.setADLevels(255, 200);        // Attack peak, Sustain level
envelope.setTimes(20, 50, 300, 100);   // A, D, S, R millisekunteina
```

**Miksi tarvitaan?**
Ilman envelopea nuotit alkaisivat ja loppuisivat äkillisesti (click-ääni). Envelope tekee nuoteista luonnollisen kuuloisia.

### 3. Melodiadata

**Rakenne:**
```cpp
struct Note {
  byte pitch;              // MIDI-numero (60-79) tai REST (0)
  unsigned int duration;   // Millisekunteina
};
```

**MIDI-järjestelmä:**
```
C4 = 60 → mtof(60) → 261.63 Hz
D4 = 62 → mtof(62) → 293.66 Hz
E4 = 64 → mtof(64) → 329.63 Hz
```

mtof()-funktio käyttää kaavaa:
```
freq = 440 * 2^((note - 69) / 12)
```

Missä note on MIDI-numero ja 69 = A4 (440 Hz).

**Esimerkki melodiasta:**
```cpp
Note melody[] = {
  {F4, 300},  // "Yes-" (F4, 300ms)
  {E4, 300},  // "ter-" (E4, 300ms)
  {D4, 600},  // "day"  (D4, 600ms)
  ...
};
```

### 4. Ajastusjärjestelmä

**Kaksi tasoa:**

#### a) Control Rate (64 Hz)
```cpp
void updateControl() {
  // Suoritetaan 64 kertaa sekunnissa
  // Hoitaa: nuottien vaihto, envelopen päivitys
}
```

64 Hz riittää musiikin logiikalle, koska:
- Nopein nuotti kestää ~200ms
- 64 Hz = päivitys joka 15.6ms
- Tarkkuus riittää musiikille

#### b) Audio Rate (16384 Hz)
```cpp
AudioOutput_t updateAudio() {
  // Suoritetaan 16384 kertaa sekunnissa
  // Hoitaa: audionäytteiden generointi
}
```

16384 Hz audionäytteenottotaajuus:
- Nyquist-teoreeman mukaan max taajuus = 16384/2 = 8192 Hz
- Riittää musiikille (ihmisen kuuloalue ~20-20000 Hz)
- Alempi kuin CD-laatu (44100 Hz), mutta toimii ESP32:lla kevyesti

### 5. I2S-protokolla

**Signaalit:**
```
BCLK (Bit Clock):  ┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌┐┌
                   └┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└

LRCLK (Word Sel):  ┌───────────────┐───────────────┐
                   └───────────────┘───────────────┘
                   L Channel        R Channel

DIN (Data):        ═══D15D14D13...D0═══D15D14D13...D0═══
```

**Parametrit:**
- BCLK = 16384 Hz × 32 bit × 2 kanavaa = 1,048,576 Hz
- LRCLK = 16384 Hz (näytteenottotaajuus)
- Data = 16-bit näytteet (vaikka generoimme 8-bit)

## Ohjelman kulku

### Käynnistys (setup)

```
1. Serial.begin(115200)
   └─ Käynnistä sarjaliikenne debuggausta varten

2. Tunnista ESP32-laitteen tyyppi
   └─ Tulosta laitetyyppi ja I2S-periferioiden määrä

3. startMozzi(CONTROL_RATE)
   ├─ Alusta I2S-ajuri
   ├─ Luo DMA-puskurit
   ├─ Käynnistä ajastimet
   └─ Rekisteröi callback-funktiot

4. envelope.setADLevels(255, 200)
   └─ Konfiguroi ADSR-verhokäyrä

5. playNote(0)
   └─ Aloita ensimmäinen nuotti
```

### Pääsilmukka (loop)

```
loop() {
  audioHook();  // Mozzin sisäinen funktio
}
```

audioHook() hoitaa:
```
┌─────────────────────────────────────┐
│ audioHook()                         │
├─────────────────────────────────────┤
│ 1. Tarkista onko päivitysaika?      │
│    ↓                                │
│ 2. Kutsu updateControl()            │
│    (jos 15.6ms kulunut)             │
│    ↓                                │
│ 3. Silmukka 256 kertaa:             │
│    ├─ Kutsu updateAudio()           │
│    └─ Tallenna näyte DMA-puskuriin  │
│    ↓                                │
│ 4. Kun puskuri täynnä:              │
│    └─ I2S-ajuri lähettää MAX98357A  │
└─────────────────────────────────────┘
```

### Nuotin soitto

```
playNote(index) kutsutaan
    ↓
Hae nuotti melody[index]
    ↓
Onko REST?
    ├─ Kyllä: notePlaying = false
    │         (updateAudio() palauttaa 0)
    │
    └─ Ei: float freq = mtof(pitch)
           ↓
           aOscil.setFreq(freq)
           aOscil2.setFreq(freq * 1.01)
           ↓
           envelope.noteOn()
           ↓
           notePlaying = true
           ↓
           (updateAudio() generoi näytteitä)
```

### Audionäytteen generointi

```cpp
AudioOutput_t updateAudio() {
  if (notePlaying) {
    // 1. Hae näytteet oskillaattoreilta
    int sample1 = aOscil.next();   // -128 ... +127
    int sample2 = aOscil2.next();  // -128 ... +127
    
    // 2. Miksaa (keskiarvo)
    int mixed = (sample1 + sample2) >> 1;  // -128 ... +127
    
    // 3. Sovella envelope
    int envLevel = envelope.next();        // 0 ... 255
    int output = (mixed * envLevel) >> 8;  // -128 ... +127
    
    // 4. Palauta I2S:lle
    return MonoOutput::from8Bit(output);
  } else {
    return MonoOutput::from8Bit(0);  // Hiljaisuus
  }
}
```

**Matematiikka:**

Jos mixed = 100 ja envLevel = 200:
```
output = (100 * 200) >> 8
       = 20000 >> 8
       = 20000 / 256
       = 78
```

Näin envelope vaikuttaa äänenvoimakkuuteen.

### Nuotin vaihto

```cpp
void updateControl() {
  unsigned long currentTime = millis();
  Note currentNoteData = melody[currentNote];
  
  // Onko nuotti soitettu tarpeeksi kauan?
  if (currentTime - noteStartTime >= currentNoteData.duration) {
    
    // Lopeta vanha nuotti
    if (notePlaying) {
      envelope.noteOff();  // Release-vaihe alkaa
    }
    
    // Aloita uusi nuotti
    playNote(currentNote + 1);
  }
  
  // Päivitä envelope (liikuttaa A/D/S/R-käyrällä)
  envelope.update();
}
```

**Aikajana esimerkki:**

```
t=0ms     playNote(0) → F4, 300ms
t=15ms    updateControl() → tarkista aika
t=31ms    updateControl() → tarkista aika
...
t=300ms   updateControl() → vaihda nuottiin 1
t=300ms   playNote(1) → E4, 300ms
...
```

## Muistin käyttö

### Flash-muisti (ohjelmakoodi)

- Mozzi-kirjasto: ~50 KB
- Aaltomuototaulukot: ~4 KB (2048 × 2 näytettä)
- Ohjelmakoodi: ~10 KB
- **Yhteensä: ~64 KB**

### RAM-muisti

- Mozzi-puskurit: ~2 KB
- I2S DMA-puskurit: ~4 KB
- Melody-taulukko: ~200 bytes
- Muuttujat: ~1 KB
- **Yhteensä: ~7 KB**

ESP32-S3:ssa on 512 KB RAM, joten muisti ei ole ongelma.

### Prosessorikuorma

**Control Rate (64 Hz):**
- Suoritus kerran per 15.6ms
- Kesto: ~100 µs
- Kuorma: 0.6%

**Audio Rate (16384 Hz):**
- Suoritus kerran per 61 µs
- Kesto: ~10 µs
- Kuorma: 16%

**I2S-ajuri:**
- DMA hoitaa siirron automaattisesti
- Ei kuormita CPU:ta

**Yhteensä: ~17% CPU-käyttö @ 240 MHz**

## Laajennusmahdollisuudet

### 1. Lisää oskillaattoreita (harmonia)

```cpp
Oscil<SIN2048_NUM_CELLS, AUDIO_RATE> aOscil3(SIN2048_DATA);

void playNote(int noteIndex) {
  // ...
  aOscil3.setFreq(mtof(note.pitch + 7)); // Kvintti (+7 puolisäveltä)
}

AudioOutput_t updateAudio() {
  int sample3 = aOscil3.next();
  int mixed = (sample1 + sample2 + sample3) / 3;
  // ...
}
```

### 2. Lisää efektejä

```cpp
#include <ReverbTank.h>
ReverbTank reverb;

AudioOutput_t updateAudio() {
  // ... generointi ...
  int withReverb = reverb.next(output);
  return MonoOutput::from8Bit(withReverb);
}
```

### 3. Dynaaminen tempo

```cpp
float tempoMultiplier = 1.0; // 1.0 = normaali, 2.0 = kaksinkertainen

void playNote(int noteIndex) {
  // ...
  int adjustedDuration = note.duration / tempoMultiplier;
  // Käytä adjustedDuration ajan tarkistuksessa
}
```

### 4. Useita instrumentteja

```cpp
enum Instrument {
  PIANO,
  GUITAR,
  FLUTE
};

void setInstrument(Instrument inst) {
  switch(inst) {
    case PIANO:
      // Sini-aalto, nopea attack
      envelope.setTimes(5, 50, 200, 300);
      break;
    case GUITAR:
      // Saha-aalto, keskipitkä attack
      envelope.setTimes(20, 100, 300, 100);
      break;
    case FLUTE:
      // Kolmio-aalto, hidas attack
      envelope.setTimes(100, 50, 400, 200);
      break;
  }
}
```

## Yhteenveto

Ohjelma on modulaarinen äänisyntetisaattori, joka yhdistää:
- **Oskillaattorit** → Perusääni
- **ADSR-envelope** → Dynamiikka
- **MIDI-järjestelmä** → Nuottien määrittely
- **I2S-protokolla** → Audion siirto
- **MAX98357A** → DA-muunnos ja vahvistus

Arkkitehtuuri on selkeä ja laajennettavissa. Mozzi-kirjasto käsittelee audion matalalla tasolla, jolloin ohjelmointi pysyy korkealla tasolla.

Kahden tason päivitysjärjestelmä (64 Hz kontrolli, 16384 Hz audio) pitää prosessorikuorman alhaisena samalla kun audion laatu riittää musiikin toistoon.
