#include <stdio.h>
#include "config.c"
#include "def_pinos.h"

// Virtual switch bounce emulation
#define BOUNCE(X) X=0;delay(5);X=1;delay(5);X=0;delay(150);X=1;delay(5);X=0;delay(5);X=1;break;

// Macros to make code more readable
#define true 1
#define false 0
#define OP_WRITE 0
#define OP_READ 1

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
#define KEY_0 0
#define KEY_1 1
#define KEY_2 2
#define KEY_3 3
#define KEY_4 4
#define KEY_5 5
#define KEY_6 6
#define KEY_7 7
#define KEY_8 8
#define KEY_9 9
#define KEY_E 10
#define KEY_C 11
	
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

unsigned char key_to_ascii(unsigned char key) {
    // if numeric key
    if (key <= KEY_9) {
        return key + 48;
    }

    if (key == KEY_E) {
        return 'e';
    }

    if (key == KEY_C) {
        return 'c';
    }

    return '\0';
}

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
            case '0': BOUNCE(KP0);
            case '1': BOUNCE(KP1);
            case '2': BOUNCE(KP2);
            case '3': BOUNCE(KP3);
            case '4': BOUNCE(KP4);
            case '5': BOUNCE(KP5);
            case '6': BOUNCE(KP6);
            case '7': BOUNCE(KP7);
            case '8': BOUNCE(KP8);
            case '9': BOUNCE(KP9);
            case 'e': BOUNCE(KPE);
            case 'c': BOUNCE(KPC);
            default: break; 
        }
        RI0=0;
    }
}

void int_keys(void) __interrupt 5 {
    static unsigned char previous_key = NULL_KEY;
    static unsigned char current_key = NULL_KEY;

    TF2 = 0;

    previous_key = current_key;
    current_key = read_keyboard();

    // if (keypress != NULL_KEY) {
    //     printf_fast_f("keypress: %c\n", key_to_ascii(keypress));
    // }

    if (previous_key == NULL_KEY && current_key != NULL_KEY) {
        // printf_fast_f("Received key %c\n", key_to_ascii(current_key));
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
void putchar(unsigned char c) {
    SBUF0 = c;
    while (TI0 == 0);
    TI0 = 0;
}

unsigned char poll_input(void) {
    unsigned char caught;

    while (keypress == NULL_KEY);

    caught = keypress;
    keypress = NULL_KEY;

    return caught;
}

unsigned char read_string(unsigned char data[], unsigned char len) {
    unsigned char i, key;

    for (i = 0; i < len; ++i) {
        // printf_fast_f("Pooling for %d\n", i);
        key = poll_input();

        if (key == KEY_C || key == KEY_E) {
            return false;
        }

        data[i] = key_to_ascii(key);
    }

    data[i] = '\0';

    return true;
}

void main(void) {
    unsigned char operation = 0;
    unsigned char address[5];
    unsigned char data[4];

    Init_Device();
    SFRPAGE = LEGACY_PAGE;
    keypress = NULL_KEY;

    printf_fast_f("TOP10\n");

    while (1) {
        // clear buffer

        // read operation
        operation = poll_input();

        printf_fast_f("Enter operation\n");
        // if operation is not valid, reset loop
        if (operation != OP_WRITE && operation != OP_READ) {
            printf_fast_f("Did not receive a valid operation key\n");
            continue;
        }

        printf_fast_f("Enter address\n");
        // tries to read an address
        if (read_string(address, sizeof(address) - 1) == false) {
            printf_fast_f("Failed to read address\n");
            continue;
        }

        printf_fast_f("Confirm address\n");
        // expect E
        if (poll_input() != KEY_E) {
            printf_fast_f("did not receive enter\n");
            continue;
        }

        printf_fast_f("reading data\n");
        // if writing, tries to read a data value
        if (operation == OP_WRITE && read_string(data, sizeof(data) - 1) == false) {
            printf_fast_f("failed to read data\n");
            continue;
        }

        printf_fast_f("confirm data\n");
        // if writing, expect E
        if (operation == OP_WRITE && poll_input() != KEY_E) {
            printf_fast_f("did not receive enter\n");
            continue;
        }

        printf_fast_f("Running\n");
        // run operation
        if (operation == OP_WRITE) {
            printf_fast_f("Writing %s to %s\n", data, address);
        } else if (operation == OP_READ) {
            printf_fast_f("Read from %s\n", address);
        } else {
            continue;
        }

        // write_ram(i, (unsigned char) i);
        // printf_fast_f("[%d] = %d\n", i, read_ram(i));
    }
}