#include <stdio.h>
#include "cpu.h"

uint8_t memory[MEM_MAX] = { 0 };
uint8_t registers[REG_COUNT] = { 0 };
uint8_t ioPorts[NUM_IO_PORTS] = { 0 };

uint8_t flags = 0x02;
uint16_t PC = 0, SP = 0;

Bool halted = FALSE;
Bool interruptsEnabled = FALSE;

Bool CheckCondition(Cond cond) {
    switch (cond) {
        case COND_NZ: {
            return !IsFlagActive(FLAG_ZERO);
        }
        
        case COND_Z: {
            return IsFlagActive(FLAG_ZERO);
        }
        
        case COND_NC: {
            return !IsFlagActive(FLAG_CARRY);
        }
        
        case COND_C: {
            return IsFlagActive(FLAG_CARRY);
        }
        
        case COND_PO: {
            return !IsFlagActive(FLAG_PARITY);
        }

        case COND_PE: {
            return IsFlagActive(FLAG_PARITY);
        }
        
        case COND_P: {
            return !IsFlagActive(FLAG_SIGN);
        }
        
        case COND_M: {
            return IsFlagActive(FLAG_SIGN);
        }
        
        default: {
            return FALSE;
        }
    }
}

uint8_t MemRead(uint16_t addr) {
    return memory[addr];
}

void MemWrite(uint16_t addr, uint8_t value) {
    memory[addr] = value;
}

uint8_t IORead(uint8_t port) {
    return ioPorts[port];
}
void IOWrite(uint8_t port, uint8_t value) {
    ioPorts[port] = value;
}

uint8_t FetchByte(void) {
    return memory[PC++];
}

uint16_t FetchWord(void) {
    uint16_t word = memory[PC] | (memory[PC + 1] << 8);
    PC += 2;

    return word;
}

Bool IsFlagActive(Flags flagBit) {
    return (flags >> flagBit) & 1;
}

void ActivateFlag(Flags flagBit) {
    flags |= (1 << flagBit);
}

void DeActivateFlag(Flags flagBit) {
    flags &= ~(1 << flagBit);
}

void ClearFlags(void) {
    flags = 0x02;
}

uint8_t Parity(uint8_t value) {
    uint8_t count = 0;
    
    for (int i = 0; i < 8; i++) {
        if (value & (1 << i)) {
            count++;
        }
    }
    
    return (count % 2) == 0;
}

int CalcRegisterIdx(RegPair rp) {
    return (rp - RP_BC) * 2 + 1;
}

uint16_t GetPSW(void) {
    return (registers[REG_A] << 8) | flags;
}

void SetPSW(uint16_t psw) {
    registers[REG_A] = psw >> 8;
    flags = psw & 0xFF;
}

void UpdateZSP(uint8_t result) {
    if (result == 0) {
        ActivateFlag(FLAG_ZERO);
    } else {
        DeActivateFlag(FLAG_ZERO);
    }

    if (result & 0x80) {
        ActivateFlag(FLAG_SIGN);
    } else {
        DeActivateFlag(FLAG_SIGN);
    }

    if (Parity(result)) {
        ActivateFlag(FLAG_PARITY);
    } else {
        DeActivateFlag(FLAG_PARITY);
    }
}

void UpdateFlagsZSPCA_ADD(uint8_t oldVal, uint8_t updateVal, uint16_t result) {
    UpdateZSP((uint8_t)result);
    
    if (result & 0x100) {
        ActivateFlag(FLAG_CARRY);
    } else {
        DeActivateFlag(FLAG_CARRY);
    }

    if (((oldVal & 0x0F) + (updateVal & 0x0F)) & 0x10) {
        ActivateFlag(FLAG_AUXILIARY_CARRY);
    } else {
        DeActivateFlag(FLAG_AUXILIARY_CARRY);
    }
}

void UpdateFlagsZSPCA_ADC(uint8_t oldVal, uint8_t updateVal, uint8_t carry_in, uint16_t result) {
    UpdateZSP((uint8_t)result);
    
    if (result & 0x100) {
        ActivateFlag(FLAG_CARRY);
    } else {
        DeActivateFlag(FLAG_CARRY);
    }

    if (((oldVal & 0x0F) + (updateVal & 0x0F) + carry_in) & 0x10) {
        ActivateFlag(FLAG_AUXILIARY_CARRY);
    } else {
        DeActivateFlag(FLAG_AUXILIARY_CARRY);
    }
}

void UpdateFlagsZSPA_ADD(uint8_t oldVal, uint8_t updateVal, uint16_t result) {
    UpdateZSP((uint8_t)result);

    if (((oldVal & 0x0F) + (updateVal & 0x0F)) & 0x10) {
        ActivateFlag(FLAG_AUXILIARY_CARRY);
    } else {
        DeActivateFlag(FLAG_AUXILIARY_CARRY);
    }
}

void UpdateFlagsZSPCA_SUB(uint8_t oldVal, uint8_t updateVal, uint16_t result) {
    UpdateZSP((uint8_t)result);
    
    if (result & 0xFF00) {
        ActivateFlag(FLAG_CARRY);
    } else {
        DeActivateFlag(FLAG_CARRY);
    }

    if ((oldVal & 0x0F) >= (updateVal & 0x0F)) {
        ActivateFlag(FLAG_AUXILIARY_CARRY);
    } else {
        DeActivateFlag(FLAG_AUXILIARY_CARRY);
    }
}

void UpdateFlagsZSPCA_SBB(uint8_t oldVal, uint8_t updateVal, uint8_t borrow_in, uint16_t result) {
    UpdateZSP((uint8_t)result);
    
    if (result & 0xFF00) {
        ActivateFlag(FLAG_CARRY);
    } else {
        DeActivateFlag(FLAG_CARRY);
    }

    if ((oldVal & 0x0F) >= ((updateVal & 0x0F) + borrow_in)) {
        ActivateFlag(FLAG_AUXILIARY_CARRY);
    } else {
        DeActivateFlag(FLAG_AUXILIARY_CARRY);
    }
}

void UpdateFlagsZSPA_SUB(uint8_t oldVal, uint8_t updateVal, uint16_t result) {
    UpdateZSP((uint8_t)result);

    if ((oldVal & 0x0F) >= (updateVal & 0x0F)) {
        ActivateFlag(FLAG_AUXILIARY_CARRY);
    } else {
        DeActivateFlag(FLAG_AUXILIARY_CARRY);
    }
}

void UpdateFlagsZSP_Logical(uint8_t result, int acVal) {
    UpdateZSP(result);
    DeActivateFlag(FLAG_CARRY);
    if (acVal) {
        ActivateFlag(FLAG_AUXILIARY_CARRY);
    } else {
        DeActivateFlag(FLAG_AUXILIARY_CARRY);
    }
}

void UpdateFlagC_ADD(uint8_t oldVal, uint8_t updateVal, uint16_t result) {
    DeActivateFlag(FLAG_CARRY);

    if(result & 0x100) {
        ActivateFlag(FLAG_CARRY);
    }
}

void UpdateFlagC_SUB(uint8_t oldVal, uint8_t updateVal) {
    DeActivateFlag(FLAG_CARRY);

    if(oldVal < updateVal) {
        ActivateFlag(FLAG_CARRY);
    }
}

void MOV(Reg8 dst, Reg8 src) {
    registers[dst] = registers[src];
}

void MOV_FROM_M(Reg8 dst) {
    uint16_t addr = (registers[REG_H] << 8) | registers[REG_L];
    registers[dst] = MemRead(addr);
}

