// Bad Apple for ESP32 with OLED SSD1306 | 2018 by Hackerspace-FFM.de | MIT-License.
// Ported to Adafruit_SSD1306 + LittleFS.
#include "FS.h"
#include "LittleFS.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "heatshrink_decoder.h"

// Hints:
// * Adjust the display pins / I2C address below
// * After uploading to ESP32, also upload the data folder (containing video.hs)
//   to LittleFS using your LittleFS data upload tool

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#if HEATSHRINK_DYNAMIC_ALLOC
#error HEATSHRINK_DYNAMIC_ALLOC must be false for static allocation test suite.
#endif

static heatshrink_decoder hsd;

// global storage for putPixels
int16_t curr_x = 0;
int16_t curr_y = 0;

// global storage for decodeRLE
int32_t runlength = -1;
int32_t c_to_dup = -1;

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

uint32_t lastRefresh = 0;

void putPixels(uint8_t c, int32_t len) {
  uint8_t b = 0;
  while(len--) {
    b = 128;
    for(int i=0; i<8; i++) {
      uint16_t color = (c & b) ? SSD1306_WHITE : SSD1306_BLACK;
      b >>= 1;
      display.drawPixel(curr_x, curr_y, color);
      curr_x++;
      if(curr_x >= 128) {
        curr_x = 0;
        curr_y++;
        if(curr_y >= 64) {
          curr_y = 0;
          display.display();
          //display.clearDisplay();
          // 30 fps target rate
          if(digitalRead(0)) while((millis() - lastRefresh) < 33) ;
          lastRefresh = millis();
        }
      }
    }
  }
}

void decodeRLE(uint8_t c) {
    if(c_to_dup == -1) {
      if((c == 0x55) || (c == 0xaa)) {
        c_to_dup = c;
      } else {
        putPixels(c, 1);
      }
    } else {
      if(runlength == -1) {
        if(c == 0) {
          putPixels(c_to_dup & 0xff, 1);
          c_to_dup = -1;
        } else if((c & 0x80) == 0) {
          if(c_to_dup == 0x55) {
            putPixels(0, c);
          } else {
            putPixels(255, c);
          }
          c_to_dup = -1;
        } else {
          runlength = c & 0x7f;
        }
      } else {
        runlength = runlength | (c << 7);
        if(c_to_dup == 0x55) {
          putPixels(0, runlength);
        } else {
          putPixels(255, runlength);
        }
        c_to_dup = -1;
        runlength = -1;
      }
    }
}

#define RLEBUFSIZE 4096
#define READBUFSIZE 2048
void readFile(fs::FS &fs, const char * path){
    static uint8_t rle_buf[RLEBUFSIZE];
    size_t rle_bufhead = 0;
    size_t rle_size = 0;

    size_t filelen = 0;
    size_t filesize;
    static uint8_t compbuf[READBUFSIZE];

    Serial.printf("Reading file: %s\n", path);
    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("Failed to open file for reading");
        display.clearDisplay();
        display.setCursor(0, 10);
        display.print("File open error.");
        display.display();
        return;
    }
    filelen = file.size();
    filesize = filelen;
    Serial.printf("File size: %d\n", filelen);

    // init display, putPixels and decodeRLE
    display.clearDisplay();
    display.display();
    curr_x = 0;
    curr_y = 0;
    runlength = -1;
    c_to_dup = -1;
    lastRefresh = millis();

    // init decoder
    heatshrink_decoder_reset(&hsd);
    size_t   count  = 0;
    uint32_t sunk   = 0;
    size_t toRead;
    size_t toSink = 0;
    uint32_t sinkHead = 0;

    // Go through file...
    while(filelen) {
      if(toSink == 0) {
        toRead = filelen;
        if(toRead > READBUFSIZE) toRead = READBUFSIZE;
        file.read(compbuf, toRead);
        filelen -= toRead;
        toSink = toRead;
        sinkHead = 0;
      }

      // uncompress buffer
      HSD_sink_res sres;
      sres = heatshrink_decoder_sink(&hsd, &compbuf[sinkHead], toSink, &count);
      //Serial.print("^^ sinked ");
      //Serial.println(count);
      toSink -= count;
      sinkHead = count;
      sunk += count;
      if (sunk == filesize) {
        heatshrink_decoder_finish(&hsd);
      }

      HSD_poll_res pres;
      do {
          rle_size = 0;
          pres = heatshrink_decoder_poll(&hsd, rle_buf, RLEBUFSIZE, &rle_size);
          //Serial.print("^^ polled ");
          //Serial.println(rle_size);
          if(pres < 0) {
            Serial.print("POLL ERR! ");
            Serial.println(pres);
            return;
          }

          rle_bufhead = 0;
          while(rle_size) {
            rle_size--;
            if(rle_bufhead >= RLEBUFSIZE) {
              Serial.println("RLE_SIZE ERR!");
              return;
            }
            decodeRLE(rle_buf[rle_bufhead++]);
          }
      } while (pres == HSDR_POLL_MORE);
    }
    file.close();
    Serial.println("Done.");
}

void setup(){
    Serial.begin(115200);
    Wire.begin(21, 22);  // SDA, SCL
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Mounting LittleFS...");
    display.display();

    if(!LittleFS.begin()){
        Serial.println("LittleFS mount failed");
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("LittleFS mount failed.");
        display.println("Upload video.hs.");
        display.display();
        return;
    }

    pinMode(0, INPUT_PULLUP);
    Serial.print("totalBytes(): ");
    Serial.println(LittleFS.totalBytes());
    Serial.print("usedBytes(): ");
    Serial.println(LittleFS.usedBytes());
    listDir(LittleFS, "/", 0);
    readFile(LittleFS, "/video.hs");
    Serial.println("Done.");

    //Serial.print("Format LittleFS? (enter y for yes): ");
    // while(!Serial.available()) ;
    //if(Serial.read() == 'y') {
    //  bool ret = LittleFS.format();
    //  if(ret) Serial.println("Success. "); else Serial.println("FAILED! ");
    //} else {
    //  Serial.println("Aborted.");
    //}
}

void loop(){

}
