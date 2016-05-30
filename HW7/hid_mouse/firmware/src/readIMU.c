#include<xc.h>           // processor SFR definitions
#include<sys/attribs.h>  // __ISR macro
#include <stdio.h>
#include <math.h> 
#include "readIMU.h"

#define IMU_ADDRESS 0b1101011
#define OUT_TEMP_L 0x20

//CTRL1,CTRL2,CTRL3 initialize values//
#define CTRL1_XL 0b10000001
#define CTRL2_G  0b10000000
#define CTRL3_C  0b00000100

//variable initialization//
    unsigned char output[14];
    signed short scale = 2000;  // Lower means faster mouse speed
    signed short temp;
    float tempf, accel;
    char array[100];
    

static unsigned char pGammaSet[15]= {0x36,0x29,0x12,0x22,0x1C,0x15,0x42,0xB7,0x2F,0x13,0x12,0x0A,0x11,0x0B,0x06};
static unsigned char nGammaSet[15]= {0x09,0x16,0x2D,0x0D,0x13,0x15,0x40,0x48,0x53,0x0C,0x1D,0x25,0x2E,0x34,0x39};

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

float Read_IMU_Mouse(char reg) {
    
        I2C_read_multiple(IMU_ADDRESS<<1,reg,output,2);
        
        temp = (output[0] | (output[1] << 8));
        accel=((float)temp)/16383;
        tempf = ((float)temp)/scale;
        double f = accel;
        
        if (reg==0x28){
            sprintf(array,"ACCEL_X(g):  %.2f  ",f);
            LCD_drawString(10,49,array);
        }
        else if (reg==0x2A){
            sprintf(array,"ACCEL_Y(g):  %.2f  ",f);
            LCD_drawString(10,74,array);
        }
      
        return tempf;
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

void SPI1_init() {
	SDI1Rbits.SDI1R = 0b0100; // B8 is SDI1
    RPA1Rbits.RPA1R = 0b0011; // A1 is SDO1
    TRISBbits.TRISB7 = 0; // SS is B7
    LATBbits.LATB7 = 1; // SS starts high

    // A0 / DAT pin
    ANSELBbits.ANSB15 = 0;
    ANSELBbits.ANSB14 = 0;
    TRISBbits.TRISB15 = 0;
    LATBbits.LATB15 = 0;
	
	SPI1CON = 0; // turn off the spi module and reset it
    SPI1BUF; // clear the rx buffer by reading from it
    SPI1BRG = 1; // baud rate to 12 MHz [SPI1BRG = (48000000/(2*desired))-1]
    SPI1STATbits.SPIROV = 0; // clear the overflow bit
    SPI1CONbits.CKE = 1; // data changes when clock goes from hi to lo (since CKP is 0)
    SPI1CONbits.MSTEN = 1; // master operation
    SPI1CONbits.ON = 1; // turn on spi1
}

unsigned char spi_io(unsigned char o) {
  SPI1BUF = o;
  while(!SPI1STATbits.SPIRBF) { // wait to receive the byte
    ;
  }
  return SPI1BUF;
}

void LCD_command(unsigned char com) {
    LATBbits.LATB15 = 0; // DAT
    LATBbits.LATB7 = 0; // CS
    spi_io(com);
    LATBbits.LATB7 = 1; // CS
}

void LCD_data(unsigned char dat) {
    LATBbits.LATB15 = 1; // DAT
    LATBbits.LATB7 = 0; // CS
    spi_io(dat);
    LATBbits.LATB7 = 1; // CS
}

void LCD_data16(unsigned short dat) {
    LATBbits.LATB15 = 1; // DAT
    LATBbits.LATB7 = 0; // CS
    spi_io(dat>>8);
    spi_io(dat);
    LATBbits.LATB7 = 1; // CS
}

void LCD_init() {
    int time = 0;
    LCD_command(CMD_SWRESET);//software reset
    time = _CP0_GET_COUNT();
    while (_CP0_GET_COUNT() < time + 48000000/2/2) {} //delay(500);

	LCD_command(CMD_SLPOUT);//exit sleep
    time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/200) {} //delay(5);

	LCD_command(CMD_PIXFMT);//Set Color Format 16bit
	LCD_data(0x05);
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/200) {} //delay(5);

	LCD_command(CMD_GAMMASET);//default gamma curve 3
	LCD_data(0x04);//0x04
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_GAMRSEL);//Enable Gamma adj
	LCD_data(0x01);
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_NORML);

	LCD_command(CMD_DFUNCTR);
	LCD_data(0b11111111);
	LCD_data(0b00000110);

    int i = 0;
	LCD_command(CMD_PGAMMAC);//Positive Gamma Correction Setting
	for (i=0;i<15;i++){
		LCD_data(pGammaSet[i]);
	}

	LCD_command(CMD_NGAMMAC);//Negative Gamma Correction Setting
	for (i=0;i<15;i++){
		LCD_data(nGammaSet[i]);
	}

	LCD_command(CMD_FRMCTR1);//Frame Rate Control (In normal mode/Full colors)
	LCD_data(0x08);//0x0C//0x08
	LCD_data(0x02);//0x14//0x08
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_DINVCTR);//display inversion
	LCD_data(0x07);
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_PWCTR1);//Set VRH1[4:0] & VC[2:0] for VCI1 & GVDD
	LCD_data(0x0A);//4.30 - 0x0A
	LCD_data(0x02);//0x05
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_PWCTR2);//Set BT[2:0] for AVDD & VCL & VGH & VGL
	LCD_data(0x02);
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_VCOMCTR1);//Set VMH[6:0] & VML[6:0] for VOMH & VCOML
	LCD_data(0x50);//0x50
	LCD_data(99);//0x5b
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_VCOMOFFS);
	LCD_data(0);//0x40
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_CLMADRS);//Set Column Address
	LCD_data16(0x00);
    LCD_data16(_GRAMWIDTH);

	LCD_command(CMD_PGEADRS);//Set Page Address
	LCD_data16(0x00);
    LCD_data16(_GRAMHEIGH);

	LCD_command(CMD_VSCLLDEF);
	LCD_data16(0); // __OFFSET
	LCD_data16(_GRAMHEIGH); // _GRAMHEIGH - __OFFSET
	LCD_data16(0);

	LCD_command(CMD_MADCTL); // rotation
    LCD_data(0b00001000); // bit 3 0 for RGB, 1 for GBR, rotation: 0b00001000, 0b01101000, 0b11001000, 0b10101000

	LCD_command(CMD_DISPON);//display ON
	time = _CP0_GET_COUNT();
	while (_CP0_GET_COUNT() < time + 48000000/2/1000) {} //delay(1);

	LCD_command(CMD_RAMWR);//Memory Write
}

void LCD_drawPixel(unsigned short x, unsigned short y, unsigned short color) {
    // check boundary
    LCD_setAddr(x,y,x+1,y+1);
    LCD_data16(color);
}

void LCD_setAddr(unsigned short x0, unsigned short y0, unsigned short x1, unsigned short y1) {
    LCD_command(CMD_CLMADRS); // Column
    LCD_data16(x0);
	LCD_data16(x1);

	LCD_command(CMD_PGEADRS); // Page
	LCD_data16(y0);
	LCD_data16(y1);

	LCD_command(CMD_RAMWR); //Into RAM
}

void LCD_clearScreen(unsigned short color) {
    int i;
    LCD_setAddr(0,0,_GRAMWIDTH,_GRAMHEIGH);
		for (i = 0;i < _GRAMSIZE; i++){
			LCD_data16(color);
		}
}

// Draw characters to the LCD
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

// Draw strings to the LCD
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

