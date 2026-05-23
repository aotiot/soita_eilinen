# Beatles "Yesterday" - ESP32 + MAX98357A + Mozzi

Mozzi-kirjastolla toteutettu Beatles-kappaleen "Yesterday" soitin ESP32-mikrokontrollereille ja MAX98357A I2S-audiovahvistimelle.

## Mitä tämä tekee

Ohjelma soittaa "Yesterday"-kappaleen melodian käyttäen Mozzi-äänisyntetisaattorikirjastoa. Melodia koostuu kahdesta säkeistöstä ja kertosäkeestä. Ohjelma toistaa melodian loputtomasti.

## Laitteisto

### Tuetut mikrokontrollerit

- ESP32-S2
- ESP32-S3 (suositeltu, kaksi I2S-periferiaa)
- ESP32-C6
- ESP32-C3
- ESP32 (alkuperäinen, vaatii eri GPIO-pinnit)

### MAX98357A I2S-audiovahvistin

3W Class D -audiovahvistin I2S-digitaalitulolla. Sisältää:
- I2S-dekooderin
- DAC:n (Digital-to-Analog Converter)
- Vahvistimen

### Kytkennät

```
MAX98357A      ESP32-S2/S3/C6
─────────      ──────────────
LRC (LRCLK) ── GPIO 4
BCLK        ── GPIO 5
DIN         ── GPIO 6
GND         ── GND
VIN         ── 5V (tai 3.3V)
```

**Kaiutin:** 4-8 ohmin kaiutin MAX98357A:n + ja - liittimiin.

### Lisäasetukset MAX98357A:lla

- **SD (Shutdown)**: Kytke GND = päällä, VIN = pois päältä
- **GAIN**: Säädä vahvistusta
  - GND = 3dB
  - VIN = 9dB  
  - Irti = 15dB (oletus)

## Asennus

### 1. Arduino IDE

Asenna ESP32-tuki:
1. File → Preferences
2. Additional Board Manager URLs: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Tools → Board Manager → "esp32" → Install

### 2. Mozzi-kirjasto

Asenna Mozzi:
1. Sketch → Include Library → Manage Libraries
2. Etsi "Mozzi"
3. Asenna uusin versio

### 3. Mozzi-konfiguraatio ESP32-S2/S3/C6:lle

Siirry Mozzi-kirjaston kansioon:
```
Documents/Arduino/libraries/Mozzi/
```

Avaa tiedosto `AudioConfigESP32.h` ja varmista I2S-pinnit:

```cpp
#define I2S_BCLK_PIN 5
#define I2S_LRCLK_PIN 4  
#define I2S_DOUT_PIN 6
```

Jos tiedostossa on eri arvot, muuta ne vastaamaan yllä olevia.

### 4. Valitse oikea levy Arduino IDE:ssä

Tools → Board → ESP32 Arduino → valitse:
- ESP32S2 Dev Module
- ESP32S3 Dev Module
- ESP32C6 Dev Module

### 5. Lataa koodi

1. Avaa `yesterday_esp32.ino`
2. Kytke ESP32-laitteesi USB:llä
3. Tools → Port → valitse oikea portti
4. Upload

## Käyttö

1. Kytke virta ESP32:een
2. Melodia alkaa soida automaattisesti
3. Serial Monitor (115200 baud) näyttää:
   - Laitteen tyypin
   - Kytkentäohjeet
   - Soitettavat nuotit reaaliajassa

## Koodin rakenne

### Pääkomponentit

#### 1. Oskillaattorit
```cpp
Oscil<SIN2048_NUM_CELLS, AUDIO_RATE> aOscil(SIN2048_DATA);
Oscil<SAW2048_NUM_CELLS, AUDIO_RATE> aOscil2(SAW2048_DATA);
```
Kaksi oskillaattoria täyteläisemmän soundin saamiseksi. Toinen käyttää sini-aaltomuotoa, toinen saha-aaltomuotoa.