void MOV_TO_M(Reg8 src) {
    uint16_t addr = (registers[REG_H] << 8) | registers[REG_L];
    MemWrite(addr, registers[src]);
}

void MVI(Reg8 dst, uint8_t imm) {
    registers[dst] = imm;
}

void MVI_M(uint8_t imm) {
    uint16_t addr = (uint16_t)((registers[REG_H] << 8) | registers[REG_L]);
    MemWrite(addr, imm);
}

void LXI(RegPair rp, uint16_t imm) {
    if (rp == RP_SP) {
        SP = imm;
        return;
    }
    
    if (rp == RP_PSW) {
        registers[REG_A] = (uint8_t)(imm >> 8);
        flags = (uint8_t)(imm & 0xFF);
        return;
    }

    int idx = CalcRegisterIdx(rp);
    registers[idx] = (uint8_t)(imm >> 8);
    registers[idx + 1] = (uint8_t)(imm & 0xFF);
}

void LDA(uint16_t addr) {
    registers[REG_A] = MemRead(addr);
}

void STA(uint16_t addr) {
    MemWrite(addr, registers[REG_A]);
}

void LHLD(uint16_t addr) {
    registers[REG_L] = MemRead(addr);
    registers[REG_H] = MemRead(addr + 1);
}

void SHLD(uint16_t addr) {
    MemWrite(addr, registers[REG_L]);
    MemWrite(addr + 1, registers[REG_H]);
}

void LDAX(RegPair rp) {
    uint16_t addr = 0;
    
    if (rp == RP_BC) {
        addr = (uint16_t)((registers[REG_B] << 8) | registers[REG_C]);
    }

    if (rp == RP_DE) {
        addr = (uint16_t)((registers[REG_D] << 8) | registers[REG_E]);
    }

    registers[REG_A] = MemRead(addr);
}

void STAX(RegPair rp) {
    uint16_t addr = 0;
    
    if (rp == RP_BC) {
        addr = (uint16_t)((registers[REG_B] << 8) | registers[REG_C]);
    }

    if (rp == RP_DE) {
        addr = (uint16_t)((registers[REG_D] << 8) | registers[REG_E]);
    }
    
    MemWrite(addr, registers[REG_A]);
}

void XCHG(void) {
    uint8_t tempH = registers[REG_H], tempL = registers[REG_L];
    registers[REG_H] = registers[REG_D];
    registers[REG_L] = registers[REG_E];
    registers[REG_D] = tempH;
    registers[REG_E] = tempL;
}

void ADD(Reg8 src) {
    uint16_t result = registers[REG_A] + registers[src];
    
    UpdateFlagsZSPCA_ADD(registers[REG_A], registers[src], result);
    registers[REG_A] = (uint8_t)result;
}

void ADD_M(void) {
    uint16_t addr = (registers[REG_H] << 8) | registers[REG_L];
    uint8_t memVal = MemRead(addr);
    uint16_t result = registers[REG_A] + memVal;
    
    UpdateFlagsZSPCA_ADD(registers[REG_A], memVal, result);
    registers[REG_A] = (uint8_t)result;
}

void ADI(uint8_t imm) {
    uint16_t result = registers[REG_A] + imm;

    UpdateFlagsZSPCA_ADD(registers[REG_A], imm, result);
    registers[REG_A] = (uint8_t)result;
}

void ADC(Reg8 src) {
    uint8_t carry_in = IsFlagActive(FLAG_CARRY);
    uint16_t result = registers[REG_A] + registers[src] + carry_in;
    
    ClearFlags();

    uint8_t res8 = (uint8_t)result;

    if(res8 == 0) {
        ActivateFlag(FLAG_ZERO);
    }

    if(res8 & 0x80) {
        ActivateFlag(FLAG_SIGN);
    }

    if(Parity(res8)) {
        ActivateFlag(FLAG_PARITY);
    }

    if(result & 0x100) {
        ActivateFlag(FLAG_CARRY);
    }

    if(((registers[REG_A] & 0x0F) + (registers[src] & 0x0F) + carry_in) & 0x10) {
        ActivateFlag(FLAG_AUXILIARY_CARRY);
    }
    
    registers[REG_A] = res8;
}

void ADC_M(void) {
    uint16_t addr = (registers[REG_H] << 8) | registers[REG_L];
    uint8_t memVal = MemRead(addr);
    uint8_t carry_in = IsFlagActive(FLAG_CARRY);
    uint16_t result = registers[REG_A] + memVal + carry_in;
    
    ClearFlags();
    
    UpdateFlagsZSPCA_ADC(registers[REG_A], memVal, carry_in, result);
    registers[REG_A] = (uint8_t)result;
}

void ACI(uint8_t imm) {
    uint8_t carry_in = IsFlagActive(FLAG_CARRY);
    uint16_t result = registers[REG_A] + imm + carry_in;
    
    ClearFlags();
    
    UpdateFlagsZSPCA_ADC(registers[REG_A], imm, carry_in, result);
    registers[REG_A] = (uint8_t)result;
}

void SUB(Reg8 src) {
    uint16_t result = registers[REG_A] - registers[src];
    
    UpdateFlagsZSPCA_SUB(registers[REG_A], registers[src], result);
    registers[REG_A] = (uint8_t)result;
}

void SUB_M(void) {
    uint16_t addr = (registers[REG_H] << 8) | registers[REG_L];
    uint8_t memVal = MemRead(addr);
    uint16_t result = registers[REG_A] - memVal;
    
    UpdateFlagsZSPCA_SUB(registers[REG_A], memVal, result);
    registers[REG_A] = (uint8_t)result;
}

void SUI(uint8_t imm) {
    uint16_t result = registers[REG_A] - imm;
    
    UpdateFlagsZSPCA_SUB(registers[REG_A], imm, result);
    registers[REG_A] = (uint8_t)result;
}

void SBB(Reg8 src) {
    uint8_t borrow_in = IsFlagActive(FLAG_CARRY);
    uint16_t result = registers[REG_A] - registers[src] - borrow_in;
    
    ClearFlags();
    
    UpdateFlagsZSPCA_SBB(registers[REG_A], registers[src], borrow_in, result);
    registers[REG_A] = (uint8_t)result;
}

void SBB_M(void) {
    uint16_t addr = (registers[REG_H] << 8) | registers[REG_L];
    uint8_t memVal = MemRead(addr);
    uint8_t borrow_in = IsFlagActive(FLAG_CARRY);
    uint16_t result = registers[REG_A] - memVal - borrow_in;
    
    ClearFlags();
    
    UpdateFlagsZSPCA_SBB(registers[REG_A], memVal, borrow_in, result);
    registers[REG_A] = (uint8_t)result;
}

void SBI(uint8_t imm) {
    uint8_t borrow_in = IsFlagActive(FLAG_CARRY);
    uint16_t result = registers[REG_A] - imm - borrow_in;
    
    ClearFlags();
    
    UpdateFlagsZSPCA_SBB(registers[REG_A], imm, borrow_in, result);
    registers[REG_A] = (uint8_t)result;
}

void INR(Reg8 dst) {
    uint16_t result = registers[dst] + 1;
    
    UpdateFlagsZSPA_ADD(registers[dst], 1, result);
    registers[dst] = (uint8_t)result;
}

