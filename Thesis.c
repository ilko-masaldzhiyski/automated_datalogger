/////////////////////////////////////////////////////////////////////////
////                      ex_mmcsd.c                                 ////
////                                                                 ////
//// Similar to ex_extee.c, an example that demonstrates             ////
//// writing and reading to an MMC/SD card.                          ////
////                                                                 ////
/////////////////////////////////////////////////////////////////////////
////        (C) Copyright 2007 Custom Computer Services              ////
//// This source code may only be used by licensed users of the CCS  ////
//// C compiler.  This source code may only be distributed to other  ////
//// licensed users of the CCS C compiler.  No other use,            ////
//// reproduction or distribution is permitted without written       ////
//// permission.  Derivative programs created using this software    ////
//// in object code form are not restricted in any way.              ////
/////////////////////////////////////////////////////////////////////////

//These settings are for the CCS PICEEC development kit which contains
//an MMC/SD connector.
#include <24fj128ga006.h>
#fuses HS, NOWDT, NOPROTECT
#use delay(oscillator=4M,clock=4M)
#use rs232(baud=9600, bits = 8, parity = N, xmit = PIN_F3, rcv = PIN_F2, ERRORS)

#include <stdlib.h> // for atoi32
#include <stdio.h>
//meda library, a compatable media library is required for FAT.
#use fast_io(c)
#define PIN_C2    		  PIN_G9
#define MMCSD_PIN_SCL     PIN_G6 //o
#define MMCSD_PIN_SDI     PIN_G7 //i
#define MMCSD_PIN_SDO     PIN_G8 //o
#define MMCSD_PIN_SELECT  PIN_G9 //o
#include <mmcsd.c>
#include <input.c>
#include <string.h>
char Buffer[512];
char sBuffer[512];
char BootSector[512];
char Logger[32] = {0x44,0x4C,0x4F,0x47,0x47,0x45,0x52,0x20,0x49,0x4C,0x4D,0x00,
                    0x00,0xBC,0xEE,0x8B,0xC5,0x42,0x00,0x00,0x00,0x00,0xF8,0x8B,
                    0xC5,0x42,0x02,0x00,0x047,0x02,0x00,0x00};
int32 ResSec,FilePA,BytesPS,SecPF,NumRE,RDStart,FATStart,DataStart;

void SD_read(unsigned long sector, unsigned int offset,
             unsigned int len) {
    int i;

    OUTPUT_LOW(PIN_C2);

    SPI_WRITE2(0x51);
    SPI_WRITE2(sector>>15); // sector*512 >> 24
    SPI_WRITE2(sector>>7);  // sector*512 >> 16
    SPI_WRITE2(sector<<1);  // sector*512 >> 8
    SPI_WRITE2(0);          // sector*512
    SPI_WRITE2(0xFF);

    for(i=0; i<10 && SPI_READ2(0xFF) != 0x00; i++) {} // wait for 0

    for(i=0; i<20 && SPI_READ2(0xFF) != 0xFE; i++) {} // wait for data start

    for(i=0; i<offset; i++) // "skip" bytes
        SPI_WRITE2(0xFF);

    for(i=0; i<len; i++){ // read len bytes
        Buffer[i] = SPI_READ2(0xFF);
        printf("%c",Buffer[i]);
        if (i%32==0 && i!=0)
            printf("\r\n");
    }
    printf("\r\n");

    for(i+=offset; i<512; i++) // "skip" again
        SPI_WRITE2(0xFF);

    // skip checksum
    SPI_WRITE2(0xFF);
    SPI_WRITE2(0xFF);

    OUTPUT_HIGH(PIN_C2);
}
void getBoot(void){
    int32 temp;
    int i;
    SD_read(0,0,512);               //
    for (i=0;i<512;i++)             // read boot sector and transfer to boot array
        BootSector[i]=Buffer[i];    //
    (char)temp = BootSector[12];
    temp<<=8;
    (char)temp += BootSector[11];
    BytesPS = temp;
    FilePA = BootSector[13];
    temp = 0;
    (char)temp = BootSector[15];
    temp<<=8;
    (char)temp += BootSector[14];
    ResSec = temp;
    temp = 0;
    (char)temp = BootSector[18];
    temp <<=8;
    (char)temp += BootSector[17];
    NumRE = temp;
    temp = 0;
    (char)temp = BootSector[23];
    temp<<=8;
    (char)temp+=BootSector[22];
    SecPF = temp;
    FATStart = ResSec;
    RDStart = FATStart + SecPF*2;
    DataStart = NumRE*32;
    DataStart >>=9;
    DataStart += RDStart;
}
void SD_write(unsigned long sector, unsigned int offset,
              unsigned int len) {
    int i;
    SD_Read(sector,0,512);
    OUTPUT_LOW(PIN_C2);
    SPI_WRITE2(0x58);
    SPI_WRITE2(sector>>15); // sector*512 >> 24
    SPI_WRITE2(sector>>7);  // sector*512 >> 16
    SPI_WRITE2(sector<<1);  // sector*512 >> 8
    SPI_WRITE2(0x00);          // sector*512
    SPI_WRITE2(0xFF);
    SPI_WRITE2(0xFF);
    if (SPI_READ2(0xFF)==0){
        SPI_WRITE2(0xFE);
        for (i=0;i<offset;i++){
            SPI_WRITE2(Buffer[i]);
            printf("Just wrote from Buffer: %c\r\n",Buffer[i]);
        }
        for (i=0;i<len;i++){
            SPI_WRITE2(sBuffer[i]);
            printf("Just wrote from sBuffer: %c\r\n",sBuffer[i]);
        }
        for (i+=offset;i<512;i++){
            SPI_WRITE2(Buffer[i]);
            printf("Just wrote again from Buffer: %c\r\n",Buffer[i]);
        }
        SPI_WRITE2(0xFF);                   // CRC low byte
        SPI_WRITE2(0xFF);                   // CRC high byte
        SPI_READ2(0xFF);                    // NRC
        for(i=0;i<512;i++){
            if(SPI_READ2(0xFF)==0){        //waiting for 00 meaning all went well
                printf("The writing went well !\r\n");
                break;
            }
        }
    }
}

