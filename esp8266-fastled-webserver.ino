/*
M.nu ESP8266 + FastLED web server.
Based on: https://github.com/jasoncoon/esp8266-fastled-webserver
Copyright (C) 2015 Jason Coon

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.


This code has been adjusted for use with WeMos D1 Mini and the
M.nu Levelshift Shield, but can of course be tailored to use with other
hardware.

Changes:

*/
#include "FastLED.h"
FASTLED_USING_NAMESPACE

extern "C" {
    #include "user_interface.h"
}

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <FS.h>
#include <EEPROM.h>
#include "GradientPalettes.h"

ESP8266WebServer server(80);

#define DATA_PIN      D7     // for Huzzah: Pins w/o special function:  #4, #5, #12, #13, #14; // #16 does not work :(
#define CLK_PIN       D6
#define LED_TYPE      WS2801
#define COLOR_ORDER   RGB
#define NUM_LEDS      150

#define MILLI_AMPS         2000     // IMPORTANT: set here the max milli-Amps of your power supply 5V 2A = 2000
#define FRAMES_PER_SECOND  120 // here you can control the speed. With the Access Point / Web Server the animations run a bit slower.

CRGB leds[NUM_LEDS];

bool patternChanged = false;

uint8_t patternIndex = 0;

const uint8_t brightnessCount = 5;
uint8_t brightnessMap[brightnessCount] = { 16, 32, 64, 128, 255 };
int brightnessIndex = 0;
uint8_t brightness = brightnessMap[brightnessIndex];

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// ten seconds per color palette makes a good demo
// 20-120 is better for deployment
#define SECONDS_PER_PALETTE 10

///////////////////////////// PROTOTYPES
void loadSettings();
void sendAll();
void sendPower();
void sendPattern();
void sendPalette();
void sendBrightness();
void sendSolidColor();
void setPower(uint8_t value);
void setSolidColor(CRGB color);
void setSolidColor(uint8_t r, uint8_t g, uint8_t b);
void adjustPattern(bool up);
void setPattern(int value);
void setPalette(int value);
void adjustBrightness(bool up);
void setBrightness(int value);
void showSolidColor();
void rainbow();
void rainbowWithGlitter();
void snowGlitter();
void colorRun();
void addGlitter( fract8 chanceOfGlitter);
void confetti();
void sinelon();
void bpm();
void juggle();
void pride();
void colorwaves();
void palettetest();
///////////////////////////////////////////////////////////////////////

// Forward declarations of an array of cpt-city gradient palettes, and
// a count of how many there are.  The actual color palette definitions
// are at the bottom of this file.
extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];
extern const uint8_t gGradientPaletteCount;

// Current palette number from the 'playlist' of color palettes
uint8_t gCurrentPaletteNumber = 0;

CRGBPalette16 gCurrentPalette( CRGB::Black);
CRGBPalette16 gTargetPalette( gGradientPalettes[0] );

uint8_t currentPatternIndex = 0; // Index number of which pattern is current

uint8_t currentPaletteIndex = 0;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

CRGB solidColor = CRGB::Blue;

uint8_t power = 1;

