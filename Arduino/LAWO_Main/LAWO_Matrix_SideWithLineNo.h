#include <Arduino.h>

#define MATRIX_WIDTH 112
#define MATRIX_HEIGHT 16
#define PANEL_WIDTH 28

byte PANEL_LINES[4] = {1, 2, 3, 0};

bool hasHalfPanelOffset(byte colIndex) {
  /*
   * Determine if the given column index belongs to a panel which was preceded by a 14-col half panel.
   */

  return false;
}
