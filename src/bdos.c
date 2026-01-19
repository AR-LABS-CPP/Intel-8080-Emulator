#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "bdos.h"
#include "cpu.h"

/*
    Senor please see here:
    http://cpmarchives.classiccmp.org/cpm/mirrors/electrickery.xs4all.nl/comp/divcomp/doc/TPCH05.pdf
*/

#define MAX_OPEN_FILES 16

typedef struct {
    FILE *fp;
    uint16_t fcb_addr;
} OpenFile;

static OpenFile openFiles[MAX_OPEN_FILES];
static uint16_t dmaAddress = 0x0080;
static uint8_t currentDisk = 0;

void BDOS_Init(void) {
    dmaAddress = 0x0080;
    currentDisk = 0;
    
    for (int idx = 0; idx < MAX_OPEN_FILES; idx++) {
        openFiles[idx].fp = NULL;
        openFiles[idx].fcb_addr = 0;
    }
}

static void GetFilename(uint16_t fcb_addr, char *dest) {
    int pos = 0;
    
    for (int idx = 0; idx < 8; idx++) {
        char c = (char)MemRead(fcb_addr + 1 + idx);
        
        if (c != ' ') {
            dest[pos++] = (char)tolower(c);
        }
    }
    
    char ext[4];
    int extPos = 0;
    
    for (int idx = 0; idx < 3; idx++) {
        char c = (char)MemRead(fcb_addr + 9 + idx);
        
        if (c != ' ') {
            ext[extPos++] = (char)tolower(c);
        }
    }
    
    if (extPos > 0) {
        dest[pos++] = '.';
        
        for (int idx = 0; idx < extPos; idx++) {
            dest[pos++] = ext[idx];
        }
    }
    
    dest[pos] = '\0';
}

static int FindFileHandle(uint16_t fcb_addr) {
    for (int idx = 0; idx < MAX_OPEN_FILES; idx++) {
        if (openFiles[idx].fp != NULL && openFiles[idx].fcb_addr == fcb_addr) {
            return idx;
        }
    }
    
    return -1;
}

static int GetFreeHandle(void) {
    for (int idx = 0; idx < MAX_OPEN_FILES; idx++) {
        if (openFiles[idx].fp == NULL) {
            return idx;
        }
    }
    
    return -1;
}