void INR_M(void) {
    uint16_t addr = (registers[REG_H] << 8) | registers[REG_L];
    uint8_t memVal = MemRead(addr);
    uint16_t result = memVal + 1;
    
    UpdateFlagsZSPA_ADD(memVal, 1, result);
    MemWrite(addr, (uint8_t)result);
}

void DCR(Reg8 dst) {
    uint16_t result = registers[dst] - 1;
    
    UpdateFlagsZSPA_SUB(registers[dst], 1, result);
    registers[dst] = (uint8_t)result;
}

void DCR_M(void) {
    uint16_t addr = (registers[REG_H] << 8) | registers[REG_L];
    uint8_t memVal = MemRead(addr);
    uint16_t result = memVal - 1;
    
    UpdateFlagsZSPA_SUB(memVal, 1, result);
    MemWrite(addr, (uint8_t)result);
}

void INX(RegPair rp) {
    if (rp == RP_SP) {
        SP++;
    } else {
        int idx = CalcRegisterIdx(rp);
        uint16_t result = ((registers[idx] << 8) | registers[idx + 1]) + 1;
        
        registers[idx]     = result >> 8;
        registers[idx + 1] = result & 0xFF;
    }
}

void DCX(RegPair rp) {
    if (rp == RP_SP) {
        SP--;
    } else {
        int idx = CalcRegisterIdx(rp);
        uint16_t result = ((registers[idx] << 8) | registers[idx + 1]) - 1;
        
        registers[idx]     = result >> 8;
        registers[idx + 1] = result & 0xFF;
    }
}

void DAD(RegPair rp) {
    uint32_t operand = 0;
    
    if (rp == RP_SP) {
        operand = SP;
    } else {
        int idx = CalcRegisterIdx(rp);
        operand = ((registers[idx] << 8) | registers[idx + 1]);
    }
    
    uint32_t result = ((registers[REG_H] << 8) | registers[REG_L]) + operand;
    
    /* DAD only affects Carry flag, preserves all other flags (Z, S, P, AC) */
    if(result > 0xFFFF) {
        ActivateFlag(FLAG_CARRY);
    }
    else {
        DeActivateFlag(FLAG_CARRY);
    }

    registers[REG_H] = (uint8_t)((result >> 8) & 0xFF);
    registers[REG_L] = (uint8_t)(result & 0xFF);
}

void DAA(void) {
    uint8_t oldA = registers[REG_A];
    uint8_t adjust = 0;
    uint8_t carry = IsFlagActive(FLAG_CARRY);
    uint8_t ac = IsFlagActive(FLAG_AUXILIARY_CARRY);

    if ((oldA & 0x0F) > 9 || ac) {
        adjust += 0x06;
    }
    
    if (oldA > 0x99 || carry) {
        adjust += 0x60;
        ActivateFlag(FLAG_CARRY);
    } else {
        DeActivateFlag(FLAG_CARRY);
    }

    uint16_t result = oldA + adjust;
    
    /* AC is set if there's a carry from bit 3 to 4 during the first adjustment */
    if (((oldA & 0x0F) + (adjust & 0x0F)) & 0x10) {
        ActivateFlag(FLAG_AUXILIARY_CARRY);
    }
    else {
        DeActivateFlag(FLAG_AUXILIARY_CARRY);
    }

    registers[REG_A] = (uint8_t)result;
    
    if (registers[REG_A] == 0) {
        ActivateFlag(FLAG_ZERO);
    }
    else {
        DeActivateFlag(FLAG_ZERO);
    }
    
    if (registers[REG_A] & 0x80) {
        ActivateFlag(FLAG_SIGN);
    }
    else {
        DeActivateFlag(FLAG_SIGN);
    }
    
    if (Parity(registers[REG_A])) {
        ActivateFlag(FLAG_PARITY);
    }
    else {
        DeActivateFlag(FLAG_PARITY);
    }
}

void ANA(Reg8 src) {
    uint8_t oldA = registers[REG_A];
    registers[REG_A] &= registers[src];
    
    ClearFlags();
    UpdateFlagsZSP_Logical(registers[REG_A], (oldA | registers[src]) & 0x08);
}

void ANA_M(void) {
    uint16_t addr = (registers[REG_H] << 8) | registers[REG_L];
    uint8_t memVal = MemRead(addr);
    uint8_t oldA = registers[REG_A];
    
    registers[REG_A] &= memVal;
    
    ClearFlags();
    UpdateFlagsZSP_Logical(registers[REG_A], (oldA | memVal) & 0x08);
}

void ANI(uint8_t imm) {
    uint8_t oldA = registers[REG_A];
    registers[REG_A] &= imm;
    
    ClearFlags();
    UpdateFlagsZSP_Logical(registers[REG_A], (oldA | imm) & 0x08);
}

void ORA(Reg8 src) {
    registers[REG_A] |= registers[src];
    
    ClearFlags();
    UpdateFlagsZSP_Logical(registers[REG_A], 0);
}

void ORA_M(void) {
    uint16_t addr = (registers[REG_H] << 8) | registers[REG_L];
    registers[REG_A] |= MemRead(addr);
    
    ClearFlags();
    UpdateFlagsZSP_Logical(registers[REG_A], 0);
}

void ORI(uint8_t imm) {
    registers[REG_A] |= imm;
    
    ClearFlags();
    UpdateFlagsZSP_Logical(registers[REG_A], 0);
}

void XRA(Reg8 src) {
    registers[REG_A] ^= registers[src];
    
    ClearFlags();
    UpdateFlagsZSP_Logical(registers[REG_A], 0);
}

void XRA_M(void) {
    uint16_t addr = (registers[REG_H] << 8) | registers[REG_L];
    registers[REG_A] ^= MemRead(addr);
    
    ClearFlags();
    UpdateFlagsZSP_Logical(registers[REG_A], 0);
}

void XRI(uint8_t imm) {
    registers[REG_A] ^= imm;
    
    ClearFlags();
    UpdateFlagsZSP_Logical(registers[REG_A], 0);
}

void CMP(Reg8 src) {
    uint16_t result = registers[REG_A] - registers[src];
    UpdateFlagsZSPCA_SUB(registers[REG_A], registers[src], result);
}

void CMP_M(void) {
    uint16_t addr = (registers[REG_H] << 8) | registers[REG_L];
    uint8_t memVal = MemRead(addr);
    uint16_t result = registers[REG_A] - memVal;
    
    UpdateFlagsZSPCA_SUB(registers[REG_A], memVal, result);
}

void CPI(uint8_t imm) {
    uint16_t result = registers[REG_A] - imm;
    UpdateFlagsZSPCA_SUB(registers[REG_A], imm, result);
}

void RLC(void) {
    uint8_t msb = (uint8_t)((registers[REG_A] & 0x80) >> 7);
    registers[REG_A] = (uint8_t)((registers[REG_A] << 1) | msb);
    
    if (msb) {
        ActivateFlag(FLAG_CARRY);
    } else {
        DeActivateFlag(FLAG_CARRY);
    }
}

void RRC(void) {
    uint8_t lsb = (uint8_t)(registers[REG_A] & 0x01);
    registers[REG_A] = (uint8_t)((registers[REG_A] >> 1) | (lsb << 7));
    
    if (lsb) {
        ActivateFlag(FLAG_CARRY);
    } else {
        DeActivateFlag(FLAG_CARRY);
    }
}

void RAL(void) {
    uint8_t oldCarry = IsFlagActive(FLAG_CARRY);
    uint8_t msb = (uint8_t)((registers[REG_A] & 0x80) >> 7);
    
    registers[REG_A] = (uint8_t)((registers[REG_A] << 1) | oldCarry);
    
    if (msb) {
        ActivateFlag(FLAG_CARRY);
    } else {
        DeActivateFlag(FLAG_CARRY);
    }
}

