#include "decode.hpp"

namespace
{
// Sign extend a value of 'bits' width to int64_t
int64_t signExtend(uint64_t value, uint32_t bits)
{
	const uint64_t sign = uint64_t{1} << (bits - 1);
	return static_cast<int64_t>((value ^ sign) - sign);
}

InstructionField decodeR(uint32_t instr)
{
	return {.type   = InstructionType::R,
	        .raw    = instr,
	        .opcode = instr & 0x7F,
	        .rd     = (instr >> 7) & 0x1F,
	        .rs1    = (instr >> 15) & 0x1F,
	        .rs2    = (instr >> 20) & 0x1F,
	        .funct3 = (instr >> 12) & 0x07,
	        .funct7 = (instr >> 25) & 0x7F,
	        .imm    = std::nullopt};
}

InstructionField decodeRW(uint32_t instr)
{
	return {.type   = InstructionType::RW,
	        .raw    = instr,
	        .opcode = instr & 0x7F,
	        .rd     = (instr >> 7) & 0x1F,
	        .rs1    = (instr >> 15) & 0x1F,
	        .rs2    = (instr >> 20) & 0x1F,
	        .funct3 = (instr >> 12) & 0x07,
	        .funct7 = (instr >> 25) & 0x7F,
	        .imm    = std::nullopt};
}

InstructionField decodeI(uint32_t instr)
{
	return {.type   = InstructionType::I,
	        .raw    = instr,
	        .opcode = instr & 0x7F,
	        .rd     = (instr >> 7) & 0x1F,
	        .rs1    = (instr >> 15) & 0x1F,
	        .rs2    = std::nullopt,
	        .funct3 = (instr >> 12) & 0x07,
	        .funct7 = (instr >> 25) & 0x7F,
	        .imm    = signExtend((instr >> 20) & 0xFFF, 12)};
}

InstructionField decodeIW(uint32_t instr)
{
	return {.type   = InstructionType::IW,
	        .raw    = instr,
	        .opcode = instr & 0x7F,
	        .rd     = (instr >> 7) & 0x1F,
	        .rs1    = (instr >> 15) & 0x1F,
	        .rs2    = std::nullopt,
	        .funct3 = (instr >> 12) & 0x07,
	        .funct7 = (instr >> 25) & 0x7F,
	        .imm    = signExtend((instr >> 20) & 0xFFF, 12)};
}

InstructionField decodeIL(uint32_t instr)
{
	return {.type   = InstructionType::IL,
	        .raw    = instr,
	        .opcode = instr & 0x7F,
	        .rd     = (instr >> 7) & 0x1F,
	        .rs1    = (instr >> 15) & 0x1F,
	        .rs2    = std::nullopt,
	        .funct3 = (instr >> 12) & 0x07,
	        .funct7 = std::nullopt,
	        .imm    = signExtend((instr >> 20) & 0xFFF, 12)};
}

InstructionField decodeJALR(uint32_t instr)
{
	return {.type   = InstructionType::JALR,
	        .raw    = instr,
	        .opcode = instr & 0x7F,
	        .rd     = (instr >> 7) & 0x1F,
	        .rs1    = (instr >> 15) & 0x1F,
	        .rs2    = std::nullopt,
	        .funct3 = (instr >> 12) & 0x07,
	        .funct7 = std::nullopt,
	        .imm    = signExtend((instr >> 20) & 0xFFF, 12)};
}

InstructionField decodeS(uint32_t instr)
{
	const uint64_t imm = ((instr >> 25) & 0x7F) << 5 | ((instr >> 7) & 0x1F);
	return {.type   = InstructionType::S,
	        .raw    = instr,
	        .opcode = instr & 0x7F,
	        .rd     = std::nullopt,
	        .rs1    = (instr >> 15) & 0x1F,
	        .rs2    = (instr >> 20) & 0x1F,
	        .funct3 = (instr >> 12) & 0x07,
	        .funct7 = std::nullopt,
	        .imm    = signExtend(imm, 12)};
}

InstructionField decodeB(uint32_t instr)
{
	const uint64_t imm = ((instr >> 31) & 0x1) << 12 | ((instr >> 7) & 0x1) << 11 |
	                     ((instr >> 25) & 0x3F) << 5 | ((instr >> 8) & 0x0F) << 1;
	return {.type   = InstructionType::B,
	        .raw    = instr,
	        .opcode = instr & 0x7F,
	        .rd     = std::nullopt,
	        .rs1    = (instr >> 15) & 0x1F,
	        .rs2    = (instr >> 20) & 0x1F,
	        .funct3 = (instr >> 12) & 0x07,
	        .funct7 = std::nullopt,
	        .imm    = signExtend(imm, 13)};
}

InstructionField decodeU(uint32_t instr)
{
	return {.type   = InstructionType::U,
	        .raw    = instr,
	        .opcode = instr & 0x7F,
	        .rd     = (instr >> 7) & 0x1F,
	        .rs1    = std::nullopt,
	        .rs2    = std::nullopt,
	        .funct3 = std::nullopt,
	        .funct7 = std::nullopt,
	        .imm    = static_cast<int64_t>(static_cast<int32_t>(instr & 0xFFFFF000))};
}

InstructionField decodeJ(uint32_t instr)
{
	const uint64_t imm = ((instr >> 31) & 0x001) << 20 | ((instr >> 12) & 0x0FF) << 12 |
	                     ((instr >> 20) & 0x001) << 11 | ((instr >> 21) & 0x3FF) << 1;
	return {.type   = InstructionType::J,
	        .raw    = instr,
	        .opcode = instr & 0x7F,
	        .rd     = (instr >> 7) & 0x1F,
	        .rs1    = std::nullopt,
	        .rs2    = std::nullopt,
	        .funct3 = std::nullopt,
	        .funct7 = std::nullopt,
	        .imm    = signExtend(imm, 21)};
}

InstructionField decodeSYS(uint32_t instr)
{
	return {.type   = InstructionType::SYS,
	        .raw    = instr,
	        .opcode = instr & 0x7F,
	        .rd     = (instr >> 7) & 0x1F,
	        .rs1    = (instr >> 15) & 0x1F,
	        .rs2    = std::nullopt,
	        .funct3 = (instr >> 12) & 0x07,
	        .funct7 = std::nullopt,
	        .imm    = static_cast<int64_t>((instr >> 20) & 0xFFF)};
}
}  // namespace

namespace decode
{
InstructionField decode(uint32_t instr)
{
	const uint32_t opcode = instr & 0x7F;
	switch (opcode)
	{
		case 0x33:
			return decodeR(instr);
		case 0x3B:
			return decodeRW(instr);
		case 0x13:
			return decodeI(instr);
		case 0x1B:
			return decodeIW(instr);
		case 0x03:
			return decodeIL(instr);
		case 0x67:
			return decodeJALR(instr);
		case 0x23:
			return decodeS(instr);
		case 0x63:
			return decodeB(instr);
		case 0x37:
		case 0x17:
			return decodeU(instr);
		case 0x6F:
			return decodeJ(instr);
		case 0x73:
			return decodeSYS(instr);
		case 0x0F:
			return {.type = InstructionType::FENCE, .raw = instr, .opcode = opcode};
		default:
			return {.type = InstructionType::ILLEGAL, .raw = instr, .opcode = opcode};
	}
}
}  // namespace decode