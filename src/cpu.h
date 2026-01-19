#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#define BDOS_CALL_ADDR              0x0005
#define BDOS_PRINT_CHAR             0x2
#define BDOS_PRINT_STRING           0x9
#define BDOS_READ_CHAR              0x1
#define BDOS_READ_STRING            0xA

#define MEM_MAX                     0x10000
#define REG_COUNT                   0x7
#define NUM_IO_PORTS                0x100

typedef enum {
    REG_A = 0,
    REG_B,
    REG_C,
    REG_D,
    REG_E,
    REG_H,
    REG_L
} Reg8;

typedef enum {
    RP_BC = 0,
    RP_DE,
    RP_HL,
    RP_SP,
    RP_PSW
} RegPair;

typedef enum {
    COND_NZ = 0,
    COND_Z,
    COND_NC,
    COND_C,
    COND_PO,
    COND_PE,
    COND_P,
    COND_M
} Cond;

typedef enum {
    FLAG_CARRY = 0,
    UNUSED_BIT_1_SET_TO_1,
    FLAG_PARITY,
    UNUSED_BIT_3_SET_TO_0,
    FLAG_AUXILIARY_CARRY,
    UNUSED_BIT_5_SET_TO_0,
    FLAG_ZERO,
    FLAG_SIGN
} Flags;

typedef enum {
    FALSE = 0,
    TRUE
} Bool;

static const uint8_t condFlag[4] = {
    FLAG_ZERO,
    FLAG_CARRY,
    FLAG_PARITY,
    FLAG_SIGN
};

extern uint8_t memory[MEM_MAX];
extern uint8_t registers[REG_COUNT];
extern uint8_t ioPorts[NUM_IO_PORTS];
extern uint8_t flags;
extern uint16_t PC, SP;

extern Bool halted;
extern Bool interruptsEnabled;

void OpInit(void);
int Step(void);

uint8_t MemRead(uint16_t addr);
void MemWrite(uint16_t addr, uint8_t value);
uint8_t FetchByte(void);
uint16_t FetchWord(void);
uint8_t IORead(uint8_t port);
void IOWrite(uint8_t port, uint8_t value);

uint16_t GetPSW(void);
void SetPSW(uint16_t psw);
int CalcRegisterIdx(RegPair rp);
Bool IsFlagActive(Flags flagBit);
void ActivateFlag(Flags flagBit);
void DeActivateFlag(Flags flagBit);
void ClearFlags(void);
Bool CheckCondition(Cond cond);

uint8_t Parity(uint8_t value);
void UpdateZSP(uint8_t result);
void UpdateFlagsZSPCA_ADD(uint8_t oldVal, uint8_t updateVal, uint16_t result);
void UpdateFlagsZSPCA_ADC(uint8_t oldVal, uint8_t updateVal, uint8_t carry_in, uint16_t result);
void UpdateFlagsZSPA_ADD(uint8_t oldVal, uint8_t updateVal, uint16_t result);
void UpdateFlagsZSPCA_SUB(uint8_t oldVal, uint8_t updateVal, uint16_t result);
void UpdateFlagsZSPCA_SBB(uint8_t oldVal, uint8_t updateVal, uint8_t borrow_in, uint16_t result);
void UpdateFlagsZSPA_SUB(uint8_t oldVal, uint8_t updateVal, uint16_t result);
void UpdateFlagsZSP_Logical(uint8_t result, int acVal);
void UpdateFlagC_ADD(uint8_t oldVal, uint8_t updateVal, uint16_t result);
void UpdateFlagC_SUB(uint8_t oldVal, uint8_t updateVal);

void MOV(Reg8 dst, Reg8 src);
void MOV_FROM_M(Reg8 dst);
void MOV_TO_M(Reg8 src);
void MVI(Reg8 dst, uint8_t imm);
void MVI_M(uint8_t imm);
void LXI(RegPair rp, uint16_t imm);
void LDA(uint16_t addr);
void STA(uint16_t addr);
void LHLD(uint16_t addr);
void SHLD(uint16_t addr);
void LDAX(RegPair rp);
void STAX(RegPair rp);
void XCHG(void);

void ADD(Reg8 src);
void ADD_M(void);
void ADI(uint8_t imm);
void ADC(Reg8 src);
void ADC_M(void);
void ACI(uint8_t imm);
void SUB(Reg8 src);
void SUB_M(void);
void SUI(uint8_t imm);
void SBB(Reg8 src);
void SBB_M(void);
void SBI(uint8_t imm);

void INR(Reg8 dst);
void INR_M(void);
void DCR(Reg8 dst);
void DCR_M(void);
void INX(RegPair rp);
void DCX(RegPair rp);
void DAD(RegPair rp);
void DAA(void);

void ANA(Reg8 src);
void ANA_M(void);
void ANI(uint8_t imm);
void ORA(Reg8 src);
void ORA_M(void);
void ORI(uint8_t imm);
void XRA(Reg8 src);
void XRA_M(void);
void XRI(uint8_t imm);
void CMP(Reg8 src);
void CMP_M(void);
void CPI(uint8_t imm);

void RLC(void);
void RRC(void);
void RAL(void);
void RAR(void);
void CMA(void);
void CMC(void);
void STC(void);

void JMP(uint16_t addr);
void JCC(Cond cond, uint16_t addr);
void CALL(uint16_t addr);
void CCC(Cond cond, uint16_t addr);
void RET(void);
void RCC(Cond cond);
void RST(uint8_t rst);
void PCHL(void);

void PUSH(RegPair rp);
void PUSH_PSW(void);
void POP(RegPair rp);
void POP_PSW(void);
void XTHL(void);
void SPHL(void);

void IN(uint8_t port);
void OUT(uint8_t port);
void EI(void);
void DI(void);
void HLT(void);
void NOP(void);

typedef int (*OpcodeHandler)(void);
extern OpcodeHandler opcodeTable[256];

#endif