void RAR(void) {
    uint8_t oldCarry = IsFlagActive(FLAG_CARRY);
    uint8_t lsb = (uint8_t)(registers[REG_A] & 0x01);
    
    registers[REG_A] = (uint8_t)((registers[REG_A] >> 1) | (oldCarry << 7));
    
    if (lsb) {
        ActivateFlag(FLAG_CARRY);
    } else {
        DeActivateFlag(FLAG_CARRY);
    }
}

void CMA(void) {
    registers[REG_A] = ~registers[REG_A];
}

void CMC(void) {
    flags ^= 1;
}

void STC(void) {
    ActivateFlag(FLAG_CARRY);
}

void JMP(uint16_t addr) {
    PC = addr;
}

void JCC(Cond cond, uint16_t addr) {
    if (CheckCondition(cond)) {
        PC = addr;
    }
}

void CALL(uint16_t addr) {
    MemWrite(--SP, (PC >> 8) & 0xFF);
    MemWrite(--SP, PC & 0xFF);
    PC = addr;
}

void CCC(Cond cond, uint16_t addr) {
    if (CheckCondition(cond)) {
        CALL(addr);
    }
}

void RET(void) {
    uint8_t low = MemRead(SP++);
    uint8_t high = MemRead(SP++);
    PC = ((uint16_t)high << 8) | low;
}

void RCC(Cond cond) {
    if (CheckCondition(cond)) {
        RET();
    }
}

void RST(uint8_t rst) {
    MemWrite(--SP, (PC >> 8) & 0xFF);
    MemWrite(--SP, PC & 0xFF);
    PC = 8 * rst;
}

void PCHL(void) {
    PC = (uint16_t)((registers[REG_H] << 8) | registers[REG_L]);
}

void PUSH(RegPair rp) {
    uint8_t high, low;
    
    if (rp == RP_SP) {
        high = (uint8_t)(SP >> 8);
        low = (uint8_t)(SP & 0xFF);
    } else {
        int idx = CalcRegisterIdx(rp);
        high = registers[idx];
        low  = registers[idx + 1];
    }

    MemWrite(--SP, high);
    MemWrite(--SP, low);
}

void PUSH_PSW(void) {
    uint8_t psw = 0x02;
    psw |= (flags & (1 << FLAG_SIGN)) ? 0x80 : 0;
    psw |= (flags & (1 << FLAG_ZERO)) ? 0x40 : 0;
    psw |= (flags & (1 << FLAG_AUXILIARY_CARRY)) ? 0x10 : 0;
    psw |= (flags & (1 << FLAG_PARITY)) ? 0x04 : 0;
    psw |= (flags & (1 << FLAG_CARRY)) ? 0x01 : 0;

    MemWrite(--SP, registers[REG_A]);
    MemWrite(--SP, psw);
}

void POP(RegPair rp) {
    uint8_t low  = MemRead(SP++);
    uint8_t high = MemRead(SP++);
    
    if (rp == RP_SP) {
        SP = (uint16_t)((high << 8) | low);
    } else {
        int idx = CalcRegisterIdx(rp);
        registers[idx]     = high;
        registers[idx + 1] = low;
    }
}

void POP_PSW(void) {
    uint8_t psw = MemRead(SP++);
    registers[REG_A] = MemRead(SP++);
    
    flags = 0x02;
    
    if (psw & 0x80) {
        ActivateFlag(FLAG_SIGN);
    }
    
    if (psw & 0x40) {
        ActivateFlag(FLAG_ZERO);
    }
    
    if (psw & 0x10) {
        ActivateFlag(FLAG_AUXILIARY_CARRY);
    }
    
    if (psw & 0x04) {
        ActivateFlag(FLAG_PARITY);
    }
    
    if (psw & 0x01) {
        ActivateFlag(FLAG_CARRY);
    }
}

void XTHL(void) {
    uint8_t tempL = registers[REG_L];
    uint8_t tempH = registers[REG_H];

    registers[REG_L] = MemRead(SP);
    registers[REG_H] = MemRead(SP + 1);

    MemWrite(SP, tempL);
    MemWrite(SP + 1, tempH);
}

void SPHL(void) {
    SP = (uint16_t)((registers[REG_H] << 8) | registers[REG_L]);
}

void IN(uint8_t port) {
    registers[REG_A] = IORead(port);
}

void OUT(uint8_t port) {
    IOWrite(port, registers[REG_A]);
}

void EI(void) {
    interruptsEnabled = TRUE;
}

void DI(void) {
    interruptsEnabled = FALSE;
}

void HLT(void) {
    halted = TRUE;
}

void NOP(void) {}

/*
    Jump table.
    PROS: no humoungous switch statement
    CONS: humongous list of opcode functions on-top of normal instruction handlers
*/
OpcodeHandler opcodeTable[256];

static int op_00(void) {
    NOP();
    return 4;
}

static int op_01(void) {
    LXI(RP_BC, FetchWord());
    return 10;
}

static int op_02(void) {
    STAX(RP_BC);
    return 7;
}

static int op_03(void) {
    INX(RP_BC);
    return 5;
}

static int op_04(void) {
    INR(REG_B);
    return 5;
}

static int op_05(void) {
    DCR(REG_B);
    return 5;
}

static int op_06(void) {
    MVI(REG_B, FetchByte());
    return 7;
}

static int op_07(void) {
    RLC();
    return 4;
}

static int op_08(void) {
    NOP();
    return 4;
}

static int op_09(void) {
    DAD(RP_BC);
    return 10;
}

static int op_0a(void) {
    LDAX(RP_BC);
    return 7;
}

static int op_0b(void) {
    DCX(RP_BC);
    return 5;
}

static int op_0c(void) {
    INR(REG_C);
    return 5;
}

static int op_0d(void) {
    DCR(REG_C);
    return 5;
}

static int op_0e(void) {
    MVI(REG_C, FetchByte());
    return 7;
}

static int op_0f(void) {
    RRC();
    return 4;
}

static int op_10(void) {
    NOP();
    return 4;
}

static int op_11(void) {
    LXI(RP_DE, FetchWord());
    return 10;
}

static int op_12(void) {
    STAX(RP_DE);
    return 7;
}

static int op_13(void) {
    INX(RP_DE);
    return 5;
}

static int op_14(void) {
    INR(REG_D);
    return 5;
}

static int op_15(void) {
    DCR(REG_D);
    return 5;
}

static int op_16(void) {
    MVI(REG_D, FetchByte());
    return 7;
}

static int op_17(void) {
    RAL();
    return 4;
}

static int op_18(void) {
    NOP();
    return 4;
}

static int op_19(void) {
    DAD(RP_DE);
    return 10;
}

static int op_1a(void) {
    LDAX(RP_DE);
    return 7;
}

static int op_1b(void) {
    DCX(RP_DE);
    return 5;
}

static int op_1c(void) {
    INR(REG_E);
    return 5;
}

static int op_1d(void) {
    DCR(REG_E);
    return 5;
}

static int op_1e(void) {
    MVI(REG_E, FetchByte());
    return 7;
}

static int op_1f(void) {
    RAR();
    return 4;
}

static int op_20(void) {
    NOP();
    return 4;
}

static int op_21(void) {
    LXI(RP_HL, FetchWord());
    return 10;
}

