#pragma once

#include <cstdint>

namespace ALU
{
// R-type
uint64_t ADD(uint64_t rs1, uint64_t rs2);
uint64_t SUB(uint64_t rs1, uint64_t rs2);
uint64_t SLL(uint64_t rs1, uint64_t rs2);
uint64_t SLT(uint64_t rs1, uint64_t rs2);
uint64_t SLTU(uint64_t rs1, uint64_t rs2);
uint64_t XOR(uint64_t rs1, uint64_t rs2);
uint64_t SRL(uint64_t rs1, uint64_t rs2);
uint64_t SRA(uint64_t rs1, uint64_t rs2);
uint64_t OR(uint64_t rs1, uint64_t rs2);
uint64_t AND(uint64_t rs1, uint64_t rs2);

// R-type M-extension
uint64_t MUL(uint64_t rs1, uint64_t rs2);
uint64_t MULH(uint64_t rs1, uint64_t rs2);
uint64_t MULHSU(uint64_t rs1, uint64_t rs2);
uint64_t MULHU(uint64_t rs1, uint64_t rs2);
uint64_t DIV(uint64_t rs1, uint64_t rs2);
uint64_t DIVU(uint64_t rs1, uint64_t rs2);
uint64_t REM(uint64_t rs1, uint64_t rs2);
uint64_t REMU(uint64_t rs1, uint64_t rs2);

// RW-type (32-bit, sign-extended result)
int64_t ADDW(uint64_t rs1, uint64_t rs2);
int64_t SUBW(uint64_t rs1, uint64_t rs2);
int64_t SLLW(uint64_t rs1, uint64_t rs2);
int64_t SRLW(uint64_t rs1, uint64_t rs2);
int64_t SRAW(uint64_t rs1, uint64_t rs2);

// RW-type M-extension
int64_t MULW(uint64_t rs1, uint64_t rs2);
int64_t DIVW(uint64_t rs1, uint64_t rs2);
int64_t DIVUW(uint64_t rs1, uint64_t rs2);
int64_t REMW(uint64_t rs1, uint64_t rs2);
int64_t REMUW(uint64_t rs1, uint64_t rs2);

// I-type
uint64_t ADDI(uint64_t rs1, int64_t imm);
uint64_t SLLI(uint64_t rs1, int64_t imm);
uint64_t SLTI(uint64_t rs1, int64_t imm);
uint64_t SLTIU(uint64_t rs1, int64_t imm);
uint64_t XORI(uint64_t rs1, int64_t imm);
uint64_t SRLI(uint64_t rs1, int64_t imm);
uint64_t SRAI(uint64_t rs1, int64_t imm);
uint64_t ORI(uint64_t rs1, int64_t imm);
uint64_t ANDI(uint64_t rs1, int64_t imm);

// IW-type
int64_t ADDIW(uint64_t rs1, int64_t imm);
int64_t SLLIW(uint64_t rs1, int64_t imm);
int64_t SRLIW(uint64_t rs1, int64_t imm);
int64_t SRAIW(uint64_t rs1, int64_t imm);
}  // namespace ALU