#### 2. ADSR Envelope
```cpp
ADSR <CONTROL_RATE, AUDIO_RATE> envelope;
```
Attack-Decay-Sustain-Release -verhokäyrä antaa nuoteille luonnollisen dynamiikan.

#### 3. Melodiadata
```cpp
Note melody[] = { ... };
```
Rakenne sisältää nuotin korkeuden (MIDI-numero) ja keston (millisekunteina).

### Keskeiset funktiot

#### `setup()`
- Käynnistää sarjaliikenneyhteyden
- Tunnistaa ESP32-laitteen tyypin
- Alustaa Mozzin ja ADSR-envelopen
- Käynnistää ensimmäisen nuotin

#### `playNote(int noteIndex)`
- Soittaa yksittäisen nuotin tai tauon
- Asettaa oskillaattoreiden taajuuden
- Käynnistää ADSR-envelopen
- Tulostaa nuotin nimen Serial Monitoriin

#### `updateControl()`
- Suoritetaan kontrollinopeudella (64 Hz)
- Tarkistaa onko aika siirtyä seuraavaan nuottiin
- Päivittää ADSR-envelopen tilan

#### `updateAudio()`
- Suoritetaan audionäytteenottotaajuudella (16384 Hz)
- Generoi audionäytteet
- Miksaa kaksi oskillaattoria
- Soveltaa ADSR-envelopea
- Palauttaa näytteen I2S:lle

### Äänensynteesi

Ohjelma käyttää kahta oskillaattoria pienen detune-efektin kanssa:
```cpp
aOscil.setFreq(freq);
aOscil2.setFreq(freq * 1.01); // 1% korkeampi taajuus
```

Tämä luo chorus-tyyppisen efektin, joka tekee soundista täyteläisemmän.

## Säätömahdollisuudet

### Muuta tempoa

Muuta nuottien kestoja `melody[]` -taulukossa:
```cpp
{F4, 300},  // 300ms → 150ms = kaksinkertainen tempo
```

### Muuta soundia

Vaihda aaltomuotoja:
```cpp
#include <tables/triangle2048_int8.h>
#include <tables/square_no_alias_2048_int8.h>

Oscil<TRIANGLE2048_NUM_CELLS, AUDIO_RATE> aOscil(TRIANGLE2048_DATA);
```

Käytettävissä olevat aaltomuodot:
- `sin` - Pehmeä, puhdas ääni
- `saw` - Terävä, kirkas ääni
- `square` - Onttoa, videöpelimäinen ääni
- `triangle` - Pehmeämpi kuin saha

### Säädä ADSR-envelopea

```cpp
envelope.setADLevels(255, 200);  // Attack, Sustain -tasot
envelope.setTimes(20, 50, 300, 100);  // Attack, Decay, Sustain, Release (ms)
```

- **Attack**: Kuinka nopeasti ääni nousee (ms)
- **Decay**: Kuinka nopeasti ääni laskee sustain-tasolle (ms)
- **Sustain**: Kuinka kauan sustain-taso kestää (ms)
- **Release**: Kuinka nopeasti ääni häipyy (ms)

### Muuta GPIO-pinnit

Jos GPIO 4, 5, 6 ovat varattuja muuhun käyttöön, voit muuttaa pinnit tiedostossa `AudioConfigESP32.h`:

```cpp
#define I2S_BCLK_PIN 14    // Vaihda haluamaksesi
#define I2S_LRCLK_PIN 15   // Vaihda haluamaksesi
#define I2S_DOUT_PIN 16    // Vaihda haluamaksesi
```

## Vianmääritys

### Ei ääntä

1. Tarkista kytkennät multimetrillä
2. Varmista että MAX98357A saa virran (LED palaa)
3. Tarkista Serial Monitor - näkyykö nuotteja?
4. Kokeile nostaa GAIN-pinni VIN:iin (9dB)
5. Mittaa oskilloskoopilla DIN-pinniltä - näkyykö signaali?

### Ääni säröilee

1. Laske äänenvoimakkuutta GAIN-pinnillä (kytke GND:hen)
2. Varmista että virtalähde antaa riittävästi virtaa
3. Käytä erillisiä GND- ja VCC-johtoja MAX98357A:lle