static int op_22(void) {
    SHLD(FetchWord());
    return 16;
}

static int op_23(void) {
    INX(RP_HL);
    return 5;
}

static int op_24(void) {
    INR(REG_H);
    return 5;
}

static int op_25(void) {
    DCR(REG_H);
    return 5;
}

static int op_26(void) {
    MVI(REG_H, FetchByte());
    return 7;
}

static int op_27(void) {
    DAA();
    return 4;
}

static int op_28(void) {
    NOP();
    return 4;
}

static int op_29(void) {
    DAD(RP_HL);
    return 10;
}

static int op_2a(void) {
    LHLD(FetchWord());
    return 16;
}

static int op_2b(void) {
    DCX(RP_HL);
    return 5;
}

static int op_2c(void) {
    INR(REG_L);
    return 5;
}

static int op_2d(void) {
    DCR(REG_L);
    return 5;
}

static int op_2e(void) {
    MVI(REG_L, FetchByte());
    return 7;
}

static int op_2f(void) {
    CMA();
    return 4;
}

static int op_30(void) {
    NOP();
    return 4;
}

static int op_31(void) {
    LXI(RP_SP, FetchWord());
    return 10;
}

static int op_32(void) {
    STA(FetchWord());
    return 13;
}

static int op_33(void) {
    INX(RP_SP);
    return 5;
}

static int op_34(void) {
    INR_M();
    return 10;
}

static int op_35(void) {
    DCR_M();
    return 10;
}

static int op_36(void) {
    MVI_M(FetchByte());
    return 10;
}

static int op_37(void) {
    STC();
    return 4;
}

static int op_38(void) {
    NOP();
    return 4;
}

static int op_39(void) {
    DAD(RP_SP);
    return 10;
}

static int op_3a(void) {
    LDA(FetchWord());
    return 13;
}

static int op_3b(void) {
    DCX(RP_SP);
    return 5;
}

static int op_3c(void) {
    INR(REG_A);
    return 5;
}

static int op_3d(void) {
    DCR(REG_A);
    return 5;
}

static int op_3e(void) {
    MVI(REG_A, FetchByte());
    return 7;
}

static int op_3f(void) {
    CMC();
    return 4;
}

static int op_40(void) {
    MOV(REG_B, REG_B);
    return 5;
}

static int op_41(void) {
    MOV(REG_B, REG_C);
    return 5;
}

static int op_42(void) {
    MOV(REG_B, REG_D);
    return 5;
}

static int op_43(void) {
    MOV(REG_B, REG_E);
    return 5;
}

static int op_44(void) {
    MOV(REG_B, REG_H);
    return 5;
}

static int op_45(void) {
    MOV(REG_B, REG_L);
    return 5;
}

static int op_46(void) {
    MOV_FROM_M(REG_B);
    return 7;
}

static int op_47(void) {
    MOV(REG_B, REG_A);
    return 5;
}

static int op_48(void) {
    MOV(REG_C, REG_B);
    return 5;
}

static int op_49(void) {
    MOV(REG_C, REG_C);
    return 5;
}

static int op_4a(void) {
    MOV(REG_C, REG_D);
    return 5;
}

static int op_4b(void) {
    MOV(REG_C, REG_E);
    return 5;
}

static int op_4c(void) {
    MOV(REG_C, REG_H);
    return 5;
}

static int op_4d(void) {
    MOV(REG_C, REG_L);
    return 5;
}

static int op_4e(void) {
    MOV_FROM_M(REG_C);
    return 7;
}

static int op_4f(void) {
    MOV(REG_C, REG_A);
    return 5;
}


static int op_50(void) {
    MOV(REG_D, REG_B);
    return 5;
}

static int op_51(void) {
    MOV(REG_D, REG_C);
    return 5;
}

static int op_52(void) {
    MOV(REG_D, REG_D);
    return 5;
}

static int op_53(void) {
    MOV(REG_D, REG_E);
    return 5;
}

static int op_54(void) {
    MOV(REG_D, REG_H);
    return 5;
}

static int op_55(void) {
    MOV(REG_D, REG_L);
    return 5;
}

static int op_56(void) {
    MOV_FROM_M(REG_D);
    return 7;
}

static int op_57(void) {
    MOV(REG_D, REG_A);
    return 5;
}

static int op_58(void) {
    MOV(REG_E, REG_B);
    return 5;
}

static int op_59(void) {
    MOV(REG_E, REG_C);
    return 5;
}

static int op_5a(void) {
    MOV(REG_E, REG_D);
    return 5;
}

static int op_5b(void) {
    MOV(REG_E, REG_E);
    return 5;
}

static int op_5c(void) {
    MOV(REG_E, REG_H);
    return 5;
}

static int op_5d(void) {
    MOV(REG_E, REG_L);
    return 5;
}

static int op_5e(void) {
    MOV_FROM_M(REG_E);
    return 7;
}

static int op_5f(void) {
    MOV(REG_E, REG_A);
    return 5;
}

static int op_60(void) {
    MOV(REG_H, REG_B);
    return 5;
}

static int op_61(void) {
    MOV(REG_H, REG_C);
    return 5;
}

static int op_62(void) {
    MOV(REG_H, REG_D);
    return 5;
}

static int op_63(void) {
    MOV(REG_H, REG_E);
    return 5;
}

static int op_64(void) {
    MOV(REG_H, REG_H);
    return 5;
}

static int op_65(void) {
    MOV(REG_H, REG_L);
    return 5;
}

static int op_66(void) {
    MOV_FROM_M(REG_H);
    return 7;
}

static int op_67(void) {
    MOV(REG_H, REG_A);
    return 5;
}

static int op_68(void) {
    MOV(REG_L, REG_B);
    return 5;
}

static int op_69(void) {
    MOV(REG_L, REG_C);
    return 5;
}

static int op_6a(void) {
    MOV(REG_L, REG_D);
    return 5;
}

static int op_6b(void) {
    MOV(REG_L, REG_E);
    return 5;
}

static int op_6c(void) {
    MOV(REG_L, REG_H);
    return 5;
}

static int op_6d(void) {
    MOV(REG_L, REG_L);
    return 5;
}

static int op_6e(void) {
    MOV_FROM_M(REG_L);
    return 7;
}

static int op_6f(void) {
    MOV(REG_L, REG_A);
    return 5;
}

static int op_70(void) {
    MOV_TO_M(REG_B);
    return 7;
}

static int op_71(void) {
    MOV_TO_M(REG_C);
    return 7;
}

static int op_72(void) {
    MOV_TO_M(REG_D);
    return 7;
}

static int op_73(void) {
    MOV_TO_M(REG_E);
    return 7;
}

static int op_74(void) {
    MOV_TO_M(REG_H);
    return 7;
}

static int op_75(void) {
    MOV_TO_M(REG_L);
    return 7;
}

static int op_76(void) {
    HLT();
    return 7;
}

static int op_77(void) {
    MOV_TO_M(REG_A);
    return 7;
}

static int op_78(void) {
    MOV(REG_A, REG_B);
    return 5;
}

static int op_79(void) {
    MOV(REG_A, REG_C);
    return 5;
}

static int op_7a(void) {
    MOV(REG_A, REG_D);
    return 5;
}

static int op_7b(void) {
    MOV(REG_A, REG_E);
    return 5;
}

static int op_7c(void) {
    MOV(REG_A, REG_H);
    return 5;
}

static int op_7d(void) {
    MOV(REG_A, REG_L);
    return 5;
}

