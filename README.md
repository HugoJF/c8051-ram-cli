# C8051 SPI RAM CLI 

Firmware for a CLI for a SPI RAM chip using a virtual keyboard via serial terminal that emulates real switches with bouncing.

## How it works

Commands are sent through via serial terminal (provided by BIG8051 module with USB COM interface) as a series of characters which are then processed by the uC.

Write commands have the following format: `0<4 char RAM address>e<3 char data>e`;
Read commands have teh following format: `1<4 char RAM address>e`.
