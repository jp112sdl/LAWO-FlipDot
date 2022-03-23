/*
 * LAWO_Control.h
 *
 *  Created on: 21 Apr 2020
 *      Author: pechj
 */

// Copyright (C) 2016 Julian Metzler
// modified in 2020 by Jérôme Pech

/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef LAWO_CONTROL_H_
#define LAWO_CONTROL_H_

#include "LAWO_MCP23017.h"

#include "Icons.h"
#include "Font.h"

#define ADDRESS_ROW_MCP_Y     0x20
#define ADDRESS_ROW_MCP_B     0x21
#define ADDR_ROW_MCP_BROSE    0x23

#define FLIP_DURATION       2 // in microseconds
#define FLIP_PAUSE_DURATION 1 // in microseconds

#ifndef VIRTUAL_WIDTH
#define VIRTUAL_WIDTH (MATRIX_WIDTH*1)
#endif

enum pxStates {
  BLACK  = 0,
  YELLOW = 1
};

enum flipspeed {
  FS_NORMAL = 250,
  FS_SLOW   = 500
};

#ifndef _swap_int16_t
#define _swap_int16_t(a, b)                                                    \
  {                                                                            \
    int16_t t = a;                                                             \
    a = b;                                                                     \
    b = t;                                                                     \
  }
#endif

template<const uint8_t * COLUMN_LINES,const uint8_t * E_LINES, const uint8_t NUM_E_LINE_PINS, const uint8_t D, const uint8_t MCP_RESET = 0, const uint8_t LED = 0>
class LAWODisplay {
private:
  LawoFont lawoFont;
  MCP23017Type<ADDRESS_ROW_MCP_Y> ROW_MCP_Y;
  MCP23017Type<ADDRESS_ROW_MCP_B> ROW_MCP_B;
#ifdef USE_BROSE
  MCP23017Type<ADDR_ROW_MCP_BROSE> ROW_MCP_BROSE;
#endif
  uint16_t flipSpeedFactor;
  uint8_t lastColIndex;
  uint8_t lastRowIndex;
  uint8_t lastState;
  uint8_t charSpacing;
  bool initOK;
  bool mcp_ok;
  uint32_t PixelState[VIRTUAL_WIDTH];     //uint32_t 32bit represent up to 32 rows
  uint32_t NextPixelState[VIRTUAL_WIDTH]; //uint32_t 32bit represent up to 32 rows
private:
  void initPins() {
    if (MCP_RESET != 0)
      pinMode(MCP_RESET, OUTPUT);
    
    for (uint8_t i = 0; i < 5; i++) {
      pinMode(COLUMN_LINES[i], OUTPUT);
    }
    
    for (uint8_t i = 0; i< NUM_E_LINE_PINS; i++) {
      pinMode(E_LINES[i], OUTPUT);
    }    
    
    pinMode(D, OUTPUT);
    
    if (LED != 0)
      pinMode(LED, OUTPUT);
  }

  bool initMCP() {
    if (MCP_RESET != 0) {
      //strobe RESET to reset the chip
      digitalWrite(MCP_RESET, LOW);
      delay(2);
      digitalWrite(MCP_RESET, HIGH);
      delay(25);
    }
    //this initializes the MCP, sets all pins to OUTPUT and puts state LOW
    bool mcp_y_ok = ROW_MCP_Y.init();
    bool mcp_b_ok = ROW_MCP_B.init();
    mcp_ok = (mcp_y_ok && mcp_b_ok) ;

#ifdef USE_BROSE
    bool mcp_brose_ok = ROW_MCP_BROSE.init();
    mcp_ok = (mcp_ok && mcp_brose_ok);
#endif

    if (!mcp_ok) {
      Serial.println("Error while initializing MCP23017");
      return false;
    }
    return true;
  }

