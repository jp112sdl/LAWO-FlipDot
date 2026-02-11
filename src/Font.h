// https://pjrp.github.io/MDParolaFontEditor
#ifndef _FONT_H_
#define _FONT_H_

#define FONT_FILE_INDICATOR 'F' ///< Font table indicator prefix for info header
#include "Arduino.h"

class LawoFont {
public:

typedef const uint8_t fontType_t;
typedef struct {
     uint8_t version;     // (v1) font definition version number (for compliance)
     uint8_t height;      // (v1) font height in pixels
     uint8_t widthMax;    // (v1) font maximum width in pixels (widest character)
     uint16_t firstASCII; // (v1,2) the first character code in the font table
     uint16_t lastASCII;  // (v1,2) the last character code in the font table
     uint16_t dataOffset; // (v1) offset from the start of table to first character definition
   } fontInfo_t;

static fontType_t DefaultFont[] PROGMEM;
static fontType_t DefaultFontCondensed[] PROGMEM;
static fontType_t Font5x3[] PROGMEM;
static fontType_t BigFont[] PROGMEM;
static fontType_t BigFontCondensed[] PROGMEM;

// Font related data
fontType_t  *_fontData;   // pointer to the current font data being used
fontInfo_t  _fontInfo;    // properties of the current font table
  

void setFontInfoDefault(void)
// Set the defaults for the info block compatible with version 0 of the file
{
  _fontInfo.version = 0;
  _fontInfo.height = 8;
  _fontInfo.widthMax = 0;
  _fontInfo.firstASCII = 0;
  _fontInfo.lastASCII = 255;
  _fontInfo.dataOffset = 0;
}

uint8_t getFontWidth(void)
{
  uint8_t   max = 0;
  uint8_t   charWidth;
  uint32_t  offset = _fontInfo.dataOffset;

  if (_fontData != nullptr)
  {
    for (uint16_t i = _fontInfo.firstASCII; i <= _fontInfo.lastASCII; i++)
    {
      charWidth = pgm_read_byte(_fontData + offset);
      /*
      PRINT("\nASCII '", i);
      PRINT("' offset ", offset);
      PRINT("' width ", charWidth);
      */
      if (charWidth > max)
      {
        max = charWidth;
      }
      offset += charWidth;  // skip character data
      offset++; // skip to size byte
    }
  }

  return(max);
}

void loadFontInfo(void) {
  uint8_t c;
  uint16_t offset = 0;

  setFontInfoDefault();

  if (_fontData != nullptr) {
    // Read the first character. If this is not the file type indicator
    // then we have a version 0 file and the defaults are ok, otherwise
    // read the font info from the data table.
    c = pgm_read_byte(_fontData + offset++);
    if (c == FONT_FILE_INDICATOR) {
      c = pgm_read_byte(_fontData + offset++);  // read the version number
      switch (c) {
        case 2:
          _fontInfo.firstASCII = (pgm_read_byte(_fontData + offset++) << 8);
          _fontInfo.firstASCII += pgm_read_byte(_fontData + offset++);
          _fontInfo.lastASCII = (pgm_read_byte(_fontData + offset++) << 8);
          _fontInfo.lastASCII += pgm_read_byte(_fontData + offset++);
          _fontInfo.height = pgm_read_byte(_fontData + offset++);
          break;

        case 1:
          _fontInfo.firstASCII = pgm_read_byte(_fontData + offset++);
          _fontInfo.lastASCII  = pgm_read_byte(_fontData + offset++);
          _fontInfo.height     = pgm_read_byte(_fontData + offset++);
          break;

        case 0:
        default:
          // nothing to do, use the library defaults
          break;
      }
      _fontInfo.dataOffset = offset;
    }

    // these always set
    _fontInfo.widthMax = getFontWidth();
  }
  //Serial.print(F("_fontInfo.firstASCII = "));Serial.println(_fontInfo.firstASCII, DEC);
  //Serial.print(F("_fontInfo.lastASCII = "));Serial.println(_fontInfo.lastASCII, DEC);
  //Serial.print(F("_fontInfo.height = "));Serial.println(_fontInfo.height, DEC);
}

void setFont(fontType_t *f) {
  if (f != _fontData) {
    _fontData = (f == nullptr ? DefaultFont : f);
    loadFontInfo();
  }
}

int32_t getFontCharOffset(uint16_t c) {
  int32_t  offset = _fontInfo.dataOffset;
  if (c < _fontInfo.firstASCII || c > _fontInfo.lastASCII)
    offset = -1;
  else {
    for (uint16_t i=_fontInfo.firstASCII; i<c; i++) {
      offset += pgm_read_byte(_fontData+offset);
      offset++; // skip size byte we used above
    }
  }
  return(offset);
}

uint8_t getChar(uint16_t c, uint8_t size, uint8_t *buf)
{
  if (buf == nullptr)
    return(0);

  int32_t offset = getFontCharOffset(c);
  if (offset == -1){
    memset(buf, 0, size);
    size = 0;
  }
  else {
    size = min(size, pgm_read_byte(_fontData+offset));

    offset++; // skip the size byte

    for (uint8_t i=0; i<size; i++)
      *buf++ = pgm_read_byte(_fontData+offset+i);
  }

  return(size);
}

uint8_t getTextWidth(const char * Text) {
  uint8_t buf[24];
  uint8_t w = 0;
  for (uint8_t i = 0; i < strlen(Text); i++) {
    w += getChar(Text[i], sizeof(buf)/sizeof(buf[0]), buf);
    w++;
  }
  return w;
}

uint8_t getCenterPos(const char * Text, uint8_t w) {
  return (w / 2) - (getTextWidth(Text) / 2);
}
};
#endif