void setup(void) {
    Serial.begin(115200);
    delay(100);
    Serial.setDebugOutput(true);

    // If device does not have valid AP login, starts as AP with WiFi config @ http://192.168.4.1
    WiFiManager wifiManager;
    wifiManager.autoConnect("XMasLights", "m.nu");

    //FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);         // for WS2812 (Neopixel)
    FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN,COLOR_ORDER>(leds, NUM_LEDS); // for APA102 (Dotstar), WS2801 etc
    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(brightness);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, MILLI_AMPS);
    fill_solid(leds, NUM_LEDS, solidColor);
    FastLED.show();

    EEPROM.begin(512);
    loadSettings();

    FastLED.setBrightness(brightness);

    Serial.println();
    Serial.print( F("Heap: ") ); Serial.println(system_get_free_heap_size());
    Serial.print( F("Boot Vers: ") ); Serial.println(system_get_boot_version());
    Serial.print( F("CPU: ") ); Serial.println(system_get_cpu_freq());
    Serial.print( F("SDK: ") ); Serial.println(system_get_sdk_version());
    Serial.print( F("Chip ID: ") ); Serial.println(system_get_chip_id());
    Serial.print( F("Flash ID: ") ); Serial.println(spi_flash_get_id());
    Serial.print( F("Flash Size: ") ); Serial.println(ESP.getFlashChipRealSize());
    Serial.print( F("Vcc: ") ); Serial.println(ESP.getVcc());
    Serial.println();

    SPIFFS.begin();
    {
        Dir dir = SPIFFS.openDir("/");
        while (dir.next()) {
            String fileName = dir.fileName();
            size_t fileSize = dir.fileSize();
            Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
        }
        Serial.printf("\n");
    }

    server.on("/all", HTTP_GET, []() {
        sendAll();
    });

    server.on("/power", HTTP_GET, []() {
        sendPower();
    });

    server.on("/power", HTTP_POST, []() {
        String value = server.arg("value");
        setPower(value.toInt());
        sendPower();
    });

    server.on("/solidColor", HTTP_GET, []() {
        sendSolidColor();
    });

    server.on("/solidColor", HTTP_POST, []() {
        String r = server.arg("r");
        String g = server.arg("g");
        String b = server.arg("b");
        setSolidColor(r.toInt(), g.toInt(), b.toInt());
        sendSolidColor();
    });

    server.on("/pattern", HTTP_GET, []() {
        sendPattern();
    });

    server.on("/pattern", HTTP_POST, []() {
        patternChanged = true;
        String value = server.arg("value");
        setPattern(value.toInt());
        sendPattern();
    });

    //TODO: do this for button press
    server.on("/patternUp", HTTP_POST, []() {
        patternChanged = true;
        adjustPattern(true);
        sendPattern();
    });

    server.on("/patternDown", HTTP_POST, []() {
        patternChanged = true;
        adjustPattern(false);
        sendPattern();
    });

    server.on("/brightness", HTTP_GET, []() {
        sendBrightness();
    });

    server.on("/brightness", HTTP_POST, []() {
        String value = server.arg("value");
        setBrightness(value.toInt());
        sendBrightness();
    });

    server.on("/brightnessUp", HTTP_POST, []() {
        adjustBrightness(true);
        sendBrightness();
    });

    server.on("/brightnessDown", HTTP_POST, []() {
        adjustBrightness(false);
        sendBrightness();
    });

    server.on("/palette", HTTP_GET, []() {
        sendPalette();
    });

    server.on("/palette", HTTP_POST, []() {
        String value = server.arg("value");
        setPalette(value.toInt());
        sendPalette();
    });

    server.serveStatic("/index.htm", SPIFFS, "/index.htm");
    server.serveStatic("/fonts", SPIFFS, "/fonts", "max-age=86400");
    server.serveStatic("/js", SPIFFS, "/js");
    server.serveStatic("/css", SPIFFS, "/css", "max-age=86400");
    server.serveStatic("/images", SPIFFS, "/images", "max-age=86400");
    server.serveStatic("/", SPIFFS, "/index.htm");

    server.begin();

    Serial.println("HTTP server started");
}

typedef void (*Pattern)();
typedef struct {
    Pattern pattern;
    String name;
} PatternAndName;
typedef PatternAndName PatternAndNameList[];

// List of patterns to cycle through.  Each is defined as a separate function below.
PatternAndNameList patterns = {
    { colorwaves, "Color Waves" },
    { palettetest, "Palette Test" },
    { pride, "Pride" },
    { rainbow, "Rainbow" },
    { rainbowWithGlitter, "Rainbow With Glitter" },
    { snowGlitter, "Snow Glitter"},
    //{ colorRun, "Color Run"},
    { confetti, "Confetti" },
    { sinelon, "Sinelon" },
    { juggle, "Juggle" },
    { bpm, "BPM" },
    { showSolidColor, "Solid Color" },
};

const uint8_t patternCount = ARRAY_SIZE(patterns);

typedef struct {
    CRGBPalette16 palette;
    String name;
} PaletteAndName;
typedef PaletteAndName PaletteAndNameList[];

const CRGBPalette16 palettes[] = {
    RainbowColors_p,
    RainbowStripeColors_p,
    CloudColors_p,
    LavaColors_p,
    OceanColors_p,
    ForestColors_p,
    PartyColors_p,
    HeatColors_p
};

const uint8_t paletteCount = ARRAY_SIZE(palettes);

const String paletteNames[paletteCount] = {
    "Rainbow",
    "Rainbow Stripe",
    "Cloud",
    "Lava",
    "Ocean",
    "Forest",
    "Party",
    "Heat",
};

