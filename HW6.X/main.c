#include<xc.h>           // processor SFR definitions
#include<sys/attribs.h>  // __ISR macro
#include <stdio.h>
#include <math.h> 
#include "ILI9163C.h"

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
#pragma config OSCIOFNC = ON // free up secondary osc pins
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
#pragma config UPLLIDIV =  DIV_2 // divider for the 8MHz input clock, then multiply by 12 to get 48MHz for USB
#pragma config UPLLEN = ON // USB clock on

// DEVCFG3
#pragma config USERID = 0x1000 // some 16bit userid, doesn't matter what
#pragma config PMDL1WAY = OFF // allow multiple reconfigurations
#pragma config IOL1WAY=  OFF // allow multiple reconfigurations
#pragma config FUSBIDIO = ON // USB pins controlled by USB module
#pragma config FVBUSONIO = ON // USB BUSON controlled by USB module

#define IMU_ADDRESS 0b1101011
#define OUT_TEMP_L 0x20

//CTRL1,CTRL2,CTRL3 initialize values//
#define CTRL1_XL 0b10000001
#define CTRL2_G  0b10000000
#define CTRL3_C  0b00000100

//I2C functions//
void initI2C2(void){
    ANSELBbits.ANSB2 = 0;
    ANSELBbits.ANSB3 = 0;
    //some number for 100kHz; // I2CBRG = [1/(2*Fsck) - PGD]*Pblck - 2
    I2C2BRG = 233;           // PGD = 104ns, Fsck = 100kHz, Pblck = 48MHz.
    I2C2CONbits.ON = 1;      // turn on the I2C2 module
}

void i2c_master_start(void) {
    I2C2CONbits.SEN = 1;            // send the start bit
    while(I2C2CONbits.SEN) { ; }    // wait for the start bit to be sent
}

void i2c_master_restart(void) {
    I2C2CONbits.RSEN = 1;           // send a restart
    while(I2C2CONbits.RSEN) { ; }   // wait for the restart to clear
}

void i2c_master_send(unsigned char byte) { // send a byte to slave
  I2C2TRN = byte;                   // if an address, bit 0 = 0 for write, 1 for read
  while(I2C2STATbits.TRSTAT) { ; }  // wait for the transmission to finish
  if(I2C2STATbits.ACKSTAT) {        // if this is high, slave has not acknowledged
    // ("I2C2 Master: failed to receive ACK\r\n");
  }
}

unsigned char i2c_master_recv(void) { // receive a byte from the slave
    I2C2CONbits.RCEN = 1;             // start receiving data
    while(!I2C2STATbits.RBF) { ; }    // wait to receive the data
    return I2C2RCV;                   // read and return the data
}

void i2c_master_ack(int val) {        // sends ACK = 0 (slave should send another byte)
                                      // or NACK = 1 (no more bytes requested from slave)
    I2C2CONbits.ACKDT = val;          // store ACK/NACK in ACKDT
    I2C2CONbits.ACKEN = 1;            // send ACKDT
    while(I2C2CONbits.ACKEN) { ; }    // wait for ACK/NACK to be sent
}

void i2c_master_stop(void) {          // send a STOP:
  I2C2CONbits.PEN = 1;                // comm is complete and master relinquishes bus
  while(I2C2CONbits.PEN) { ; }        // wait for STOP to complete
}

//function initializations//
unsigned char readIMU(char reg);
void init_IMU(void);
void I2C_read_multiple(char address, char Register, unsigned char * data, char length);
void LCD_drawString(unsigned short x, unsigned short y, char *array);