  void selectColumn(byte colIndex) {
    /*
     * Select the appropriate panel for the specified column index and set the column address pins accordingly.
     */
    if (initOK) {

#ifdef COL_SWAP
      colIndex = PANEL_WIDTH - colIndex - 1;
#endif

      // In the case of a matrix with a 14-col panel at the end instead of a 28-col one, we need to remember that our panel index is off by half a panel, so flip the MSB
      bool halfPanelOffset = hasHalfPanelOffset(colIndex);


      // Additionally, the address needs to be reversed because of how the panels are connected
      colIndex = MATRIX_WIDTH - colIndex - 1;

      // Since addresses start from the beginning in every panel, we need to wrap around after reaching the end of a panel
      byte address = colIndex % PANEL_WIDTH;

      // A quirk of the FP2800 chip used to drive the columns is that addresses divisible by 8 are not used, so we need to skip those
      address += (address / 7) + 1;

      digitalWrite(COLUMN_LINES[0], address & 1);
      digitalWrite(COLUMN_LINES[1], address & 2);
      digitalWrite(COLUMN_LINES[2], address & 4);
      digitalWrite(COLUMN_LINES[3], address & 8);
      digitalWrite(COLUMN_LINES[4], halfPanelOffset ? !(address & 16) : address & 16);

    }
  }

  void selectRow(byte rowIndex, bool yellow) {
    if (initOK && rowIndex < MATRIX_HEIGHT) {
#ifdef ROW_SWAP
      rowIndex = MATRIX_HEIGHT - rowIndex - 1;
#endif
      ROW_MCP_Y.writeAll(0);
      ROW_MCP_B.writeAll(0);
#ifdef USE_BROSE
      ROW_MCP_BROSE.writeAll(0);
#endif
      if (rowIndex < 16) {
        if (yellow)
          ROW_MCP_Y.writePin(rowIndex, HIGH);
        else
          ROW_MCP_B.writePin(rowIndex, HIGH);
      }
#ifdef USE_BROSE
      else {
         uint8_t pin = rowIndex - 16;
         ROW_MCP_BROSE.writePin(yellow ? pin + 8 : pin , HIGH);
      }
#endif

      digitalWrite(D, !yellow);
    }
  }

  void flip(byte col) {

    /*
     * Send an impulse to the specified panel to flip the currently selected dot.
     */

    // Get the enable line for the specified panel
    if (initOK) {

      byte e = E_LINES[PANEL_LINES[col / PANEL_WIDTH]];

      digitalWrite(e, HIGH);
      delayMicroseconds(FLIP_DURATION * flipSpeedFactor);

      digitalWrite(e, LOW);
      delayMicroseconds(FLIP_PAUSE_DURATION * flipSpeedFactor);
    }

    yield();
  }

  void deselect() {
    if (initOK) {
      digitalWrite(COLUMN_LINES[0], LOW);
      digitalWrite(COLUMN_LINES[1], LOW);
      digitalWrite(COLUMN_LINES[2], LOW);
      digitalWrite(COLUMN_LINES[3], LOW);
      digitalWrite(COLUMN_LINES[4], LOW);

      ROW_MCP_Y.writeAll(LOW);
      ROW_MCP_B.writeAll(LOW);
#ifdef USE_BROSE
      ROW_MCP_BROSE.writeAll(LOW);
#endif
    }
  }

  void setPixelPhysical() {
    //unsigned long start = millis();
    for (uint16_t c = 0; c < VIRTUAL_WIDTH; c++) {
      for (uint8_t r = 0; r < MATRIX_HEIGHT; r++) {
        bool currentPixelState = bitRead(PixelState[c], r);
        bool nextPixelState = bitRead(NextPixelState[c], r);

        if (currentPixelState != nextPixelState) {
        bitWrite(PixelState[c], r, nextPixelState);

        if (c < MATRIX_WIDTH) {
          selectRow(r, nextPixelState);
          selectColumn(c);
          flip(c);
        }
       }
      }
    }
    deselect();
    //Serial.print("setPixelPhysical():");Serial.print(millis() - start,DEC);Serial.println("ms");
  }

  void setPixel(byte colIndex, byte rowIndex, bool state) {
      bitWrite(NextPixelState[colIndex], rowIndex, state);
  }

  void setFlipSpeed(uint16_t s) {
    flipSpeedFactor = s;
  }