void loop(void) {
    // Add entropy to random number generator; we use a lot of it.
    random16_add_entropy(random(65535));

    server.handleClient();

    if (power == 0) {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();
        FastLED.delay(15);
        return;
    }

    // EVERY_N_SECONDS(10) {
    //   Serial.print( F("Heap: ") ); Serial.println(system_get_free_heap_size());
    // }

    EVERY_N_MILLISECONDS( 20 ) {
        gHue++;  // slowly cycle the "base color" through the rainbow
    }

    // change to a new cpt-city gradient palette
    EVERY_N_SECONDS( SECONDS_PER_PALETTE ) {
        gCurrentPaletteNumber = addmod8( gCurrentPaletteNumber, 1, gGradientPaletteCount);
        gTargetPalette = gGradientPalettes[ gCurrentPaletteNumber ];
    }

    // slowly blend the current cpt-city gradient palette to the next
    EVERY_N_MILLISECONDS(40) {
        nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 16);
    }

    // Call the current pattern function once, updating the 'leds' array
    patterns[currentPatternIndex].pattern();

    FastLED.show();

    // insert a delay to keep the framerate modest
    FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void loadSettings() {
    brightness = EEPROM.read(0);

    currentPatternIndex = EEPROM.read(1);
    if (currentPatternIndex < 0)
    currentPatternIndex = 0;
    else if (currentPatternIndex >= patternCount)
    currentPatternIndex = patternCount - 1;

    byte r = EEPROM.read(2);
    byte g = EEPROM.read(3);
    byte b = EEPROM.read(4);

    if (!(r == 0 && g == 0 && b == 0)) {
        //}
        //else
        //{
        solidColor = CRGB(r, g, b);
    }

    currentPaletteIndex = EEPROM.read(5);
    if (currentPaletteIndex < 0)
    currentPaletteIndex = 0;
    else if (currentPaletteIndex >= paletteCount)
    currentPaletteIndex = paletteCount - 1;
}

void sendAll() {
    String json = "{";

    json += "\"power\":" + String(power) + ",";
    json += "\"brightness\":" + String(brightness) + ",";

    json += "\"currentPattern\":{";
    json += "\"index\":" + String(currentPatternIndex);
    json += ",\"name\":\"" + patterns[currentPatternIndex].name + "\"}";

    json += ",\"currentPalette\":{";
    json += "\"index\":" + String(currentPaletteIndex);
    json += ",\"name\":\"" + paletteNames[currentPaletteIndex] + "\"}";

    json += ",\"solidColor\":{";
    json += "\"r\":" + String(solidColor.r);
    json += ",\"g\":" + String(solidColor.g);
    json += ",\"b\":" + String(solidColor.b);
    json += "}";

    json += ",\"patterns\":[";
    for (uint8_t i = 0; i < patternCount; i++) {
        json += "\"" + patterns[i].name + "\"";
        if (i < patternCount - 1)
        json += ",";
    }
    json += "]";

    json += ",\"palettes\":[";
    for (uint8_t i = 0; i < paletteCount; i++) {
        json += "\"" + paletteNames[i] + "\"";
        if (i < paletteCount - 1)
        json += ",";
    }
    json += "]";

    json += "}";

    server.send(200, "text/json", json);
    json = String();
}

void sendPower() {
    String json = String(power);
    server.send(200, "text/json", json);
    json = String();
}

void sendPattern() {
    String json = "{";
    json += "\"index\":" + String(currentPatternIndex);
    json += ",\"name\":\"" + patterns[currentPatternIndex].name + "\"";
    json += "}";
    server.send(200, "text/json", json);
    json = String();
}

void sendPalette() {
    String json = "{";
    json += "\"index\":" + String(currentPaletteIndex);
    json += ",\"name\":\"" + paletteNames[currentPaletteIndex] + "\"";
    json += "}";
    server.send(200, "text/json", json);
    json = String();
}

void sendBrightness() {
    String json = String(brightness);
    server.send(200, "text/json", json);
    json = String();
}

void sendSolidColor() {
    String json = "{";
    json += "\"r\":" + String(solidColor.r);
    json += ",\"g\":" + String(solidColor.g);
    json += ",\"b\":" + String(solidColor.b);
    json += "}";
    server.send(200, "text/json", json);
    json = String();
}

void setPower(uint8_t value) {
    power = value == 0 ? 0 : 1;
}

void setSolidColor(CRGB color) {
    setSolidColor(color.r, color.g, color.b);
}

void setSolidColor(uint8_t r, uint8_t g, uint8_t b) {
    solidColor = CRGB(r, g, b);

    EEPROM.write(2, r);
    EEPROM.write(3, g);
    EEPROM.write(4, b);

    setPattern(patternCount - 1);
}