//variable initialization//
    unsigned char output[14];
    signed short scaleA = 16383;
    signed short scaleG = 134;
    signed short gyroX,gyroY,gyroZ,accelX,accelY,accelZ,temp;
    float accelXf,accelYf,accelZf,gyroXf,gyroYf,gyroZf;
    char array[100];


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
    LATAbits.LATA4 = 0;
    
    initI2C2();
    SPI1_init();
    LCD_init();
    init_IMU();
    LCD_clearScreen(BLACK);
    
    __builtin_enable_interrupts();
    
        
    while(1) {
	    // use _CP0_SET_COUNT(0) and _CP0_GET_COUNT() to test the PIC timing
		// remember the core timer runs at half the CPU speed
        _CP0_SET_COUNT(0);                   // set core timer to 0
        
        while (_CP0_GET_COUNT() < 480000){;} // read at 50 Hz -- 480k / 24 MHz
               // intialize LED on
        
        
        I2C_read_multiple(IMU_ADDRESS<<1,OUT_TEMP_L,output,14);
        
        temp = (output[0] | (output[1] << 8));
        gyroX = (output[2] | (output[3] << 8));
        gyroY = (output[4] | (output[5] << 8));
        gyroZ = (output[6] | (output[7] << 8));
        accelX = (output[8] | (output[9] << 8));
        accelY = (output[10] | (output[11] << 8));
        accelZ = (output[14] | (output[13] << 8));
        
        //accelX = accelX/scale;
        accelXf = ((float)accelX)/scaleA;
        accelYf = ((float)accelY)/scaleA;
        accelZf = ((float)accelZ)/scaleA;
        gyroXf = ((float)gyroX)/scaleG;
        gyroYf = ((float)gyroY)/scaleG;
        gyroZf = ((float)gyroZ)/scaleG;
        
        // Write acceleration and gyro values to LCD
        sprintf(array,"accelX(g): %.2f   ",accelXf);
        LCD_drawString(5,12,array);
        sprintf(array,"accelY(g): %.2f   ",accelYf);
        LCD_drawString(5,27,array);
        sprintf(array,"accelZ(g): %.2f   ",accelZf);
        LCD_drawString(5,42,array);
        sprintf(array,"gyroX(dps): %.2f   ",gyroXf);
        LCD_drawString(5,57,array);
        sprintf(array,"gyroY(dps): %.2f   ",gyroYf);
        LCD_drawString(5,72,array);
        sprintf(array,"gyroZ(dps): %.2f   ",gyroZf);
        LCD_drawString(5,87,array);
        sprintf(array,"TEMP: %i   ",temp);
        LCD_drawString(5,102,array);
        
    }
}


//IMU Setup//

unsigned char readIMU(char reg){
    //LATAbits.LATA4 = 1;
    i2c_master_start();
    i2c_master_send(IMU_ADDRESS<<1);
    i2c_master_send(reg);
    i2c_master_restart();
    i2c_master_send(0b11010111);
    unsigned char r = i2c_master_recv();
    i2c_master_ack(1); 
    i2c_master_stop(); 
    return r;
}

void init_IMU(void){
    //init_XL//
    i2c_master_start(); // make the start bit
    i2c_master_send(IMU_ADDRESS<<1); // write the address, shifted left by 1, or'ed with a 0 to indicate writing
    i2c_master_send(0x10); // the register to write to
    i2c_master_send(CTRL1_XL); // the value to put in the register
    i2c_master_stop(); // make the stop bit
    //init_G//
    i2c_master_start(); // make the start bit
    i2c_master_send(IMU_ADDRESS<<1); // write the address, shifted left by 1, or'ed with a 0 to indicate writing
    i2c_master_send(0x11); // the register to write to
    i2c_master_send(CTRL2_G); // the value to put in the register
    i2c_master_stop(); // make the stop bit
    //init_C//
    i2c_master_start(); // make the start bit
    i2c_master_send(IMU_ADDRESS<<1); // write the address, shifted left by 1, or'ed with a 0 to indicate writing
    i2c_master_send(0x12); // the register to write to
    i2c_master_send(CTRL3_C); // the value to put in the register
    i2c_master_stop(); // make the stop bit
}

void I2C_read_multiple(char address, char Register, unsigned char * data, char length){
    int i = 0;
    i2c_master_start();
    i2c_master_send(address);
    i2c_master_send(Register);
    i2c_master_restart();
    i2c_master_send(0b11010111);
    for (i=0;i<=length;i++){
    output[i] = i2c_master_recv();
    if (i<length){
        i2c_master_ack(0);
    }
    else if (i==length){
        i2c_master_ack(1);
    }
    } 
    i2c_master_stop(); 
}