static int op_7e(void) {
    MOV_FROM_M(REG_A);
    return 7;
}

static int op_7f(void) {
    MOV(REG_A, REG_A);
    return 5;
}

static int op_80(void) {
    ADD(REG_B);
    return 4;
}

static int op_81(void) {
    ADD(REG_C);
    return 4;
}

static int op_82(void) {
    ADD(REG_D);
    return 4;
}

static int op_83(void) {
    ADD(REG_E);
    return 4;
}

static int op_84(void) {
    ADD(REG_H);
    return 4;
}

static int op_85(void) {
    ADD(REG_L);
    return 4;
}

static int op_86(void) {
    ADD_M();
    return 7;
}

static int op_87(void) {
    ADD(REG_A);
    return 4;
}

static int op_88(void) {
    ADC(REG_B);
    return 4;
}

static int op_89(void) {
    ADC(REG_C);
    return 4;
}

static int op_8a(void) {
    ADC(REG_D);
    return 4;
}

static int op_8b(void) {
    ADC(REG_E);
    return 4;
}

static int op_8c(void) {
    ADC(REG_H);
    return 4;
}

static int op_8d(void) {
    ADC(REG_L);
    return 4;
}

static int op_8e(void) {
    ADC_M();
    return 7;
}

static int op_8f(void) {
    ADC(REG_A);
    return 4;
}

static int op_90(void) {
    SUB(REG_B);
    return 4;
}

static int op_91(void) {
    SUB(REG_C);
    return 4;
}

static int op_92(void) {
    SUB(REG_D);
    return 4;
}

static int op_93(void) {
    SUB(REG_E);
    return 4;
}

static int op_94(void) {
    SUB(REG_H);
    return 4;
}

static int op_95(void) {
    SUB(REG_L);
    return 4;
}

static int op_96(void) {
    SUB_M();
    return 7;
}

static int op_97(void) {
    SUB(REG_A);
    return 4;
}

static int op_98(void) {
    SBB(REG_B);
    return 4;
}

static int op_99(void) {
    SBB(REG_C);
    return 4;
}

static int op_9a(void) {
    SBB(REG_D);
    return 4;
}

static int op_9b(void) {
    SBB(REG_E);
    return 4;
}

static int op_9c(void) {
    SBB(REG_H);
    return 4;
}

static int op_9d(void) {
    SBB(REG_L);
    return 4;
}

static int op_9e(void) {
    SBB_M();
    return 7;
}

static int op_9f(void) {
    SBB(REG_A);
    return 4;
}

static int op_a0(void) {
    ANA(REG_B);
    return 4;
}

static int op_a1(void) {
    ANA(REG_C);
    return 4;
}

static int op_a2(void) {
    ANA(REG_D);
    return 4;
}

static int op_a3(void) {
    ANA(REG_E);
    return 4;
}

static int op_a4(void) {
    ANA(REG_H);
    return 4;
}

static int op_a5(void) {
    ANA(REG_L);
    return 4;
}

static int op_a6(void) {
    ANA_M();
    return 7;
}

static int op_a7(void) {
    ANA(REG_A);
    return 4;
}

static int op_a8(void) {
    XRA(REG_B);
    return 4;
}

static int op_a9(void) {
    XRA(REG_C);
    return 4;
}

static int op_aa(void) {
    XRA(REG_D);
    return 4;
}

static int op_ab(void) {
    XRA(REG_E);
    return 4;
}

static int op_ac(void) {
    XRA(REG_H);
    return 4;
}

static int op_ad(void) {
    XRA(REG_L);
    return 4;
}

static int op_ae(void) {
    XRA_M();
    return 7;
}

static int op_af(void) {
    XRA(REG_A);
    return 4;
}


static int op_b0(void) {
    ORA(REG_B);
    return 4;
}

static int op_b1(void) {
    ORA(REG_C);
    return 4;
}

static int op_b2(void) {
    ORA(REG_D);
    return 4;
}

static int op_b3(void) {
    ORA(REG_E);
    return 4;
}

static int op_b4(void) {
    ORA(REG_H);
    return 4;
}

static int op_b5(void) {
    ORA(REG_L);
    return 4;
}

static int op_b6(void) {
    ORA_M();
    return 7;
}

static int op_b7(void) {
    ORA(REG_A);
    return 4;
}

static int op_b8(void) {
    CMP(REG_B);
    return 4;
}

static int op_b9(void) {
    CMP(REG_C);
    return 4;
}

static int op_ba(void) {
    CMP(REG_D);
    return 4;
}

static int op_bb(void) {
    CMP(REG_E);
    return 4;
}

static int op_bc(void) {
    CMP(REG_H);
    return 4;
}

static int op_bd(void) {
    CMP(REG_L);
    return 4;
}

static int op_be(void) {
    CMP_M();
    return 7;
}

static int op_bf(void) {
    CMP(REG_A);
    return 4;
}

static int op_c0(void) {
    int c = IsFlagActive(FLAG_ZERO) ? 5 : 11;
    RCC(COND_NZ);
    
    return c;
}

static int op_c1(void) {
    POP(RP_BC);
    return 10;
}

static int op_c2(void) {
    JCC(COND_NZ, FetchWord());
    return 10;
}

static int op_c3(void) {
    JMP(FetchWord());
    return 10;
}

static int op_c4(void) {
    int c = IsFlagActive(FLAG_ZERO) ? 11 : 17;
    CCC(COND_NZ, FetchWord());
    
    return c;
}

static int op_c5(void) {
    PUSH(RP_BC);
    return 11;
}

static int op_c6(void) {
    ADI(FetchByte());
    return 7;
}

static int op_c7(void) {
    RST(0);
    return 11;
}

static int op_c8(void) {
    int c = IsFlagActive(FLAG_ZERO) ? 11 : 5;
    RCC(COND_Z);
    
    return c;
}

static int op_c9(void) {
    RET();
    return 10;
}

static int op_ca(void) {
    JCC(COND_Z, FetchWord());
    return 10;
}

static int op_cb(void) {
    JMP(FetchWord());
    return 10;
}

static int op_cc(void) {
    int c = IsFlagActive(FLAG_ZERO) ? 17 : 11;
    CCC(COND_Z, FetchWord());
    
    return c;
}

static int op_cd(void) {
    CALL(FetchWord());
    return 17;
}

static int op_ce(void) {
    ACI(FetchByte());
    return 7;
}

static int op_cf(void) {
    RST(1);
    return 11;
}

static int op_d0(void) {
    int c = IsFlagActive(FLAG_CARRY) ? 5 : 11;
    RCC(COND_NC);
    
    return c;
}

static int op_d1(void) {
    POP(RP_DE);
    return 10;
}

static int op_d2(void) {
    JCC(COND_NC, FetchWord());
    return 10;
}

static int op_d3(void) {
    OUT(FetchByte());
    return 10;
}

static int op_d4(void) {
    int c = IsFlagActive(FLAG_CARRY) ? 11 : 17;
    CCC(COND_NC, FetchWord());
    
    return c;
}

static int op_d5(void) {
    PUSH(RP_DE);
    return 11;
}

static int op_d6(void) {
    SUI(FetchByte());
    return 7;
}

static int op_d7(void) {
    RST(2);
    return 11;
}

static int op_d8(void) {
    int c = IsFlagActive(FLAG_CARRY) ? 11 : 5;
    RCC(COND_C);
    
    return c;
}

static int op_d9(void) {
    RET();
    return 10;
}