  void drawLineH(uint8_t col, uint8_t row, uint8_t width, bool state) {
    //Serial.print("drawLineV col: "); Serial.print(col, DEC);Serial.print(", row: ");Serial.print(row, DEC);Serial.print(", width: ");Serial.println(width);
    for (uint8_t i = 0; i < width; i++) {
      setPixel(col+i, row, state);
    }
  }

  void drawLineV(uint8_t col, uint8_t row, uint8_t height, bool state) {
    //Serial.print("drawLineH col: "); Serial.print(col, DEC);Serial.print(", row: ");Serial.print(row, DEC);Serial.print(", height: ");Serial.println(height);
    for (uint8_t i = 0; i < height; i++) {
      setPixel(col, row + i, state);
    }
  }

  void writeLine(uint8_t x0, uint8_t y0,  uint8_t x1, uint8_t y1, bool state) {
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
     _swap_int16_t(x0, y0);
     _swap_int16_t(x1, y1);
    }

    if (x0 > x1) {
      _swap_int16_t(x0, x1);
      _swap_int16_t(y0, y1);
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int16_t err = dx / 2;
    int16_t ystep;

    if (y0 < y1) {
      ystep = 1;
    } else {
      ystep = -1;
    }

    for (; x0 <= x1; x0++) {
      if (steep) {
        drawPixel(y0, x0, state);
      } else {
        drawPixel(x0, y0, state);
      }
      err -= dy;
      if (err < 0) {
        y0 += ystep;
        err += dx;
      }
    }
  }

  void drawBitmap(uint8_t IconIndex, uint8_t col_offset, uint8_t row_offset, bool state, bool invert) {
    //The first half of the icon bytes (i.e. byte 0...23 for an icon width of 24px (which has total 48 Bytes) represent the upper 8 rows
    //The second half of the icon bytes (24...47) represent the lower 8 rows
    uint8_t iconWidth = Icons[IconIndex].width;
    uint8_t iconHeight = Icons[IconIndex].height;
    const uint8_t *icondata =  Icons[IconIndex].Icon;

    for (uint8_t i = 0 ; i < iconWidth; i++) {

      uint8_t rowscan = 8;
      if (iconHeight > 8) rowscan = iconHeight / 2;

      for (uint8_t j = 0; j < rowscan; j++) {
        //Rows 0 - 7
        byte data = pgm_read_byte(&icondata[i]);
        bool bit = bitRead(data, j) == invert ? 1 : 0;
        if (bit)
         drawPixel(i+col_offset, j + row_offset, state);
       if (iconHeight > 8) {
          //Rows 8 - 15
          data = pgm_read_byte(&icondata[i+iconWidth]);
          bit = bitRead(data, j) == invert ? 1 : 0;
          if (bit)
            drawPixel(i+col_offset, j + row_offset + rowscan, state);
        }
     }

    }
  }

public:
  LAWODisplay () : flipSpeedFactor(FS_NORMAL), lastColIndex(0xff), lastRowIndex(0xff), lastState(0), charSpacing(1), initOK(false), mcp_ok(false) {}
  virtual ~LAWODisplay() {}

  void setLED(bool state) {
  if (LED != 0)
    digitalWrite(LED, (state == 1) ? LOW : HIGH);
  }
  
  bool init() {
    lawoFont.setFont(LawoFont::DefaultFont);
    initIcons();
    initPins();
    setLED(0);

    initOK = initMCP();

    Serial.print("LAWODisplay Init ");Serial.println(initOK ? "OK":"ERROR");

    return initOK;
  }

  void clear(bool withYellow = true) {
    if (withYellow) {
      for (byte col = 0; col < MATRIX_WIDTH; col++) {
        selectColumn(col);
        for (int row = 0; row < MATRIX_HEIGHT; row++) {
          selectRow(MATRIX_HEIGHT - row - 1, 1);
          flip(col);
        }
      }

      memset(PixelState, 0xffffffff, MATRIX_WIDTH * sizeof(uint32_t));
      delay(200);
    }

    for (byte col = 0; col < MATRIX_WIDTH; col++) {
      selectColumn(col);
      for (int row = 0; row < MATRIX_HEIGHT; row++) {
        selectRow(MATRIX_HEIGHT - row - 1, 0);
        flip(col);
      }
    }

    deselect();

    memset(PixelState, 0, MATRIX_WIDTH * sizeof(uint32_t));
  }

