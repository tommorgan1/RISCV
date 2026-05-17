#include <catch2/catch_test_macros.hpp>

#include "decode.hpp"

// ── Helpers ───────────────────────────────────────────────────────────────────
// Build a minimal instruction word from fields
static uint32_t makeR(uint32_t funct7, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t rd,
                      uint32_t opcode)
{
	return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

static uint32_t makeI(uint32_t imm12, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t opcode)
{
	return (imm12 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

static uint32_t makeS(uint32_t imm12, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t opcode)
{
	const uint32_t imm_11_5 = (imm12 >> 5) & 0x7F;
	const uint32_t imm_4_0  = imm12 & 0x1F;
	return (imm_11_5 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (imm_4_0 << 7) | opcode;
}

static uint32_t makeU(uint32_t imm20, uint32_t rd, uint32_t opcode)
{
	return (imm20 << 12) | (rd << 7) | opcode;
}

// ── R-type ────────────────────────────────────────────────────────────────────
TEST_CASE("Decode R-type ADD", "[decode][R]")
{
	// ADD x1, x2, x3  →  funct7=0, rs2=3, rs1=2, funct3=0, rd=1, opcode=0x33
	const auto f = decode::decode(makeR(0, 3, 2, 0, 1, 0x33));
	REQUIRE(f.type == InstructionType::R);
	REQUIRE(f.rd == 1u);
	REQUIRE(f.rs1 == 2u);
	REQUIRE(f.rs2 == 3u);
	REQUIRE(f.funct3 == 0u);
	REQUIRE(f.funct7 == 0u);
	REQUIRE_FALSE(f.imm.has_value());
}

TEST_CASE("Decode R-type SUB distinguishes funct7", "[decode][R]")
{
	const auto f = decode::decode(makeR(0x20, 3, 2, 0, 1, 0x33));
	REQUIRE(f.type == InstructionType::R);
	REQUIRE(f.funct7 == 0x20u);
}

TEST_CASE("Decode RW-type ADDW", "[decode][RW]")
{
	const auto f = decode::decode(makeR(0, 3, 2, 0, 1, 0x3B));
	REQUIRE(f.type == InstructionType::RW);
	REQUIRE(f.rd == 1u);
	REQUIRE(f.rs1 == 2u);
	REQUIRE(f.rs2 == 3u);
}

// ── I-type ────────────────────────────────────────────────────────────────────
TEST_CASE("Decode I-type ADDI positive immediate", "[decode][I]")
{
	// ADDI x1, x2, 42
	const auto f = decode::decode(makeI(42, 2, 0, 1, 0x13));
	REQUIRE(f.type == InstructionType::I);
	REQUIRE(f.rd == 1u);
	REQUIRE(f.rs1 == 2u);
	REQUIRE(f.funct3 == 0u);
	REQUIRE(f.imm == 42);
}

TEST_CASE("Decode I-type ADDI negative immediate sign-extends", "[decode][I]")
{
	// imm = -1 = 0xFFF in 12-bit two's complement
	const auto f = decode::decode(makeI(0xFFF, 2, 0, 1, 0x13));
	REQUIRE(f.imm == -1);
}

TEST_CASE("Decode I-type ADDI minimum negative immediate", "[decode][I]")
{
	// imm = -2048 = 0x800 in 12-bit
	const auto f = decode::decode(makeI(0x800, 0, 0, 0, 0x13));
	REQUIRE(f.imm == -2048);
}

TEST_CASE("Decode IW-type ADDIW", "[decode][IW]")
{
	const auto f = decode::decode(makeI(10, 1, 0, 2, 0x1B));
	REQUIRE(f.type == InstructionType::IW);
	REQUIRE(f.imm == 10);
}

TEST_CASE("Decode IL-type load", "[decode][IL]")
{
	// LW x1, 4(x2)
	const auto f = decode::decode(makeI(4, 2, 2, 1, 0x03));
	REQUIRE(f.type == InstructionType::IL);
	REQUIRE(f.funct3 == 2u);
	REQUIRE(f.imm == 4);
	REQUIRE_FALSE(f.funct7.has_value());
}

// ── S-type ────────────────────────────────────────────────────────────────────
TEST_CASE("Decode S-type SW immediate reconstruction", "[decode][S]")
{
	// SW x3, 100(x2)  —  imm=100=0x64, split: imm[11:5]=0x03, imm[4:0]=0x04
	const auto f = decode::decode(makeS(100, 3, 2, 2, 0x23));
	REQUIRE(f.type == InstructionType::S);
	REQUIRE(f.rs1 == 2u);
	REQUIRE(f.rs2 == 3u);
	REQUIRE(f.imm == 100);
}

TEST_CASE("Decode S-type negative immediate", "[decode][S]")
{
	const auto f = decode::decode(makeS(0xFFF, 1, 2, 0, 0x23));
	REQUIRE(f.imm == -1);
}

// ── U-type ────────────────────────────────────────────────────────────────────
TEST_CASE("Decode U-type LUI", "[decode][U]")
{
	// LUI x1, 0x12345  →  upper 20 bits = 0x12345, lower 12 = 0
	const auto f = decode::decode(makeU(0x12345, 1, 0x37));
	REQUIRE(f.type == InstructionType::U);
	REQUIRE(f.rd == 1u);
	REQUIRE(f.imm == static_cast<int64_t>(0x12345000));
}

TEST_CASE("Decode U-type AUIPC", "[decode][U]")
{
	const auto f = decode::decode(makeU(1, 1, 0x17));
	REQUIRE(f.type == InstructionType::U);
	REQUIRE(f.imm == 0x1000);
}

TEST_CASE("Decode U-type LUI sign-extends through bit 31", "[decode][U]")
{
	// imm20 = 0x80000 → value = 0x80000000 → sign-extended = 0xFFFFFFFF80000000
	const auto f = decode::decode(makeU(0x80000, 1, 0x37));
	REQUIRE(f.imm == static_cast<int64_t>(static_cast<int32_t>(0x80000000u)));
}

// ── B-type ────────────────────────────────────────────────────────────────────
TEST_CASE("Decode B-type BEQ positive offset", "[decode][B]")
{
	// BEQ x1, x2, +8  (offset 8 = 0b0000_0000_1000)
	// B-immediate encoding: imm[12|10:5] | rs2 | rs1 | funct3 | imm[4:1|11]
	// offset=8: bit12=0, bit11=0, bits10:5=0, bits4:1=4(=0b0100), bit0=0(always)
	// imm_11_5 portion: bit12=0, bits10:5=0 → upper = 0
	// imm_4_0 portion: bit11=0, bits4:1=4 → lower = 0b00100 = 4
	const uint32_t imm      = 8;
	const uint32_t bit12    = (imm >> 12) & 1;
	const uint32_t bit11    = (imm >> 11) & 1;
	const uint32_t bits10_5 = (imm >> 5) & 0x3F;
	const uint32_t bits4_1  = (imm >> 1) & 0xF;
	const uint32_t instr    = (bit12 << 31) | (bits10_5 << 25) | (2 << 20) | (1 << 15) | (0 << 12) |
	                       (bits4_1 << 8) | (bit11 << 7) | 0x63;
	const auto f = decode::decode(instr);
	REQUIRE(f.type == InstructionType::B);
	REQUIRE(f.rs1 == 1u);
	REQUIRE(f.rs2 == 2u);
	REQUIRE(f.imm == 8);
}

// ── J-type ────────────────────────────────────────────────────────────────────
TEST_CASE("Decode J-type JAL positive offset", "[decode][J]")
{
	// JAL x1, +4  (offset 4)
	const uint32_t imm       = 4;
	const uint32_t bit20     = (imm >> 20) & 1;
	const uint32_t bits19_12 = (imm >> 12) & 0xFF;
	const uint32_t bit11     = (imm >> 11) & 1;
	const uint32_t bits10_1  = (imm >> 1) & 0x3FF;
	const uint32_t instr =
	    (bit20 << 31) | (bits10_1 << 21) | (bit11 << 20) | (bits19_12 << 12) | (1 << 7) | 0x6F;
	const auto f = decode::decode(instr);
	REQUIRE(f.type == InstructionType::J);
	REQUIRE(f.rd == 1u);
	REQUIRE(f.imm == 4);
}

// ── Special ───────────────────────────────────────────────────────────────────
TEST_CASE("Decode FENCE", "[decode][special]")
{
	const auto f = decode::decode(0x0000000F);
	REQUIRE(f.type == InstructionType::FENCE);
}

TEST_CASE("Decode ILLEGAL opcode", "[decode][special]")
{
	const auto f = decode::decode(0x00000000);
	REQUIRE(f.type == InstructionType::ILLEGAL);
}

TEST_CASE("Decode JALR", "[decode][JALR]")
{
	// JALR x1, 0(x2)
	const auto f = decode::decode(makeI(0, 2, 0, 1, 0x67));
	REQUIRE(f.type == InstructionType::JALR);
	REQUIRE(f.rd == 1u);
	REQUIRE(f.rs1 == 2u);
	REQUIRE(f.imm == 0);
}