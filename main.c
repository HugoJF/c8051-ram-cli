#include <stdio.h>
#include "config.c"
#include "def_pinos.h"

// Virtual switch bounce emulation
#define BOUNCE(X) X=0;delay(5);X=1;delay(5);X=0;delay(150);X=1;delay(5);X=0;delay(5);X=1;

// Keyboard key to uC pin
#define KP0 P3_0
#define KP1 P3_1
#define KP2 P3_2
#define KP3 P3_3
#define KP4 P3_4
#define KP5 P3_5
#define KP6 P3_6
#define KP7 P3_7
#define KP8 P2_4
#define KP9 P2_5
#define KPE P2_6
#define KPC P2_7

// Keyboard key flags
#define K0 0
#define K1 1
#define K2 2
#define K3 3
#define K4 4
#define K5 5
#define K6 6
#define K7 7
#define K8 8
#define K9 9
#define KE 10
#define KC 11
	
// No keys pressed flag
#define NULL_KEY 50

// RAM module chip-select
#define CS P2_3

// RAM addresses
#define RAM_SIZE 1<<13
#define RAM_WRITE_INSTRUCTION 0x02
#define RAM_READ_INSTRUCTION 0x03

// Last key that was pressed
volatile unsigned char keypress = NULL_KEY;

unsigned char read_keyboard(void) {
    unsigned char i, keys_2, keys_3;

    // flips bits so a pressed key shows as 1
    keys_2 = ~P2;
    keys_3 = ~P3;

    // tries to find a pressed P3_x key
    for (i = 0; i <= 7; ++i) {
        if (((keys_3 >> i) & 0x01) == 1) {
            return i;
        }
    }

    // tries to find a pressed P2_x key
    for (i = 4; i <= 7; ++i) {
        if (((keys_2 >> i) & 0x01) == 1) {
            // offset P2_x keys since 0-7 are reserved to P3_x
            return 4 + i;
        }
    }

    // if codes reaches this, no key was pressed
    return NULL_KEY;
}


void delay(unsigned int ms) {
    TMOD |= 0x01;
    TMOD &= ~0x02;

    while (ms-- > 0) {
        TR0 = 0;
        TF0 = 0;
        TL0 = 0x58;
        TH0 = 0x9e;
        TR0 = 1;
        while (TF0 == 0);
    }
}

void int_serial(void) __interrupt 4 {
    if (RI0 == 1) {
        switch (SBUF0) {
            case '0': BOUNCE(KP0); break;
            case '1': BOUNCE(KP1); break;
            case '2': BOUNCE(KP2); break;
            case '3': BOUNCE(KP3); break;
            case '4': BOUNCE(KP4); break;
            case '5': BOUNCE(KP5); break;
            case '6': BOUNCE(KP6); break;
            case '7': BOUNCE(KP7); break;
            case '8': BOUNCE(KP8); break;
            case '9': BOUNCE(KP9); break;
            case 'e': BOUNCE(KPE); break;
            case 'c': BOUNCE(KPC); break;
            default: break; 
        }
        RI0=0;
    }
}

void int_keys (void) __interrupt 5 {
    static unsigned char previous_key = NULL_KEY;
    static unsigned char current_key = NULL_KEY;

    TF2 = 0;

    previous_key = current_key;
    current_key = read_keyboard();

    if (previous_key == NULL_KEY && current_key != NULL_KEY) {
        keypress = current_key;
    }
}

unsigned char read_ram(unsigned short address) {
    unsigned char address_low = address;
    unsigned char address_high = address >> 8;

    // execute byte read sequence (specified in the datasheet)
    CS = 0; // enable RAM
    SPI0DAT = RAM_READ_INSTRUCTION; // send read instruction
    while (!TXBMT); // wait SPI0 buffer to get empty
    SPI0DAT = address_high;
    while (!TXBMT);
    SPI0DAT = address_low;
    while (!TXBMT);
    SPI0DAT = 0x00; // write 8 bits (either 1s or 0s) during which data will be received
    while (!TXBMT);
    SPIF = 0; // reset SPI0 interrupt flag
    while (!SPIF); // wait for end of transmission interrupt
    SPIF = 0; // reset it again
    CS = 1; // disable ram

    return SPI0DAT; // return the data sent by the RAM
}

void write_ram(unsigned int address, unsigned short data) {
    unsigned char address_low = address;
    unsigned char address_high = address >> 8;

    // execute byte write sequence (specified in the datasheet)
    CS = 0; // enable RAM
    SPI0DAT = RAM_WRITE_INSTRUCTION; // send write instruction
    while (!TXBMT); // wait SPI0 buffer to get empty
    SPI0DAT = address_high;
    while (!TXBMT);
    SPI0DAT = address_low;
    while (!TXBMT);
    SPI0DAT = data;
    while (!TXBMT);
    SPIF = 0; // reset SPI0 interrupt flag
    while (!SPIF); // wait for end of transmission interrupt
    SPIF = 0; // reset it again
    CS = 1; // disable ram
}

// handles printf
void putchar (unsigned char c) {
    SBUF0 = c;
    while (TI0 == 0);
    TI0 = 0;
}

void main (void) {
    unsigned int i = 0;
    unsigned char t = 0;

    Init_Device();
    SFRPAGE = LEGACY_PAGE;
    keypress = NULL_KEY;

    printf_fast_f("TOP10\n");

    while (1)
    {
        if (keypress == NULL_KEY) {
            continue;
        }

        // if numeric key
        if (keypress <= K9) {
            printf_fast_f("Numeric key: %c\n", keypress + 48);
        }

        if (keypress == KE) {
            printf_fast_f("Enter\n");
        }

        if (keypress == KC) {
            printf_fast_f("Clear\n");
        }

        keypress = NULL_KEY;
        // write_ram(i, (unsigned char) i);
        // printf_fast_f("[%d] = %d\n", i, read_ram(i));
    }
}