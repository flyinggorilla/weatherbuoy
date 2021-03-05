#ifndef MAIN_ESPUTIL_H_
#define MAIN_ESPUTIL_H_

/* The temperature sensor has a range of -40C to 125C.
 * The absolute sensor results vary by chip. User calibration increases precision.
 */

float esp32_temperature();

const char* esp32_getresetreasontext(int reason);

#endif 
