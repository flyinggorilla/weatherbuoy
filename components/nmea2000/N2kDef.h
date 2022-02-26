/*
N2kDef.h

Copyright (c) 2015-2021 Timo Lappalainen, Kave Oy, www.kave.fi

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Type definitions and utility macros used in the NMEA2000 libraries.

*/

#ifndef _tN2kDef_H_
#define _tN2kDef_H_

#include <stdint.h>

extern "C"
{
    // Application execution delay. Must be implemented by application.
    extern void delay(uint32_t ms);

    // Current uptime in milliseconds. Must be implemented by application.
    extern uint32_t millis();
}

// Declare PROGMEM macros to nothing on non-AVR targets.
#define PROGMEM
//#define pgm_read_byte(var)  *var
//#define pgm_read_word(var)  *var
//#define pgm_read_dword(var) *var
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))

// Definition for the F(str) macro. On Arduinos use what the framework
// provides to utilize the Stream class. On standard AVR8 we declare
// our own helper class which is handled by the N2kStream. On anything
// else we resort to char strings.
#ifndef F
#define F(str) str
#endif

#endif