// increase or decrease the current pattern number, and wrap around at the ends
void adjustPattern(bool up) {
    if (up)
    currentPatternIndex++;
    else
    currentPatternIndex--;

    // wrap around at the ends
    if (currentPatternIndex < 0)
    currentPatternIndex = patternCount - 1;
    if (currentPatternIndex >= patternCount)
    currentPatternIndex = 0;
}

void setPattern(int value) {
    // don't wrap around at the ends
    if (value < 0)
    value = 0;
    else if (value >= patternCount)
    value = patternCount - 1;

    currentPatternIndex = value;

    EEPROM.write(1, currentPatternIndex);
    EEPROM.commit();
}

void setPalette(int value) {
    // don't wrap around at the ends
    if (value < 0)
    value = 0;
    else if (value >= paletteCount)
    value = paletteCount - 1;

    currentPaletteIndex = value;

    EEPROM.write(5, currentPaletteIndex);
    EEPROM.commit();
}

// adjust the brightness, and wrap around at the ends
void adjustBrightness(bool up) {
    if (up)
    brightnessIndex++;
    else
    brightnessIndex--;

    // wrap around at the ends
    if (brightnessIndex < 0)
    brightnessIndex = brightnessCount - 1;
    else if (brightnessIndex >= brightnessCount)
    brightnessIndex = 0;

    brightness = brightnessMap[brightnessIndex];

    FastLED.setBrightness(brightness);

    EEPROM.write(0, brightness);
    EEPROM.commit();
}

void setBrightness(int value) {
    // don't wrap around at the ends
    if (value > 255)
    value = 255;
    else if (value < 0) value = 0;

    brightness = value;

    FastLED.setBrightness(brightness);

    EEPROM.write(0, brightness);
    EEPROM.commit();
}

void showSolidColor() {
    fill_solid(leds, NUM_LEDS, solidColor);
}

void rainbow() {
    // FastLED's built-in rainbow generator
    fill_rainbow( leds, NUM_LEDS, gHue, 10);
}

void rainbowWithGlitter() {
    // built-in FastLED rainbow, plus some random sparkly glitter
    rainbow();
    addGlitter(80);
}

void snowGlitter() {
  bool even;
  if (leds[0].r > 0) {
    even = false;
  }
  else even = true;

  for ( int i = 0; i < NUM_LEDS; i++) {
    if ((i % 2 == 0) && even) {
      leds[i] = CRGB::White;
    }
    else if ((i % 2 != 0) && !even) {
      leds[i] = CRGB::White;
    }
    else {
      leds[i] = CRGB::Black;
    }
  }
  delay(1200);
  
    // Snow glitter, only white LEDs
//    if (patternChanged) {
//        patternChanged = false;
//        // This indicates some other mode was before, so start from beginning
//        fill_solid(leds, NUM_LEDS, CRGB::Black);
//
//        for ( int i = 0; i < NUM_LEDS/3; i++) { //NUM_LEDS truncated
//            int led = random16(0, NUM_LEDS);
//            while (leds[led].r != 0) {
//                led = random16(0, NUM_LEDS); // skip those already used
//            }
//            hsv2rgb_rainbow(CHSV(0, 0, random16(10, 245)), leds[led]); // set light
//        }
//    }
//    for ( int i = 0; i < NUM_LEDS; i++) {
//        if (leds[i].r > 0) {
//            if (leds[i].r == 254) {
//                leds[i].r++;
//                leds[i].g++;
//                leds[i].b++;
//            }
//            else if (leds[i].r == 1) {
//                leds[i].r--;
//                leds[i].g--;
//                leds[i].b--;
//                int led = random16(0, NUM_LEDS);
//                while (leds[led].r != 0) {
//                    led = random16(0, NUM_LEDS); // skip those already used
//                }
//                hsv2rgb_rainbow(CHSV(0, 0, 2), leds[led]);
//            }
//            if (leds[i].r % 2 == 0) {
//                hsv2rgb_rainbow(CHSV(0, 0, leds[i].r+2), leds[i]);
//                //leds[i] = CHSV( 0, 0, leds[i].v+2); // + 2
//            }
//            else { // % 2 == 1
//                hsv2rgb_rainbow(CHSV(0, 0, leds[i].r-2), leds[i]);
//                //leds[i] = CHSV( 0, 0, leds[i].v-2);
//            }
//        }
//    }
}