  void black() {
    memset(NextPixelState, 0, MATRIX_WIDTH * sizeof(uint32_t));
  }

  void yellow() {
    memset(NextPixelState, 0xffffffff, MATRIX_WIDTH * sizeof(uint32_t));
  }

  void drawPixel(uint8_t col, uint8_t row, bool state = YELLOW) {
    setPixel(col, row, state);
  }
  
  void repairPixel(uint8_t col, uint8_t row) {
    uint16_t s = flipSpeedFactor;
    setFlipSpeed(FS_SLOW);
    bool currentState = getPixelState(col, row);
    for (uint8_t i = 0; i < 20; i++) {
      drawPixel(col, row, YELLOW);
      show();
      delay(200);
      drawPixel(col, row, BLACK);
      show();
      delay(200);
    }
    setFlipSpeed(s);
    drawPixel(col, row, currentState);
    show();
  }

  void repairColumn(uint8_t col) {
    uint16_t s = flipSpeedFactor;
    setFlipSpeed(FS_SLOW);
    bool currentState[MATRIX_HEIGHT];
    for (uint8_t i = 0; i < MATRIX_HEIGHT; i++)
      currentState[i]= getPixelState(col, i);


    for (uint8_t i = 0; i < 20; i++) {
      for (uint8_t j = 0; j < MATRIX_HEIGHT; j++)
        drawPixel(col, j, YELLOW);
      show();
      delay(200);
      for (uint8_t j = 0; j < MATRIX_HEIGHT; j++)
      drawPixel(col, j, BLACK);
      show();
      delay(200);
    }
    setFlipSpeed(s);
    for (uint8_t j = 0; j < MATRIX_HEIGHT; j++)
      drawPixel(col, j, currentState[j]);
    show();
  }

  void refreshAllPixel() {
    uint32_t current[VIRTUAL_WIDTH];
    memcpy(current, PixelState, VIRTUAL_WIDTH);
    clear();
    memcpy(NextPixelState, current, VIRTUAL_WIDTH);
    show();
  }

  uint8_t printChar(uint8_t col_offset, uint8_t row_offset, uint8_t chr, bool state) {
    uint8_t   cBuf[24];
    uint8_t   dBuf[24];

    const uint8_t dbl_offset = 128;
    bool doubleHeight = (lawoFont._fontInfo.height == 16);

    uint8_t charWidth = lawoFont.getChar(chr, sizeof(cBuf)/sizeof(cBuf[0]), cBuf);

    if (doubleHeight) lawoFont.getChar(chr + dbl_offset, sizeof(dBuf)/sizeof(dBuf[0]), dBuf);

    //Serial.print("charWidth = ");Serial.println(charWidth, DEC);

    for (uint8_t col = 0 ; col < charWidth; col++) {
      for (uint8_t row = 0; row < (lawoFont._fontInfo.height <= 8 ? lawoFont._fontInfo.height : 8); row++) {

        bool bit = bitRead(doubleHeight ? dBuf[col] : cBuf[col], row);
        if (bit) {
          setPixel(col + col_offset, row + row_offset, (bit == state));
        }

        if (doubleHeight) {
          bit = bitRead(cBuf[col], row);
          if (bit) {
            setPixel(col + col_offset, row + row_offset + 8, (bit == state));
          }
        }
      }
    }

    return charWidth;
  }

  void setCharSpacing(uint8_t s) {
    charSpacing = s;
  }

  uint16_t print(byte X, byte Y, const char * Text, bool state = YELLOW) {
    uint8_t charPos = X;
    for (uint8_t i = 0; i < strlen(Text); i++) {
      uint8_t lastCharWidth = printChar(charPos, Y, Text[i], state);
      charPos += lastCharWidth;
      charPos += charSpacing;
    }
    return charPos;
  }
  
  void print(byte X, byte Y, String Text) {
    print(X, Y, Text.c_str());
  }


  void setFont(LawoFont::fontType_t *f) {
    lawoFont.setFont(f);
  }

