#include "alu.hpp"
#include "utils.hpp"

// Separate ALU for arithmetic 

// ── R-type ────────────────────────────────────────────────────────────────────

uint64_t ALU::ADD(uint64_t rs1, uint64_t rs2)
{
	return rs1 + rs2;
}

uint64_t ALU::SUB(uint64_t rs1, uint64_t rs2)
{
	return rs1 - rs2;
}

uint64_t ALU::SLL(uint64_t rs1, uint64_t rs2)
{
	return rs1 << (rs2 & 0x3F);
}

uint64_t ALU::SLT(uint64_t rs1, uint64_t rs2)
{
	return (static_cast<int64_t>(rs1) < static_cast<int64_t>(rs2)) ? 1 : 0;
}

uint64_t ALU::SLTU(uint64_t rs1, uint64_t rs2)
{
	return (rs1 < rs2) ? 1 : 0;
}

uint64_t ALU::XOR(uint64_t rs1, uint64_t rs2)
{
	return rs1 ^ rs2;
}

uint64_t ALU::SRL(uint64_t rs1, uint64_t rs2)
{
	return rs1 >> (rs2 & 0x3F);
}

uint64_t ALU::SRA(uint64_t rs1, uint64_t rs2)
{
	return static_cast<uint64_t>(static_cast<int64_t>(rs1) >> (rs2 & 0x3F));
}

uint64_t ALU::OR(uint64_t rs1, uint64_t rs2)
{
	return rs1 | rs2;
}

uint64_t ALU::AND(uint64_t rs1, uint64_t rs2)
{
	return rs1 & rs2;
}

// ── RW-type ───────────────────────────────────────────────────────────────────

int64_t ALU::ADDW(uint64_t rs1, uint64_t rs2)
{
	uint32_t result = rs1 + rs2;
	return sign_extend(result);
}

int64_t ALU::SUBW(uint64_t rs1, uint64_t rs2)
{
	uint32_t result = rs1 - rs2;
	return sign_extend(result);
}

int64_t ALU::SLLW(uint64_t rs1, uint64_t rs2)
{
	uint32_t result = rs1 << (rs2 & 0x1F);
	return sign_extend(result);
}

int64_t ALU::SRLW(uint64_t rs1, uint64_t rs2)
{
	uint32_t result = static_cast<uint32_t>(rs1) >> (static_cast<uint32_t>(rs2) & 0x1F);
	return sign_extend(result);
}

int64_t ALU::SRAW(uint64_t rs1, uint64_t rs2)
{
	int32_t result = static_cast<int32_t>(rs1) >> (rs2 & 0x1F);
	return static_cast<int64_t>(result);
}

// ── R-type M-extension ───────────────────────────────────────────────────────

// Lower 64 bits of signed product
uint64_t ALU::MUL(uint64_t rs1, uint64_t rs2)
{
	return rs1 * rs2;
}

// Upper 64 bits of signed x signed 128-bit product
uint64_t ALU::MULH(uint64_t rs1, uint64_t rs2)
{
	__int128 product = static_cast<__int128>(static_cast<int64_t>(rs1)) *
	                   static_cast<__int128>(static_cast<int64_t>(rs2));
	return static_cast<uint64_t>(static_cast<unsigned __int128>(product) >> 64);
}

// Upper 64 bits of signed x unsigned 128-bit product
uint64_t ALU::MULHSU(uint64_t rs1, uint64_t rs2)
{
	// Upper 64 bits of signed x unsigned 128-bit product
	__int128 product = static_cast<__int128>(static_cast<int64_t>(rs1)) * static_cast<__int128>(rs2);
	return static_cast<uint64_t>(static_cast<unsigned __int128>(product) >> 64);
}

// Upper 64 bits of unsigned x unsigned 128-bit product
uint64_t ALU::MULHU(uint64_t rs1, uint64_t rs2)
{
	unsigned __int128 product =
	    static_cast<unsigned __int128>(rs1) * static_cast<unsigned __int128>(rs2);
	return static_cast<uint64_t>(product >> 64);
}

uint64_t ALU::DIV(uint64_t rs1, uint64_t rs2)
{
	if (rs2 == 0)
	{
		return UINT64_MAX;  // divide by zero => -1
	}
	if (rs1 == static_cast<uint64_t>(INT64_MIN) && rs2 == UINT64_MAX)
	{
		return rs1;  // overflow: INT64_MIN / -1
	}
	return static_cast<uint64_t>(static_cast<int64_t>(rs1) / static_cast<int64_t>(rs2));
}

uint64_t ALU::DIVU(uint64_t rs1, uint64_t rs2)
{
	if (rs2 == 0)
	{
		return UINT64_MAX;  // divide by zero => 2^64 - 1
	}
	return rs1 / rs2;
}

