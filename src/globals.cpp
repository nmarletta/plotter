#include "globals.h"

State currentState;
SerialManager serialMgr;
String selectedFile = "";

int  g_penDownS   = 100;  // M5 S100 = pen down (grbl-servo fork)
int  g_penUpS     = 800;  // M4 S800 = pen up (grbl-servo fork)
bool g_overwriteS = false;
// int encoderPosition;
// int lastEncoderPosition;
// bool encoderPressed;
// String fileNames[];

// void insertListElement(char list[][30], const char element[], int index, int maxSize) {
//     // Ensure the index is within bounds
//     if (index >= 0 && index < maxSize) {
//         // Copy the element into the specified index of the list
//         strncpy(list[index], element, sizeof(list[index]));
//         // Ensure null termination
//         list[index][sizeof(list[index]) - 1] = '\0';
//     }
// }