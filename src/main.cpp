#define LGFX_AUTODETECT
#define LGFX_USE_V1

#include <Arduino.h>
#include "arduinoFFT.h"          // Standard Arduino FFT library 
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp> 
#include <SPI.h>
// #include <TFT_eSPI.h>

arduinoFFT FFT = arduinoFFT();

#define SAMPLES 512              // Must be a power of 2
#define SAMPLING_FREQUENCY 40000 
// Hz, must be 40000 or less due to ADC conversion time.
// Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.

// TFT_eSPI lcd = TFT_eSPI();  
static LGFX lcd;
 
struct eqBand {
  const char *freqname;
  uint16_t amplitude;
  int peak;
  int lastpeak;
  uint16_t lastval;
  unsigned long lastmeasured;
};
 
eqBand audiospectrum[8] = {
  //Adjust the amplitude values to fit your microphone
  { "125Hz", 500, 0, 0, 0, 0},
  { "250Hz", 200, 0, 0, 0, 0},
  { "500Hz", 200, 0, 0, 0, 0},
  { "1KHz",  200, 0, 0, 0, 0},
  { "2KHz",  200, 0, 0, 0, 0},
  { "4KHz",  100, 0, 0, 0, 0},
  { "8KHz",  100, 0, 0, 0, 0},
  { "16KHz", 50,  0, 0, 0, 0}
};
 
unsigned int sampling_period_us;
unsigned long microseconds;
double vReal[SAMPLES];
double vImag[SAMPLES];
unsigned long newTime, oldTime;
uint16_t tft_width  = 320; // ILI9341_TFTWIDTH;
uint16_t tft_height = 240; // ILI9341_TFTHEIGHT;
uint8_t bands = 8;
uint8_t bands_width = floor( tft_width / bands );
uint8_t bands_pad = bands_width - 10;
uint16_t colormap[255]; // color palette for the band meter (pre-fill in setup)

void displayBand(int band, int dsize);
byte getBand(int i);
 
void setup() {
  pinMode(WIO_MIC, INPUT);
  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  lcd.setTextSize(1);
  lcd.setRotation(1);
  sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQUENCY));
  delay(2000);
  for(uint8_t i=0;i<tft_height;i++) {
    colormap[i] = lcd.color565(tft_height-i*.5, i*1.1, 0);
  }
  for (byte band = 0; band <= 7; band++) {
    lcd.setCursor(bands_width*band + 2, 0);
    lcd.print(audiospectrum[band].freqname);
  }
}
 
void loop() {
  for (int i = 0; i < SAMPLES; i++) {
    newTime = micros()-oldTime;
    oldTime = newTime;
    vReal[i] = analogRead(WIO_MIC); // A conversion takes about 1uS on an ESP32
    vImag[i] = 0;
    while (micros() < (newTime + sampling_period_us)) { 
      // do nothing to wait
    }
  }
  FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);
 
  for (int i = 2; i < (SAMPLES/2); i++){ 
    // Don't use sample 0 and only first SAMPLES/2 are usable. 
    // Each array eleement represents a frequency and its value the amplitude.
    if (vReal[i] > 1500) { // Add a crude noise filter, 10 x amplitude or more
      byte bandNum = getBand(i);
      if(bandNum!=8) {
        displayBand(bandNum, (int)vReal[i]/audiospectrum[bandNum].amplitude);
      }
    }
  }
   
  long vnow = millis();
  for (byte band = 0; band <= 7; band++) {
    // auto decay every 50ms on low activity bands
    if(vnow - audiospectrum[band].lastmeasured > 50) {
      displayBand(band, audiospectrum[band].lastval>4 ? audiospectrum[band].lastval-4 : 0);
    }
    if (audiospectrum[band].peak > 0) {
      audiospectrum[band].peak -= 2;
      if(audiospectrum[band].peak<=0) {
        audiospectrum[band].peak = 0;
      }
    }
    // only draw if peak changed
    if(audiospectrum[band].lastpeak != audiospectrum[band].peak) {
      // delete last peak
     lcd.drawFastHLine(bands_width*band,tft_height-audiospectrum[band].lastpeak,bands_pad,TFT_BLACK);
     audiospectrum[band].lastpeak = audiospectrum[band].peak;
     lcd.drawFastHLine(bands_width*band, tft_height-audiospectrum[band].peak,
                           bands_pad, colormap[tft_height-audiospectrum[band].peak]);
    }
  } 
}
 
void displayBand(int band, int dsize){
  uint16_t hpos = bands_width*band;
  int dmax = 200;
  if(dsize>tft_height-10) {
    dsize = tft_height-10; // leave some hspace for text
  }
  if(dsize < audiospectrum[band].lastval) {
    // lower value, delete some lines
    lcd.fillRect(hpos, tft_height-audiospectrum[band].lastval,
                    bands_pad, audiospectrum[band].lastval - dsize, TFT_BLACK);
  }
  if (dsize > dmax) dsize = dmax;
  for (int s = 0; s <= dsize; s=s+4){
    lcd.drawFastHLine(hpos, tft_height-s, bands_pad, colormap[tft_height-s]);
  }
  if (dsize > audiospectrum[band].peak) {
    audiospectrum[band].peak = dsize;
  }
  audiospectrum[band].lastval = dsize;
  audiospectrum[band].lastmeasured = millis();
}
 
byte getBand(int i) {
  if (i<=2 )             return 0;  // 125Hz
  if (i >3   && i<=5 )   return 1;  // 250Hz
  if (i >5   && i<=7 )   return 2;  // 500Hz
  if (i >7   && i<=15 )  return 3;  // 1000Hz
  if (i >15  && i<=30 )  return 4;  // 2000Hz
  if (i >30  && i<=53 )  return 5;  // 4000Hz
  if (i >53  && i<=200 ) return 6;  // 8000Hz
  if (i >200           ) return 7;  // 16000Hz
  return 8;
}