  uint8_t getCenterPos(const char * Text, uint8_t w = MATRIX_WIDTH) {
    return lawoFont.getCenterPos(Text, w);
  }

  void drawRect(uint8_t col, uint8_t row,  uint8_t width, uint8_t height, bool state = YELLOW) {
    drawLineV(col, row,          width, state);
    drawLineV(col, row + height - 1, width, state);
    drawLineH(col,             row, height-1, state);
    drawLineH(col + width -1 , row, height-1, state);
  }

  void fillRect(uint8_t col, uint8_t row,  uint8_t width, uint8_t height, bool state = YELLOW) {
    for (uint8_t i = 0; i < width; i++) {
      drawLineH(col+i, row, height, state);
    }
  }

  void drawLine(uint8_t x0, uint8_t y0,  uint8_t x1, uint8_t y1, bool state = YELLOW) {
    if (x0 == x1) {
      if (y0 > y1)
        _swap_int16_t(y0, y1);
      drawLineV(x0, y0, y1 - y0, state);
    } else if (y0 == y1) {
      if (x0 > x1)
        _swap_int16_t(x0, x1);
      drawLineH(x0, y0, x1 - x0, state);
    } else {
      writeLine(x0, y0, x1, y1, state);
    }
  }

  void drawIcon(uint8_t IconIndex, uint8_t col_offset, uint8_t row_offset, bool state = YELLOW) {
    drawBitmap(IconIndex, col_offset, row_offset, state, false);
  }

  void drawIcon(const uint8_t *icondata, uint8_t iconWidth, uint8_t iconHeight, uint8_t col_offset, uint8_t row_offset, bool state, bool invert) {
    //The first half of the icon bytes (i.e. byte 0...23 for an icon width of 24px (which has total 48 Bytes) represent the upper 8 rows
    //The second half of the icon bytes (24...47) represent the lower 8 rows
    for (uint8_t i = 0 ; i < iconWidth; i++) {

      uint8_t rowscan = 8;
      if (iconHeight > 8) rowscan = iconHeight / 2;

      for (uint8_t j = 0; j < rowscan; j++) {
        //Rows 0 - 7
        byte data = pgm_read_byte(&icondata[i]);
        bool bit = bitRead(data, j) == invert ? 1 : 0;
        if (bit)
         drawPixel(i+col_offset, j + row_offset, state);
       if (iconHeight > 8) {
          //Rows 8 - 15
          data = pgm_read_byte(&icondata[i+iconWidth]);
          bit = bitRead(data, j) == invert ? 1 : 0;
          if (bit)
            drawPixel(i+col_offset, j + row_offset + rowscan, state);
        }
     }
    }
  }

  void drawIconInvert(uint8_t IconIndex, uint8_t col_offset, uint8_t row_offset, bool state = YELLOW) {
    drawBitmap(IconIndex, col_offset, row_offset, state, true);
  }

  void drawCircle(int16_t x0, int16_t y0, int16_t r, bool state = YELLOW) {
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    drawPixel(x0, y0 + r, state);
    drawPixel(x0, y0 - r, state);
    drawPixel(x0 + r, y0, state);
    drawPixel(x0 - r, y0, state);

    while (x < y) {
      if (f >= 0) {
        y--;
        ddF_y += 2;
        f += ddF_y;
      }
      x++;
      ddF_x += 2;
      f += ddF_x;

      drawPixel(x0 + x, y0 + y, state);
      drawPixel(x0 - x, y0 + y, state);
      drawPixel(x0 + x, y0 - y, state);
      drawPixel(x0 - x, y0 - y, state);
      drawPixel(x0 + y, y0 + x, state);
      drawPixel(x0 - y, y0 + x, state);
      drawPixel(x0 + y, y0 - x, state);
      drawPixel(x0 - y, y0 - x, state);
    }
  }

  void moveRight(bool invert = false) {
    uint32_t temp[VIRTUAL_WIDTH];
    temp[0] = invert ? 0xffff : 0;
    for (uint16_t c = 0; c < VIRTUAL_WIDTH - 1; c++) {
      temp[c+1] = PixelState[c];
    }
    setPixelMap(temp);
  }