void BDOS_Call(void) {
    uint8_t func = registers[REG_C];
    uint16_t de = (registers[REG_D] << 8) | registers[REG_E];

    switch (func) {
        case 0: {
            halted = TRUE;
            break;
        }

        case 1: {
            int c = getchar();
            
            registers[REG_A] = (c == EOF) ? 0x1A : (uint8_t)c;
            registers[REG_L] = registers[REG_A];
            
            break;
        }

        case 2: {
            putchar(registers[REG_E]);
            fflush(stdout);
            break;
        }

        case 3: {
            registers[REG_A] = 0x1A;
            registers[REG_L] = registers[REG_A];
            break;
        }

        case 4: {
            break;
        }

        case 5: {
            putchar(registers[REG_E]);
            fflush(stdout);
            break;
        }

        case 6: {
            if (registers[REG_E] == 0xFF) {
                registers[REG_A] = 0;
                registers[REG_L] = 0;
            } else {
                putchar(registers[REG_E]);
                fflush(stdout);
            }
            
            break;
        }

        case 7: {
            registers[REG_A] = 0;
            registers[REG_L] = 0;
            
            break;
        }

        case 8: {
            break;
        }

        case 9: {
            uint16_t addr = de;
            char c;
            
            while ((c = MemRead(addr++)) != '$') {
                putchar(c);
            }
            
            fflush(stdout);
            break;
        }

        case 10: {
            uint16_t addr = de;
            uint8_t maxlen = MemRead(addr);
            uint8_t len = 0;
            int ch;
            
            addr++;
            uint16_t bufStart = addr + 1;
            
            while (len < maxlen && (ch = getchar()) != '\n' && ch != EOF) {
                if (ch == '\b' || ch == 127) {
                    if (len > 0) {
                        len--;
                        
                        printf("\b \b");
                        fflush(stdout);
                    }
                } else {
                    MemWrite(bufStart + len, (uint8_t)ch);
                    
                    putchar(ch);
                    fflush(stdout);
                    
                    len++;
                }
            }
            
            MemWrite(addr, len);
            
            putchar('\n');
            fflush(stdout);
            
            break;
        }

        case 11: {
            registers[REG_A] = 0;
            registers[REG_L] = 0;
            break;
        }

        case 12: {
            registers[REG_H] = 0x00;
            registers[REG_L] = 0x22;
            registers[REG_B] = 0x00;
            registers[REG_A] = 0x22;
            break;
        }

        case 13: {
            currentDisk = 0;
            dmaAddress = 0x0080;
            
            registers[REG_A] = 0;
            registers[REG_L] = 0;
            
            break;
        }

        case 14: {
            currentDisk = registers[REG_E];
            registers[REG_A] = 0;
            registers[REG_L] = 0;
            break;
        }

        case 15: {
            char filename[13];
            GetFilename(de, filename);
            
            int handle = GetFreeHandle();
            if (handle == -1) {
                registers[REG_A] = 0xFF;
                registers[REG_L] = 0xFF;
                break;
            }
            
            FILE *file = fopen(filename, "rb+");
            
            if (!file) {
                file = fopen(filename, "rb");
            }
            
            if (file) {
                openFiles[handle].fp = file;
                openFiles[handle].fcb_addr = de;
                registers[REG_A] = 0;
                registers[REG_L] = 0;
            } else {
                registers[REG_A] = 0xFF;
                registers[REG_L] = 0xFF;
            }
            
            break;
        }

        case 16: {
            int handle = FindFileHandle(de);
            
            if (handle != -1) {
                fclose(openFiles[handle].fp);
                
                openFiles[handle].fp = NULL;
                openFiles[handle].fcb_addr = 0;
                registers[REG_A] = 0;
                registers[REG_L] = 0;
            } else {
                registers[REG_A] = 0xFF;
                registers[REG_L] = 0xFF;
            }
            
            break;
        }

        case 17: {
            char filename[13];
            GetFilename(de, filename);
            
            FILE *file = fopen(filename, "rb");
            
            if (file) {
                fclose(file);
                registers[REG_A] = 0;
                registers[REG_L] = 0;
            } else {
                registers[REG_A] = 0xFF;
                registers[REG_L] = 0xFF;
            }
            
            break;
        }

        case 18: {
            registers[REG_A] = 0xFF;
            registers[REG_L] = 0xFF;
            break;
        }

        case 19: {
            char filename[13];
            GetFilename(de, filename);
            
            if (remove(filename) == 0) {
                registers[REG_A] = 0;
                registers[REG_L] = 0;
            } else {
                registers[REG_A] = 0xFF;
                registers[REG_L] = 0xFF;
            }
            
            break;
        }

        case 20: {
            int handle = FindFileHandle(de);
            
            if (handle != -1) {
                uint8_t buffer[128];
                size_t n = fread(buffer, 1, 128, openFiles[handle].fp);
            
                if (n > 0) {
                    for (size_t idx = 0; idx < n; idx++) {
                        MemWrite(dmaAddress + (uint16_t)idx, buffer[idx]);
                    }
            
                    /* Pad rest with ^Z if needed */
                    for (size_t idx = n; idx < 128; idx++) {
                        MemWrite(dmaAddress + (uint16_t)idx, 0x1A);
                    }
            
                    registers[REG_A] = 0;
                    registers[REG_L] = 0;
                } else {
                    /* EOF */
                    registers[REG_A] = 1;
                    registers[REG_L] = 1;
                }
            } else {
                registers[REG_A] = 0xFF;
                registers[REG_L] = 0xFF;
            }
            
            break;
        }

        case 21: {
            int handle = FindFileHandle(de);
            
            if (handle != -1) {
                uint8_t buffer[128];
            
                for (int idx = 0; idx < 128; idx++) {
                    buffer[idx] = MemRead(dmaAddress + (uint16_t)idx);
                }
            
                if (fwrite(buffer, 1, 128, openFiles[handle].fp) == 128) {
                    registers[REG_A] = 0;
                    registers[REG_L] = 0;
                } else {
                    registers[REG_A] = 0xFF;
                    registers[REG_L] = 0xFF;
                }
            } else {
                registers[REG_A] = 0xFF;
                registers[REG_L] = 0xFF;
            }
            
            break;
        }

        case 22: {
            char filename[13];
            GetFilename(de, filename);
            
            int handle = GetFreeHandle();
            
            if (handle == -1) {
                registers[REG_A] = 0xFF;
                registers[REG_L] = 0xFF;
                break;
            }
            
            FILE *file = fopen(filename, "wb+");
            
            if (file) {
                openFiles[handle].fp = file;
                openFiles[handle].fcb_addr = de;
                registers[REG_A] = 0;
                registers[REG_L] = 0;
            } else {
                registers[REG_A] = 0xFF;
                registers[REG_L] = 0xFF;
            }
            
            break;
        }

        case 23: {
            char oldname[13], newname[13];
            GetFilename(de, oldname);
            GetFilename(de + 16, newname);
            
            if (rename(oldname, newname) == 0) {
                registers[REG_A] = 0;
                registers[REG_L] = 0;
            } else {
                registers[REG_A] = 0xFF;
                registers[REG_L] = 0xFF;
            }
            
            break;
        }

        case 24: {
            registers[REG_H] = 0x00;
            registers[REG_L] = 0x01;
            registers[REG_A] = 0x01;
            break;
        }

        case 25: {
            registers[REG_A] = currentDisk;
            registers[REG_L] = currentDisk;
            break;
        }

        case 26: {
            dmaAddress = de;
            break;
        }

        case 27: {
            registers[REG_H] = 0x00;
            registers[REG_L] = 0x00;
            break;
        }

        case 28: {
            break;
        }

        case 29: {
            registers[REG_H] = 0x00;
            registers[REG_L] = 0x00;
            registers[REG_A] = 0x00;
            break;
        }

        case 30: {
            registers[REG_A] = 0xFF;
            registers[REG_L] = 0xFF;
            break;
        }

        case 31: {
            registers[REG_H] = 0x00;
            registers[REG_L] = 0x00;
            break;
        }

        case 32: {
            if (registers[REG_E] == 0xFF) {
                registers[REG_A] = 0;
                registers[REG_L] = 0;
            } else {
                registers[REG_A] = 0;
                registers[REG_L] = 0;
            }
            
            break;
        }

        case 33: {
            int handle = FindFileHandle(de);
            
            if (handle != -1) {
                uint32_t record = MemRead(de + 33) | (MemRead(de + 34) << 8) | (MemRead(de + 35) << 16);
                fseek(openFiles[handle].fp, record * 128, SEEK_SET);
                
                uint8_t buffer[128];
                size_t n = fread(buffer, 1, 128, openFiles[handle].fp);
            
                if (n > 0) {
                    for (size_t idx = 0; idx < n; idx++) {
                        MemWrite(dmaAddress + (uint16_t)idx, buffer[idx]);
                    }

                    for (size_t idx = n; idx < 128; idx++) {
                        MemWrite(dmaAddress + (uint16_t)idx, 0x1A);
                    }
            
                    registers[REG_A] = 0;
                    registers[REG_L] = 0;
                } else {
                    registers[REG_A] = 1;
                    registers[REG_L] = 1;
                }
            } else {
                registers[REG_A] = 0xFF;
                registers[REG_L] = 0xFF;
            }
            
            break;
        }

        case 34: {
            int handle = FindFileHandle(de);
            
            if (handle != -1) {
                uint32_t record = MemRead(de + 33) | (MemRead(de + 34) << 8) | (MemRead(de + 35) << 16);
                fseek(openFiles[handle].fp, record * 128, SEEK_SET);
                
                uint8_t buffer[128];

                for (int idx = 0; idx < 128; idx++) {
                    buffer[idx] = MemRead(dmaAddress + (uint16_t)idx);
                }
                
                if (fwrite(buffer, 1, 128, openFiles[handle].fp) == 128) {
                    registers[REG_A] = 0;
                    registers[REG_L] = 0;
                } else {
                    registers[REG_A] = 0xFF;
                    registers[REG_L] = 0xFF;
                }
            } else {
                registers[REG_A] = 0xFF;
                registers[REG_L] = 0xFF;
            }
            
            break;
        }

        case 35: {
            char filename[13];
            GetFilename(de, filename);
            
            FILE *file = fopen(filename, "rb");
            
            if (file) {
                fseek(file, 0, SEEK_END);
                long size = ftell(file);
                fclose(file);
                
                uint32_t records = (uint32_t)((size + 127) / 128);
                
                MemWrite(de + 33, records & 0xFF);
                MemWrite(de + 34, (records >> 8) & 0xFF);
                MemWrite(de + 35, (records >> 16) & 0xFF);
                
                registers[REG_A] = 0;
                registers[REG_L] = 0;
            } else {
                registers[REG_A] = 0xFF;
                registers[REG_L] = 0xFF;
            }
            
            break;
        }

        case 36: {
            int handle = FindFileHandle(de);
            
            if (handle != -1) {
                long pos = ftell(openFiles[handle].fp);
            
                uint32_t records = (uint32_t)(pos / 128);
            
                MemWrite(de + 33, records & 0xFF);
                MemWrite(de + 34, (records >> 8) & 0xFF);
                MemWrite(de + 35, (records >> 16) & 0xFF);
            
                registers[REG_A] = 0;
                registers[REG_L] = 0;
            } else {
                registers[REG_A] = 0xFF;
                registers[REG_L] = 0xFF;
            }
            
            break;
        }

        case 37: {
            registers[REG_A] = 0;
            registers[REG_L] = 0;
            break;
        }

        case 40: {
            int handle = FindFileHandle(de);
            
            if (handle != -1) {
                uint32_t record = MemRead(de + 33) | (MemRead(de + 34) << 8) | (MemRead(de + 35) << 16);
                fseek(openFiles[handle].fp, record * 128, SEEK_SET);
                
                uint8_t buffer[128];

                for (int idx = 0; idx < 128; idx++) {
                    buffer[idx] = MemRead(dmaAddress + (uint16_t)idx);
                }
                
                if (fwrite(buffer, 1, 128, openFiles[handle].fp) == 128) {
                    registers[REG_A] = 0;
                    registers[REG_L] = 0;
                } else {
                    registers[REG_A] = 0xFF;
                    registers[REG_L] = 0xFF;
                }
            } else {
                registers[REG_A] = 0xFF;
                registers[REG_L] = 0xFF;
            }
            
            break;
        }

        default: {
            registers[REG_A] = 0xFF;
            registers[REG_L] = 0xFF;
            break;
        }
    }
}