static int op_da(void) {
    JCC(COND_C, FetchWord());
    return 10;
}

static int op_db(void) {
    IN(FetchByte());
    return 10;
}

static int op_dc(void) {
    int c = IsFlagActive(FLAG_CARRY) ? 17 : 11;
    CCC(COND_C, FetchWord());
    
    return c;
}

static int op_dd(void) {
    CALL(FetchWord());
    return 17;
}

static int op_de(void) {
    SBI(FetchByte());
    return 7;
}

static int op_df(void) {
    RST(3);
    return 11;
}

static int op_e0(void) {
    int c = IsFlagActive(FLAG_PARITY) ? 5 : 11;
    RCC(COND_PO);
    
    return c;
}

static int op_e1(void) {
    POP(RP_HL);
    return 10;
}

static int op_e2(void) {
    JCC(COND_PO, FetchWord());
    return 10;
}

static int op_e3(void) {
    XTHL();
    return 18;
}

static int op_e4(void) {
    int c = IsFlagActive(FLAG_PARITY) ? 11 : 17;
    CCC(COND_PO, FetchWord());
    
    return c;
}

static int op_e5(void) {
    PUSH(RP_HL);
    return 11;
}

static int op_e6(void) {
    ANI(FetchByte());
    return 7;
}

static int op_e7(void) {
    RST(4);
    return 11;
}

static int op_e8(void) {
    int c = IsFlagActive(FLAG_PARITY) ? 11 : 5;
    RCC(COND_PE);
    
    return c;
}

static int op_e9(void) {
    PCHL();
    return 5;
}

static int op_ea(void) {
    JCC(COND_PE, FetchWord());
    return 10;
}

static int op_eb(void) {
    XCHG();
    return 5;
}

static int op_ec(void) {
    int c = IsFlagActive(FLAG_PARITY) ? 17 : 11;
    CCC(COND_PE, FetchWord());
    
    return c;
}

static int op_ed(void) {
    CALL(FetchWord());
    return 17;
}

static int op_ee(void) {
    XRI(FetchByte());
    return 7;
}

static int op_ef(void) {
    RST(5);
    return 11;
}

static int op_f0(void) {
    int c = IsFlagActive(FLAG_SIGN) ? 5 : 11;
    RCC(COND_P);
    return c;
}

static int op_f1(void) {
    POP_PSW();
    return 10;
}

static int op_f2(void) {
    JCC(COND_P, FetchWord());
    return 10;
}

static int op_f3(void) {
    DI();
    return 4;
}

static int op_f4(void) {
    int c = IsFlagActive(FLAG_SIGN) ? 11 : 17;
    CCC(COND_P, FetchWord());
    
    return c;
}

static int op_f5(void) {
    PUSH_PSW();
    return 11;
}

static int op_f6(void) {
    ORI(FetchByte());
    return 7;
}

static int op_f7(void) {
    RST(6);
    return 11;
}

static int op_f8(void) {
    int c = IsFlagActive(FLAG_SIGN) ? 11 : 5;
    RCC(COND_M);
    
    return c;
}

static int op_f9(void) {
    SPHL();
    return 5;
}

static int op_fa(void) {
    JCC(COND_M, FetchWord());
    return 10;
}

static int op_fb(void) {
    EI();
    return 4;
}

static int op_fc(void) {
    int c = IsFlagActive(FLAG_SIGN) ? 17 : 11;
    CCC(COND_M, FetchWord());
    
    return c;
}

static int op_fd(void) {
    CALL(FetchWord());
    return 17;
}

static int op_fe(void) {
    CPI(FetchByte());
    return 7;
}

static int op_ff(void) {
    RST(7);
    return 11;
}