  void moveLeft(bool invert = false) {
    uint32_t temp[VIRTUAL_WIDTH];
    temp[VIRTUAL_WIDTH-1] = invert ? 0xffff : 0;
    for (uint16_t c = 0; c < VIRTUAL_WIDTH - 1; c++) {
      temp[c] = PixelState[c+1];
    }
    setPixelMap(temp);
  }

  void moveUp(bool invert = false) {
    uint32_t temp[VIRTUAL_WIDTH];
    for (uint16_t c = 0; c < VIRTUAL_WIDTH; c++) {
      temp[c] = PixelState[c] >> 1;
      if (invert) bitSet(temp[c],15);
    }
    setPixelMap(temp);
  }

  void moveDown(bool invert = false) {
    uint32_t temp[VIRTUAL_WIDTH];
    for (uint16_t c = 0; c < VIRTUAL_WIDTH; c++) {
      temp[c] = PixelState[c] << 1;
      if (invert) bitSet(temp[c],0);
    }
    setPixelMap(temp);
  }

  bool getPixelState(uint8_t col, uint8_t row) {
    return bitRead(PixelState[col], row);
  }

  uint32_t getColumn(uint8_t col) {
    return PixelState[col];
  }

  uint32_t getRow(uint8_t row) {
    uint32_t i = 0;
    for (uint16_t c = 0; c < VIRTUAL_WIDTH; c++) {
      i |= bitRead(PixelState[c],row)<<c;
    }

    return i;
  }

  void getPixMap(uint32_t dstMap[VIRTUAL_WIDTH]) {
    memcpy(dstMap, PixelState, VIRTUAL_WIDTH);
  }

  String dumpPixMap(bool asIconArray = false) {
    const uint16_t num_columns = VIRTUAL_WIDTH;
    const uint8_t num_rows = MATRIX_HEIGHT;

    if (asIconArray == true) {
      String s = "";

      for (uint8_t c = 0; c < num_columns; c++) {
        if (c % 7 == 0) s+= "\n";
          s+="0x";
          s+= String(PixelState[c],HEX);
          if (c < num_columns -1) s+=", ";
      }
      s+="\n";
      return String(s);

    } else {
    unsigned int c_len = 0;
    unsigned char i, j, k;
    uint8_t num_modules = sizeof(PANEL_LINES);
    for (j = 0; j < num_modules; ++j) {
      c_len += num_columns;
    }
    c_len = ((c_len + 10) * (num_rows + 3)) + 1;

    char *c = (char *)malloc(c_len);
    c_len = 0;

    for (i = 0; i < num_rows; ++i) {
      c_len += sprintf(c + c_len, "\n%2u ", i);
      for (j = 0; j < num_modules; ++j) {
        for (k = 0; k < num_columns; ++k) {
          c[c_len++] = (bitRead(PixelState[k],i) == 1) ? '@' : '.';
        }
      }
    }
    c[c_len++] = '\n';
    c[c_len] = 0;
    String s(c);
    free(c);
    return s;
    }
  }

  uint32_t * get_dots() {
    return PixelState;
  }

  void show() {
    setPixelPhysical();
  }

  void invert() {
    for (uint16_t c = 0; c < VIRTUAL_WIDTH; c++) {
      for (uint8_t r = 0; r < MATRIX_HEIGHT; r++) {
        bool currentPixelState = bitRead(PixelState[c],r);
        setPixel(c, r, !currentPixelState);
      }
    }
  }

  void setPixelMap(const uint32_t srcMap[VIRTUAL_WIDTH]) {
    for (uint16_t c = 0; c < VIRTUAL_WIDTH; c++) {
      for (uint8_t r = 0; r < MATRIX_HEIGHT; r++) {
        bool newPixelState = bitRead(srcMap[c],r);
        bool currentPixelState = bitRead(PixelState[c],r);

        if (newPixelState != currentPixelState) {
          setPixel(c, r, newPixelState);
        }
      }
    }
    //setPixelPhysical();
  }
};

#endif /* LAWO_CONTROL_H_ */
