#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "cpu.h"
#include "bdos.h"

int LoadProgram(const char* filename, uint16_t startAddr) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    fread(&memory[startAddr], 1, (size_t)size, fp);
    fclose(fp);
    return 0;
}

/*
    CP/M Warm Boot: 0xCD, jmp to addr
    BDOS call: 0xCD, call 0x0005
*/
int Step(void) {
    if (halted) {
        return 0;
    }

    uint16_t prevPC = PC;
    uint8_t opcode = FetchByte();

    if (opcode == 0xCD) {
        uint16_t addr = FetchWord();

        if (addr == 0x0005) {
            BDOS_Call();
            return 17;
        }

        CALL(addr);

        return 17;
    }
    
    if (opcode == 0xC3) {
        uint16_t addr = FetchWord();
    
        if (addr == 0x0000) {
            halted = TRUE;
            return 10;
        }
    
        JMP(addr);
    
        return 10;
    }

    if (opcodeTable[opcode]) {
        return opcodeTable[opcode]();
    }

    printf("Unknown opcode: 0x%02X at PC=0x%04X\n", opcode, prevPC);
    
    return 4;
}

void PrintState(void) {
    printf("\nPC=%04X SP=%04X\n", PC, SP);
    printf("A=%02X B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X\n",
        registers[REG_A], registers[REG_B], registers[REG_C],
        registers[REG_D], registers[REG_E],
        registers[REG_H], registers[REG_L]);

    printf("S=%d Z=%d A=%d P=%d C=%d\n",
        IsFlagActive(FLAG_SIGN),
        IsFlagActive(FLAG_ZERO),
        IsFlagActive(FLAG_AUXILIARY_CARRY),
        IsFlagActive(FLAG_PARITY),
        IsFlagActive(FLAG_CARRY));

    printf("HALT=%d INT=%d\n", halted, interruptsEnabled);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s program.bin [max_instructions]\n", argv[0]);
        return 1;
    }

    OpInit();
    BDOS_Init();

    uint16_t startAddr = 0x0000;
    char *dotExt = strrchr(argv[1], '.');

    if (dotExt && _stricmp(dotExt, ".com") == 0) {
        startAddr = 0x0100;
    }

    if (LoadProgram(argv[1], startAddr) < 0) {
        fprintf(stderr, "Error: Could not load program %s\n", argv[1]);
        return 1;
    }

    /* Set up CP/M environment */
    memory[0x0000] = 0xC3;
    memory[0x0001] = 0x00;
    memory[0x0002] = 0x00;
    memory[0x0005] = 0xC9;
    
    PC = startAddr;
    SP = 0xF000;

    /* Believe me, I had to take help from AI */
    if (argc >= 2) {
        char cmdTail[128] = "";
        
        for (int idx = 1; idx < argc; idx++) {
            if (idx > 1) strcat(cmdTail, " ");
            strcat(cmdTail, argv[idx]);
        }
        
        uint8_t len = (uint8_t)strlen(cmdTail);
        memory[0x0080] = len;
        
        for (int idx = 0; idx < len; idx++) {
            memory[0x0081 + idx] = (uint8_t)cmdTail[idx];
        }

        char *arg = (argc >= 3) ? argv[2] : argv[1];
        char *lastSlash = strrchr(arg, '/');
        
        if (!lastSlash) {
            lastSlash = strrchr(arg, '\\');
        }

        char *filename = lastSlash ? lastSlash + 1 : arg;

        char *dot = strchr(filename, '.');
        int nameLen = dot ? (int)(dot - filename) : (int)strlen(filename);
        
        if (nameLen > 8) {
            nameLen = 8;
        }
        
        memset(&memory[0x005D], ' ', 11);
        
        for (int idx = 0; idx < nameLen; idx++) {
            memory[0x005D + idx] = (uint8_t)toupper(filename[idx]);
        }
        
        if (dot) {
            char *ext = dot + 1;
            int extLen = (int)strlen(ext);
            
            if (extLen > 3) {
                extLen = 3;
            }
            
            for (int idx = 0; idx < extLen; idx++) {
                memory[0x0065 + idx] = (uint8_t)toupper(ext[idx]);
            }
        }
    }

    unsigned long long cycles = 0;
    unsigned long instr = 0;
    unsigned long max_instructions = 50000000;
    
    if (argc >= 4) {
        max_instructions = strtoul(argv[3], NULL, 0);
    }

    /* Needed this to test 8080EXER.COM and 8080EXM.COM */
    if (max_instructions == 0) {
        while (!halted) {
            cycles += (unsigned long long)Step();
            instr++;
        }
    } else {
        while (!halted && instr < max_instructions) {
            cycles += (unsigned long long)Step();
            instr++;
        }
    }

    printf("\nHALTED after %lu instructions (%llu cycles)\n", instr, cycles);
    PrintState();
    
    return 0;
}