/*
    Made with the help of A.I
    I am too lazy to write this by myself
*/
void OpInit(void) {
    opcodeTable[0x00] = op_00; opcodeTable[0x01] = op_01; opcodeTable[0x02] = op_02; opcodeTable[0x03] = op_03;
    opcodeTable[0x04] = op_04; opcodeTable[0x05] = op_05; opcodeTable[0x06] = op_06; opcodeTable[0x07] = op_07;
    opcodeTable[0x08] = op_08; opcodeTable[0x09] = op_09; opcodeTable[0x0a] = op_0a; opcodeTable[0x0b] = op_0b;
    opcodeTable[0x0c] = op_0c; opcodeTable[0x0d] = op_0d; opcodeTable[0x0e] = op_0e; opcodeTable[0x0f] = op_0f;
    opcodeTable[0x10] = op_10; opcodeTable[0x11] = op_11; opcodeTable[0x12] = op_12; opcodeTable[0x13] = op_13;
    opcodeTable[0x14] = op_14; opcodeTable[0x15] = op_15; opcodeTable[0x16] = op_16; opcodeTable[0x17] = op_17;
    opcodeTable[0x18] = op_18; opcodeTable[0x19] = op_19; opcodeTable[0x1a] = op_1a; opcodeTable[0x1b] = op_1b;
    opcodeTable[0x1c] = op_1c; opcodeTable[0x1d] = op_1d; opcodeTable[0x1e] = op_1e; opcodeTable[0x1f] = op_1f;
    opcodeTable[0x20] = op_20; opcodeTable[0x21] = op_21; opcodeTable[0x22] = op_22; opcodeTable[0x23] = op_23;
    opcodeTable[0x24] = op_24; opcodeTable[0x25] = op_25; opcodeTable[0x26] = op_26; opcodeTable[0x27] = op_27;
    opcodeTable[0x28] = op_28; opcodeTable[0x29] = op_29; opcodeTable[0x2a] = op_2a; opcodeTable[0x2b] = op_2b;
    opcodeTable[0x2c] = op_2c; opcodeTable[0x2d] = op_2d; opcodeTable[0x2e] = op_2e; opcodeTable[0x2f] = op_2f;
    opcodeTable[0x30] = op_30; opcodeTable[0x31] = op_31; opcodeTable[0x32] = op_32; opcodeTable[0x33] = op_33;
    opcodeTable[0x34] = op_34; opcodeTable[0x35] = op_35; opcodeTable[0x36] = op_36; opcodeTable[0x37] = op_37;
    opcodeTable[0x38] = op_38; opcodeTable[0x39] = op_39; opcodeTable[0x3a] = op_3a; opcodeTable[0x3b] = op_3b;
    opcodeTable[0x3c] = op_3c; opcodeTable[0x3d] = op_3d; opcodeTable[0x3e] = op_3e; opcodeTable[0x3f] = op_3f;
    opcodeTable[0x40] = op_40; opcodeTable[0x41] = op_41; opcodeTable[0x42] = op_42; opcodeTable[0x43] = op_43;
    opcodeTable[0x44] = op_44; opcodeTable[0x45] = op_45; opcodeTable[0x46] = op_46; opcodeTable[0x47] = op_47;
    opcodeTable[0x48] = op_48; opcodeTable[0x49] = op_49; opcodeTable[0x4a] = op_4a; opcodeTable[0x4b] = op_4b;
    opcodeTable[0x4c] = op_4c; opcodeTable[0x4d] = op_4d; opcodeTable[0x4e] = op_4e; opcodeTable[0x4f] = op_4f;
    opcodeTable[0x50] = op_50; opcodeTable[0x51] = op_51; opcodeTable[0x52] = op_52; opcodeTable[0x53] = op_53;
    opcodeTable[0x54] = op_54; opcodeTable[0x55] = op_55; opcodeTable[0x56] = op_56; opcodeTable[0x57] = op_57;
    opcodeTable[0x58] = op_58; opcodeTable[0x59] = op_59; opcodeTable[0x5a] = op_5a; opcodeTable[0x5b] = op_5b;
    opcodeTable[0x5c] = op_5c; opcodeTable[0x5d] = op_5d; opcodeTable[0x5e] = op_5e; opcodeTable[0x5f] = op_5f;
    opcodeTable[0x60] = op_60; opcodeTable[0x61] = op_61; opcodeTable[0x62] = op_62; opcodeTable[0x63] = op_63;
    opcodeTable[0x64] = op_64; opcodeTable[0x65] = op_65; opcodeTable[0x66] = op_66; opcodeTable[0x67] = op_67;
    opcodeTable[0x68] = op_68; opcodeTable[0x69] = op_69; opcodeTable[0x6a] = op_6a; opcodeTable[0x6b] = op_6b;
    opcodeTable[0x6c] = op_6c; opcodeTable[0x6d] = op_6d; opcodeTable[0x6e] = op_6e; opcodeTable[0x6f] = op_6f;
    opcodeTable[0x70] = op_70; opcodeTable[0x71] = op_71; opcodeTable[0x72] = op_72; opcodeTable[0x73] = op_73;
    opcodeTable[0x74] = op_74; opcodeTable[0x75] = op_75; opcodeTable[0x76] = op_76; opcodeTable[0x77] = op_77;
    opcodeTable[0x78] = op_78; opcodeTable[0x79] = op_79; opcodeTable[0x7a] = op_7a; opcodeTable[0x7b] = op_7b;
    opcodeTable[0x7c] = op_7c; opcodeTable[0x7d] = op_7d; opcodeTable[0x7e] = op_7e; opcodeTable[0x7f] = op_7f;
    opcodeTable[0x80] = op_80; opcodeTable[0x81] = op_81; opcodeTable[0x82] = op_82; opcodeTable[0x83] = op_83;
    opcodeTable[0x84] = op_84; opcodeTable[0x85] = op_85; opcodeTable[0x86] = op_86; opcodeTable[0x87] = op_87;
    opcodeTable[0x88] = op_88; opcodeTable[0x89] = op_89; opcodeTable[0x8a] = op_8a; opcodeTable[0x8b] = op_8b;
    opcodeTable[0x8c] = op_8c; opcodeTable[0x8d] = op_8d; opcodeTable[0x8e] = op_8e; opcodeTable[0x8f] = op_8f;
    opcodeTable[0x90] = op_90; opcodeTable[0x91] = op_91; opcodeTable[0x92] = op_92; opcodeTable[0x93] = op_93;
    opcodeTable[0x94] = op_94; opcodeTable[0x95] = op_95; opcodeTable[0x96] = op_96; opcodeTable[0x97] = op_97;
    opcodeTable[0x98] = op_98; opcodeTable[0x99] = op_99; opcodeTable[0x9a] = op_9a; opcodeTable[0x9b] = op_9b;
    opcodeTable[0x9c] = op_9c; opcodeTable[0x9d] = op_9d; opcodeTable[0x9e] = op_9e; opcodeTable[0x9f] = op_9f;
    opcodeTable[0xa0] = op_a0; opcodeTable[0xa1] = op_a1; opcodeTable[0xa2] = op_a2; opcodeTable[0xa3] = op_a3;
    opcodeTable[0xa4] = op_a4; opcodeTable[0xa5] = op_a5; opcodeTable[0xa6] = op_a6; opcodeTable[0xa7] = op_a7;
    opcodeTable[0xa8] = op_a8; opcodeTable[0xa9] = op_a9; opcodeTable[0xaa] = op_aa; opcodeTable[0xab] = op_ab;
    opcodeTable[0xac] = op_ac; opcodeTable[0xad] = op_ad; opcodeTable[0xae] = op_ae; opcodeTable[0xaf] = op_af;
    opcodeTable[0xb0] = op_b0; opcodeTable[0xb1] = op_b1; opcodeTable[0xb2] = op_b2; opcodeTable[0xb3] = op_b3;
    opcodeTable[0xb4] = op_b4; opcodeTable[0xb5] = op_b5; opcodeTable[0xb6] = op_b6; opcodeTable[0xb7] = op_b7;
    opcodeTable[0xb8] = op_b8; opcodeTable[0xb9] = op_b9; opcodeTable[0xba] = op_ba; opcodeTable[0xbb] = op_bb;
    opcodeTable[0xbc] = op_bc; opcodeTable[0xbd] = op_bd; opcodeTable[0xbe] = op_be; opcodeTable[0xbf] = op_bf;
    opcodeTable[0xc0] = op_c0; opcodeTable[0xc1] = op_c1; opcodeTable[0xc2] = op_c2; opcodeTable[0xc3] = op_c3;
    opcodeTable[0xc4] = op_c4; opcodeTable[0xc5] = op_c5; opcodeTable[0xc6] = op_c6; opcodeTable[0xc7] = op_c7;
    opcodeTable[0xc8] = op_c8; opcodeTable[0xc9] = op_c9; opcodeTable[0xca] = op_ca; opcodeTable[0xcb] = op_cb;
    opcodeTable[0xcc] = op_cc; opcodeTable[0xcd] = op_cd; opcodeTable[0xce] = op_ce; opcodeTable[0xcf] = op_cf;
    opcodeTable[0xd0] = op_d0; opcodeTable[0xd1] = op_d1; opcodeTable[0xd2] = op_d2; opcodeTable[0xd3] = op_d3;
    opcodeTable[0xd4] = op_d4; opcodeTable[0xd5] = op_d5; opcodeTable[0xd6] = op_d6; opcodeTable[0xd7] = op_d7;
    opcodeTable[0xd8] = op_d8; opcodeTable[0xd9] = op_d9; opcodeTable[0xda] = op_da; opcodeTable[0xdb] = op_db;
    opcodeTable[0xdc] = op_dc; opcodeTable[0xdd] = op_dd; opcodeTable[0xde] = op_de; opcodeTable[0xdf] = op_df;
    opcodeTable[0xe0] = op_e0; opcodeTable[0xe1] = op_e1; opcodeTable[0xe2] = op_e2; opcodeTable[0xe3] = op_e3;
    opcodeTable[0xe4] = op_e4; opcodeTable[0xe5] = op_e5; opcodeTable[0xe6] = op_e6; opcodeTable[0xe7] = op_e7;
    opcodeTable[0xe8] = op_e8; opcodeTable[0xe9] = op_e9; opcodeTable[0xea] = op_ea; opcodeTable[0xeb] = op_eb;
    opcodeTable[0xec] = op_ec; opcodeTable[0xed] = op_ed; opcodeTable[0xee] = op_ee; opcodeTable[0xef] = op_ef;
    opcodeTable[0xf0] = op_f0; opcodeTable[0xf1] = op_f1; opcodeTable[0xf2] = op_f2; opcodeTable[0xf3] = op_f3;
    opcodeTable[0xf4] = op_f4; opcodeTable[0xf5] = op_f5; opcodeTable[0xf6] = op_f6; opcodeTable[0xf7] = op_f7;
    opcodeTable[0xf8] = op_f8; opcodeTable[0xf9] = op_f9; opcodeTable[0xfa] = op_fa; opcodeTable[0xfb] = op_fb;
    opcodeTable[0xfc] = op_fc; opcodeTable[0xfd] = op_fd; opcodeTable[0xfe] = op_fe; opcodeTable[0xff] = op_ff;
}