uint64_t ALU::REM(uint64_t rs1, uint64_t rs2)
{
	if (rs2 == 0)
	{
		return rs1;  // divide by zero => rs1
	}
	if (rs1 == static_cast<uint64_t>(INT64_MIN) && rs2 == UINT64_MAX)
	{
		return 0;  // overflow: INT64_MIN % -1 => 0
	}
	return static_cast<uint64_t>(static_cast<int64_t>(rs1) % static_cast<int64_t>(rs2));
}

uint64_t ALU::REMU(uint64_t rs1, uint64_t rs2)
{
	if (rs2 == 0)
	{
		return rs1;  // divide by zero => rs1
	}
	return rs1 % rs2;
}

// ── RW-type M-extension ──────────────────────────────────────────────────────

// Lower 32 bits of product, sign-extended to 64
int64_t ALU::MULW(uint64_t rs1, uint64_t rs2)
{
	uint32_t result = static_cast<uint32_t>(rs1) * static_cast<uint32_t>(rs2);
	return sign_extend(result);
}

int64_t ALU::DIVW(uint64_t rs1, uint64_t rs2)
{
	int32_t a = static_cast<int32_t>(rs1);
	int32_t b = static_cast<int32_t>(rs2);
	if (b == 0)
	{
		return -1LL;  // divide by zero => -1
	}
	if (a == INT32_MIN && b == -1)
	{
		return static_cast<int64_t>(a);  // overflow
	}
	return static_cast<int64_t>(a / b);
}

int64_t ALU::DIVUW(uint64_t rs1, uint64_t rs2)
{
	uint32_t a = static_cast<uint32_t>(rs1);
	uint32_t b = static_cast<uint32_t>(rs2);
	if (b == 0)
	{
		return -1LL;  // divide by zero => 2^32-1 sign-extended = -1
	}
	return sign_extend(a / b);
}

int64_t ALU::REMW(uint64_t rs1, uint64_t rs2)
{
	int32_t a = static_cast<int32_t>(rs1);
	int32_t b = static_cast<int32_t>(rs2);
	if (b == 0)
	{
		return static_cast<int64_t>(a);  // divide by zero => rs1 sign-extended
	}
	if (a == INT32_MIN && b == -1)
	{
		return 0LL;  // overflow => 0
	}
	return static_cast<int64_t>(a % b);
}

int64_t ALU::REMUW(uint64_t rs1, uint64_t rs2)
{
	uint32_t a = static_cast<uint32_t>(rs1);
	uint32_t b = static_cast<uint32_t>(rs2);
	if (b == 0)
	{
		return sign_extend(a);  // divide by zero => rs1 sign-extended
	}
	return sign_extend(a % b);
}

// ── I-type ────────────────────────────────────────────────────────────────────

uint64_t ALU::ADDI(uint64_t rs1, int64_t imm)
{
	return rs1 + static_cast<uint64_t>(imm);
}

uint64_t ALU::SLLI(uint64_t rs1, int64_t imm)
{
	return rs1 << (imm & 0x3F);
}

uint64_t ALU::SLTI(uint64_t rs1, int64_t imm)
{
	return (static_cast<int64_t>(rs1) < imm) ? 1 : 0;
}

uint64_t ALU::SLTIU(uint64_t rs1, int64_t imm)
{
	// imm is sign-extended, then compared as unsigned
	return (rs1 < static_cast<uint64_t>(imm)) ? 1 : 0;
}

uint64_t ALU::XORI(uint64_t rs1, int64_t imm)
{
	return rs1 ^ static_cast<uint64_t>(imm);
}

uint64_t ALU::SRLI(uint64_t rs1, int64_t imm)
{
	return rs1 >> (imm & 0x3F);
}

uint64_t ALU::SRAI(uint64_t rs1, int64_t imm)
{
	return static_cast<uint64_t>(static_cast<int64_t>(rs1) >> (imm & 0x3F));
}

uint64_t ALU::ORI(uint64_t rs1, int64_t imm)
{
	return rs1 | static_cast<uint64_t>(imm);
}

uint64_t ALU::ANDI(uint64_t rs1, int64_t imm)
{
	return rs1 & static_cast<uint64_t>(imm);
}

// ── IW-type ───────────────────────────────────────────────────────────────────

int64_t ALU::ADDIW(uint64_t rs1, int64_t imm)
{
	uint32_t result = static_cast<uint32_t>(rs1) + static_cast<uint32_t>(imm);
	return sign_extend(result);
}

int64_t ALU::SLLIW(uint64_t rs1, int64_t imm)
{
	uint32_t result = static_cast<uint32_t>(rs1) << (imm & 0x1F);
	return sign_extend(result);
}

int64_t ALU::SRLIW(uint64_t rs1, int64_t imm)
{
	uint32_t result = static_cast<uint32_t>(rs1) >> (imm & 0x1F);
	return sign_extend(result);
}

int64_t ALU::SRAIW(uint64_t rs1, int64_t imm)
{
	int32_t result = static_cast<int32_t>(rs1) >> (imm & 0x1F);
	return static_cast<int64_t>(result);
}