int mmc_send_cmd(int cmd,int32 sector)
{
    printf("Sector is at a distance: %d,%X\r\n",sector*512,sector*512);
    printf("Highbyte is %X, Midbyte is %X, Lowbyte is %X\r\n",((sector*512)&0xFF0000)>>16,((sector*512)&0xFF00)>>8,(sector*512)&0xFF);
    SPI_WRITE2(cmd|0x40);                           //Commands start at 0x40, so every higher command must be ORed with 0x40
    SPI_WRITE2(0x00);
    SPI_WRITE2(((sector*512)&0xFF0000)>>16);        // High byte of address HEX
    SPI_WRITE2(((sector*512)&0xFF00)>>8);           // Mid byte of address HEX
    SPI_WRITE2((sector*512)&0xFF);                  // Low byte of address HEX
    SPI_WRITE2(0x00);
    SPI_WRITE2(0xFF);
    return SPI_READ2(0xFF);
}
int mmc_init()
{
int i;
SETUP_SPI2(SPI_MASTER | SPI_L_TO_H | SPI_CLK_DIV_2 | SPI_SS_DISABLED);
OUTPUT_HIGH(PIN_C2);                    // set SS = 1 (off)
for(i=0;i<10;i++)                       // initialise the MMC card into SPI mode by sending clks on
{
        SPI_WRITE2(0xFF);
}
printf("Reset command sent !\r\n");

OUTPUT_LOW(PIN_C2);                     // set SS = 0 (on)

SPI_WRITE2(0x40);                       // Send reset command
SPI_WRITE2(0x00);
SPI_WRITE2(0x00);
SPI_WRITE2(0x00);
SPI_WRITE2(0x00);
SPI_WRITE2(0x95);
SPI_READ2(0xFF);

if(SPI_READ2(0xFF)==1)                   // SD card must respond with 1 if IDLE
    printf("MMC responded that it is IDLE !");
else {
    printf("SPI_READ2 returned %d\r\n",i);
}

SPI_WRITE2(0x41);                       // Send init command
SPI_WRITE2(0x00);
SPI_WRITE2(0x00);
SPI_WRITE2(0x00);
SPI_WRITE2(0x00);
SPI_WRITE2(0x00);
SPI_READ2(0xFF);


return SPI_READ2(0xFF);                 // Should return 0 if init went okay
}
unsigned long find_free_fat(void){
    int i,j;
    unsigned long r=0;
    for (i=0;i<SecPF;i++){
        SD_read(FATStart+i*FATStart,0,512);
        for (j=0;i<256;j+=2){
            if(Buffer[j] == 0 && Buffer[j+1] == 0){
                r = i*512 + j;
                return r;
            }
        }
    }
    return 0;
}
void create_log_file(void){
    int i,j,free,k=0;
    for (i=0;i<((NumRE*32)>>8) && k==0;i++){
        SD_read(RDStart+i,0,512);
        for(j=0;j<16;j++){
            if(Buffer[j*32]==0 || Buffer[j*32]==0xE5){
                k=1;
                if(Buffer[j*32]==0xE5)
                    k=2;
                break;
            }
        }
    }
    free = j + 16*(i-1);
    if (k==1 || k == 2){
        printf("\r\nSuccess !");
        printf("\r\nFile Place found at root entry: %d\r\n",free);
        k = free / 16;
        Logger[26] = find_free_fat() & 0x00FF;
        Logger[27] = (find_free_fat() & 0xFF00)>>8;
        sBuffer[0]=0xFF;
        sBuffer[1]=0xFF;
        i = find_free_fat() / 512;
        SD_write(FATStart + i*FATStart,Logger[26],2);
        for (i=0;i<32;i++)
            sBuffer[i]=Logger[i];
        SD_write(RDStart+k,free*32,32);
        printf("Found free FAT at: %d",find_free_fat());
        //sBuffer[0] = 0xFF;
        //sBuffer[1] = 0xFF;
        //SD_write(FATStart,find_free_fat(),2);
    }
    else
        printf("\r\nFailure !");
}

void main(void)
{
delay_ms(1);                  // Before talking to the MMC/SD 1 mS must pass

if(mmc_init() == 0)
    printf("SUCCESS !\r\n");
else
    printf("FAILURE\r\n");
getBoot();
printf("\r\nBytes per Sector: %d",BytesPS);
printf("\r\nSectors per allocation: %d"FilePA);
printf("\r\nReserved sectors: %d", ResSec);
printf("\r\nSectors per FAT: %d",SecPF);
printf("\r\nNumber of root entry directories: %d", NumRE);
printf("\r\nFAT1 starts at sector: %d", FATStart);
printf("\r\nFAT2 starts at sector: %d", FATStart+SecPF);
printf("\r\nRoot directory starts at sector: %d",RDStart);
printf("\r\nData starts at sector: %d\r\n",DataStart);
create_log_file();
}