//void colorRun() {
//    if (patternChanged) {
//        fill_solid(leds, NUM_LEDS, CRGB::Black);
//        fill_solid(leds, 10, solidColor);
//    }
//    else {
//        for ( int i = 0; i < NUM_LEDS; i++) {
//            if (leds[i] == solidColor) {
//                for ( int j = i+1; j < i+10 ; j++) {
//                    leds[j % NUM_LEDS] = solidColor;
//                }
//                
//                break;
//            }
//    }
//}

void addGlitter( fract8 chanceOfGlitter) {
    if ( random8() < chanceOfGlitter) {
        leds[ random16(NUM_LEDS) ] += CRGB::White;
    }
}

void confetti() {
    // random colored speckles that blink in and fade smoothly
    fadeToBlackBy( leds, NUM_LEDS, 10);
    int pos = random16(NUM_LEDS);
    //  leds[pos] += CHSV( gHue + random8(64), 200, 255);
    leds[pos] += ColorFromPalette(palettes[currentPaletteIndex], gHue + random8(64));
}

void sinelon() {
    // a colored dot sweeping back and forth, with fading trails
    fadeToBlackBy( leds, NUM_LEDS, 20);
    int pos = beatsin16(13, 0, NUM_LEDS - 1);
    //  leds[pos] += CHSV( gHue, 255, 192);
    leds[pos] += ColorFromPalette(palettes[currentPaletteIndex], gHue, 192);
}

void bpm() {
    // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
    uint8_t BeatsPerMinute = 62;
    CRGBPalette16 palette = palettes[currentPaletteIndex];
    uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
    for ( int i = 0; i < NUM_LEDS; i++) { //9948
        leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
    }
}

void juggle() {
    // eight colored dots, weaving in and out of sync with each other
    fadeToBlackBy( leds, NUM_LEDS, 20);
    byte dothue = 0;
    for ( int i = 0; i < 8; i++)
    {
        //    leds[beatsin16(i + 7, 0, NUM_LEDS)] |= CHSV(dothue, 200, 255);
        leds[beatsin16(i + 7, 0, NUM_LEDS)] |= ColorFromPalette(palettes[currentPaletteIndex], dothue);
        dothue += 32;
    }
}

// Pride2015 by Mark Kriegsman: https://gist.github.com/kriegsman/964de772d64c502760e5
// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
void pride() {
    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;

    uint8_t sat8 = beatsin88( 87, 220, 250);
    uint8_t brightdepth = beatsin88( 341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = sHue16;//gHue * 256;
    uint16_t hueinc16 = beatsin88(113, 1, 3000);

    uint16_t ms = millis();
    uint16_t deltams = ms - sLastMillis ;
    sLastMillis  = ms;
    sPseudotime += deltams * msmultiplier;
    sHue16 += deltams * beatsin88( 400, 5, 9);
    uint16_t brightnesstheta16 = sPseudotime;

    for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        brightnesstheta16  += brightnessthetainc16;
        uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = CHSV( hue8, sat8, bri8);

        nblend(leds[i], newcolor, 64);
    }
}

// ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
void colorwaves() {
    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;

    // uint8_t sat8 = beatsin88( 87, 220, 250);
    uint8_t brightdepth = beatsin88( 341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = sHue16;//gHue * 256;
    uint16_t hueinc16 = beatsin88(113, 300, 1500);

    uint16_t ms = millis();
    uint16_t deltams = ms - sLastMillis ;
    sLastMillis  = ms;
    sPseudotime += deltams * msmultiplier;
    sHue16 += deltams * beatsin88( 400, 5, 9);
    uint16_t brightnesstheta16 = sPseudotime;

    for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;
        uint16_t h16_128 = hue16 >> 7;
        if ( h16_128 & 0x100) {
            hue8 = 255 - (h16_128 >> 1);
        } else {
            hue8 = h16_128 >> 1;
        }

        brightnesstheta16  += brightnessthetainc16;
        uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        uint8_t index = hue8;
        //index = triwave8( index);
        index = scale8( index, 240);

        CRGB newcolor = ColorFromPalette(gCurrentPalette, index, bri8);

        nblend(leds[i], newcolor, 128);
    }
}
// Alternate rendering function just scrolls the current palette
// across the defined LED strip.
void palettetest() {
    static uint8_t startindex = 0;
    startindex--;
    fill_palette( leds, NUM_LEDS, startindex, (256 / NUM_LEDS) + 1, gCurrentPalette, 255, LINEARBLEND);
}