### Käännösvirheet

**"Mozzi.h: No such file or directory"**
- Asenna Mozzi-kirjasto Library Managerista

**"I2S driver install failed"**
- GPIO-pinnit ovat käytössä jossain muualla
- Muuta pinnit `AudioConfigESP32.h` -tiedostossa

**"undefined reference to 'i2s_driver_install'"**
- Valitse Tools → Board → ESP32-yhteensopiva levy

### ESP32-S2 erityistapaukset

ESP32-S2:ssa on vain yksi I2S-periferia. Jos käytät I2S:ää muuhun (esim. mikrofoni), melodian soitto ei toimi samanaikaisesti.

## Tekniset yksityiskohdat

### Audionäytteenottotaajuus

16384 Hz (16 kHz). Riittävä puheelle ja yksinkertaisille melodioille.

### MIDI-nuottijärjestelmä

Ohjelma käyttää MIDI-numeroita (60 = C4, keskimmäinen C). Mozzi muuntaa MIDI-numeron taajuudeksi funktiolla `mtof()` (MIDI to Frequency).

### I2S-protokolla

I2S (Inter-IC Sound) on sarjaväyläprotokolla audion siirtoon. Kolme signaalia:
- **BCLK (Bit Clock)**: Kellopulssi jokaiselle bitille
- **LRCLK (Left-Right Clock)**: Näytekello, vaihtelee vasemman/oikean kanavan välillä  
- **DIN (Data In)**: Audiodata

Näytteenottotaajuus = LRCLK-taajuus. Tässä 16384 Hz.

### Mozzi-arkkitehtuuri

Mozzi toimii kahdella päivitysnopeudella:

1. **Control Rate** (64 Hz): `updateControl()` - musiikin logiikka
2. **Audio Rate** (16384 Hz): `updateAudio()` - audionäytteiden generointi

Tämä kahdentason rakenne pitää prosessorikuorman kohtuullisena.

## Lisäominaisuuksia

### Lisää harmonia

Luo kolmas oskillaattori kvintin tai terssin päähän:

```cpp
Oscil<SIN2048_NUM_CELLS, AUDIO_RATE> aOscil3(SIN2048_DATA);

// updateControl():ssa
float freq = mtof(note.pitch);
aOscil3.setFreq(mtof(note.pitch + 7)); // Kvintti (+7 puolisäveltä)

// updateAudio():ssa
int sample3 = aOscil3.next();
int mixed = (sample1 + sample2 + sample3) / 3;
```

### Lisää efektejä

Mozzi sisältää efektikirjaston:
- Reverb
- Delay
- Flanger
- Chorus
- Distortion

Esimerkki:
```cpp
#include <ReverbTank.h>
ReverbTank reverb;
```

### Tallenna melodia SD-kortille

Muunna audiovirta WAV-tiedostoksi ja tallenna SD-kortille toistoa varten.

## Lähdekoodi

Koodi on jaettu selkeisiin osiohin:

1. **Kirjastot ja konfiguraatio** (rivit 1-30)
2. **Audio-objektit** (rivit 32-40)
3. **Melodiadata** (rivit 42-160)
4. **Setup-funktio** (rivit 162-200)
5. **Nuottien soitto** (rivit 202-250)
6. **Kontrollilogiikka** (rivit 252-280)
7. **Audionäytteiden generointi** (rivit 282-310)
8. **Pääsilmukka** (rivit 312-315)

Jokainen funktio on dokumentoitu koodin sisällä.

## Lisenssi

Koodi on MIT-lisensoitu. Mozzi-kirjasto on LGPL 2.1 -lisensoitu.

## Tekijä

Toteutettu Mozzi-kirjastolla ja ESP32-alustalla. Melodia: The Beatles - "Yesterday" (Lennon/McCartney).

## Lähteet

- [Mozzi-dokumentaatio](https://sensorium.github.io/Mozzi/)
- [ESP32-I2S-dokumentaatio](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html)
- [MAX98357A-datalevy](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf)
