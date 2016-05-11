#include <xc.h>           // processor SFR definitions
#include <sys/attribs.h>  // __ISR macro
#include "ILI9163C.h"     // SPI and LCD commands

// DEVCFG0
#pragma config DEBUG = OFF // no debugging
#pragma config JTAGEN = OFF // no jtag
#pragma config ICESEL = ICS_PGx1 // use PGED1 and PGEC1
#pragma config PWP = OFF // no write protect
#pragma config BWP = OFF // no boot write protect
#pragma config CP = OFF // no code protect

// DEVCFG1
#pragma config FNOSC = PRIPLL // use primary oscillator with pll
#pragma config FSOSCEN = OFF // turn off secondary oscillator
#pragma config IESO = OFF // no switching clocks
#pragma config POSCMOD = HS // high speed crystal mode
#pragma config OSCIOFNC = OFF // free up secondary osc pins
#pragma config FPBDIV = DIV_1 // divide CPU freq by 1 for peripheral bus clock
#pragma config FCKSM = CSDCMD // do not enable clock switch
#pragma config WDTPS = PS1048576 // slowest wdt
#pragma config WINDIS = OFF // no wdt window
#pragma config FWDTEN = OFF // wdt off by default
#pragma config FWDTWINSZ = WINSZ_25 // wdt window at 25%

// DEVCFG2 - get the CPU clock to 48MHz
#pragma config FPLLIDIV = DIV_2 // divide input clock to be in range 4-5MHz
#pragma config FPLLMUL = MUL_24 // multiply clock after FPLLIDIV
#pragma config FPLLODIV = DIV_2 // divide clock after FPLLMUL to get 48MHz
#pragma config UPLLIDIV = DIV_2 // divider for the 8MHz input clock, then multiply by 12 to get 48MHz for USB
#pragma config UPLLEN = ON // USB clock on

// DEVCFG3
#pragma config USERID = 0xFFFF // some 16bit userid, doesn't matter what
#pragma config PMDL1WAY = OFF // allow multiple reconfigurations
#pragma config IOL1WAY = OFF // allow multiple reconfigurations
#pragma config FUSBIDIO = ON // USB pins controlled by USB module
#pragma config FVBUSONIO = ON // USB BUSON controlled by USB module

// LCD function prototypes//

void LCD_drawString(unsigned short left, unsigned short top, char *text);
void LCD_drawChar(unsigned short xStart, unsigned short yStart, char symbol);


int main() {

    __builtin_disable_interrupts();

    // set the CP0 CONFIG register to indicate that kseg0 is cacheable (0x3)
    __builtin_mtc0(_CP0_CONFIG, _CP0_CONFIG_SELECT, 0xa4210583);

    // 0 data RAM access wait states
    BMXCONbits.BMXWSDRM = 0x0;

    // enable multi vector interrupts
    INTCONbits.MVEC = 0x1;

    // disable JTAG to get pins back
    DDPCONbits.JTAGEN = 0;
    
    // do your TRIS and LAT commands here
    TRISAbits.TRISA4 = 0;     // ouput
    TRISBbits.TRISB4 = 1;     // input

    SPI1_init();
    LCD_init();
    
    int leet = 1337;        // for converting to string
    char text[100];         // initialized text array to store string
    
    __builtin_enable_interrupts();
    
    
    while(1) {
	    
        _CP0_SET_COUNT(0);         // set core timer to 0
        LCD_clearScreen(BLACK);    // clear screen to start
        sprintf(text,"Hello world %i!",leet);  // create string to print
        LCD_drawString(28,32,text);      // send string to LCD
     
        // infinite loop to keep screen unchanged
        while (1){;}
                

    }  
    
}



// LCD Functions
void LCD_drawChar(unsigned short xStart, unsigned short yStart, char symbol){
    int set, xpos, ypos, asciiIndex;
    int row;       // keeps track of row, 8 rows per character
    char bitMap;   // char design
    int column;    // keeps track of column, 5 coumns per character
    asciiIndex = (int)(symbol - 32);
    
    // Iterate over each of the 5 columns
    for (column = 0;column<5;column++){
        row = 0;
        bitMap = ASCII[asciiIndex][column];
        // Iterate over each of the 8 rows per column
        for (row=0;row < 8;row++){
            set = (bitMap >> row) & 0x01;
            xpos = xStart + column;   // x coordinate of pixel
            ypos = yStart + row;      // y coordinate of pixel
            
            // Check if pixel exists on LCD, draw only if it does
            if (xpos <= 128 && ypos <= 128) {
                if (set) {
                    LCD_drawPixel(xpos, ypos, RED);  // text
                } else {
                    LCD_drawPixel(xpos, ypos, BLACK); // background
                }
            }
        }
        
    }
}

void LCD_drawString(unsigned short left, unsigned short top, char *text){
    int ii=0;
    int xpos=left;
   
    while (text[ii]!=0){
        // Identify if new line character, if so jump down 10 pixels and start at left
        if (text[ii]=='\n'){
            top = top + 10;
            xpos = left;
            ii++;
            continue;
        }
        
        LCD_drawChar(xpos,top,text[ii]);  // Draw the character on the LCD
        xpos = xpos + 6; // increment to starting spot for next char, 1px spacing
        
        ii++;  // increment
        
    }
}