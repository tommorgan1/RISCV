#include <catch2/catch_test_macros.hpp>

#include "cpu.hpp"

// Small helper — build a cpu with enough RAM, load a single instruction,
// step once, return the register file state via readReg
static constexpr uint64_t    kBase = 0x80000000ULL;
static constexpr std::size_t kRam  = 1024 * 1024;

// Write a 4-byte instruction at the reset vector and step
static Cpu makeAndStep(uint32_t instr)
{
	Cpu                        cpu(kBase, kRam);
	const std::vector<uint8_t> code = {static_cast<uint8_t>(instr), static_cast<uint8_t>(instr >> 8),
	                                   static_cast<uint8_t>(instr >> 16),
	                                   static_cast<uint8_t>(instr >> 24)};
	cpu.load(code, kBase);
	cpu.step();
	return cpu;
}

static uint32_t makeR(uint32_t f7, uint32_t rs2, uint32_t rs1, uint32_t f3, uint32_t rd,
                      uint32_t op)
{
	return (f7 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}

static uint32_t makeI(uint32_t imm12, uint32_t rs1, uint32_t f3, uint32_t rd, uint32_t op)
{
	return (imm12 << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}

static uint32_t makeU(uint32_t imm20, uint32_t rd, uint32_t op)
{
	return (imm20 << 12) | (rd << 7) | op;
}

// ── RV64I ALU ────────────────────────────────────────────────────────────────
TEST_CASE("ADD x3 = x1 + x2", "[cpu][R]")
{
	Cpu cpu(kBase, kRam);
	// seed x1=10, x2=20 via ADDI then ADD
	const std::vector<uint8_t> code = [&]
	{
		std::vector<uint32_t> instrs = {
		    makeI(10, 0, 0, 1, 0x13),    // ADDI x1, x0, 10
		    makeI(20, 0, 0, 2, 0x13),    // ADDI x2, x0, 20
		    makeR(0, 2, 1, 0, 3, 0x33),  // ADD  x3, x1, x2
		};
		std::vector<uint8_t> b;
		for (auto i : instrs)
		{
			for (int s = 0; s < 4; ++s)
			{
				b.push_back(static_cast<uint8_t>(i >> (8 * s)));
			}
		}
		return b;
	}();
	cpu.load(code, kBase);
	cpu.step();
	cpu.step();
	cpu.step();
	REQUIRE(cpu.readReg(3) == 30u);
}

TEST_CASE("SUB x3 = x1 - x2", "[cpu][R]")
{
	Cpu                   cpu(kBase, kRam);
	std::vector<uint32_t> instrs = {
	    makeI(50, 0, 0, 1, 0x13),       // ADDI x1, x0, 50
	    makeI(20, 0, 0, 2, 0x13),       // ADDI x2, x0, 20
	    makeR(0x20, 2, 1, 0, 3, 0x33),  // SUB  x3, x1, x2
	};
	std::vector<uint8_t> code;
	for (auto i : instrs)
	{
		for (int s = 0; s < 4; ++s)
		{
			code.push_back(static_cast<uint8_t>(i >> (8 * s)));
		}
	}
	cpu.load(code, kBase);
	cpu.step();
	cpu.step();
	cpu.step();
	REQUIRE(cpu.readReg(3) == 30u);
}

TEST_CASE("ADDI sign-extends negative immediate", "[cpu][I]")
{
	const auto cpu = makeAndStep(makeI(0xFFF, 0, 0, 1, 0x13));  // ADDI x1, x0, -1
	REQUIRE(cpu.readReg(1) == ~uint64_t{0});
}

TEST_CASE("x0 is always zero", "[cpu][I]")
{
	const auto cpu = makeAndStep(makeI(42, 0, 0, 0, 0x13));  // ADDI x0, x0, 42
	REQUIRE(cpu.readReg(0) == 0u);
}

TEST_CASE("LUI loads upper immediate", "[cpu][U]")
{
	const auto cpu = makeAndStep(makeU(1, 1, 0x37));  // LUI x1, 1
	REQUIRE(cpu.readReg(1) == 0x1000u);
}

TEST_CASE("LUI sign-extends bit 31", "[cpu][U]")
{
	const auto cpu = makeAndStep(makeU(0x80000, 1, 0x37));
	REQUIRE(cpu.readReg(1) == static_cast<uint64_t>(static_cast<int32_t>(0x80000000u)));
}

TEST_CASE("AUIPC adds upper immediate to PC", "[cpu][U]")
{
	const auto cpu = makeAndStep(makeU(1, 1, 0x17));  // AUIPC x1, 1
	REQUIRE(cpu.readReg(1) == kBase + 0x1000u);
}

TEST_CASE("SLLI shifts left by immediate", "[cpu][I]")
{
	Cpu                   cpu(kBase, kRam);
	std::vector<uint32_t> instrs = {
	    makeI(1, 0, 0, 1, 0x13),                                          // ADDI x1, x0, 1
	    (0 << 25) | (3 << 20) | (1 << 15) | (1 << 12) | (1 << 7) | 0x13,  // SLLI x1, x1, 3
	};
	std::vector<uint8_t> code;
	for (auto i : instrs)
	{
		for (int s = 0; s < 4; ++s)
		{
			code.push_back(static_cast<uint8_t>(i >> (8 * s)));
		}
	}
	cpu.load(code, kBase);
	cpu.step();
	cpu.step();
	REQUIRE(cpu.readReg(1) == 8u);
}

TEST_CASE("SLT sets 1 when signed less than", "[cpu][R]")
{
	Cpu                   cpu(kBase, kRam);
	std::vector<uint32_t> instrs = {
	    makeI(0xFFF, 0, 0, 1, 0x13),  // ADDI x1, x0, -1  (signed -1)
	    makeI(1, 0, 0, 2, 0x13),      // ADDI x2, x0, 1
	    makeR(0, 2, 1, 2, 3, 0x33),   // SLT  x3, x1, x2
	};
	std::vector<uint8_t> code;
	for (auto i : instrs)
	{
		for (int s = 0; s < 4; ++s)
		{
			code.push_back(static_cast<uint8_t>(i >> (8 * s)));
		}
	}
	cpu.load(code, kBase);
	cpu.step();
	cpu.step();
	cpu.step();
	REQUIRE(cpu.readReg(3) == 1u);
}

TEST_CASE("SLTU treats values as unsigned", "[cpu][R]")
{
	Cpu                   cpu(kBase, kRam);
	std::vector<uint32_t> instrs = {
	    makeI(0xFFF, 0, 0, 1, 0x13),  // ADDI x1, x0, -1  (large unsigned)
	    makeI(1, 0, 0, 2, 0x13),      // ADDI x2, x0, 1
	    makeR(0, 2, 1, 3, 3, 0x33),   // SLTU x3, x1, x2  (x1 > x2 unsigned)
	};
	std::vector<uint8_t> code;
	for (auto i : instrs)
	{
		for (int s = 0; s < 4; ++s)
		{
			code.push_back(static_cast<uint8_t>(i >> (8 * s)));
		}
	}
	cpu.load(code, kBase);
	cpu.step();
	cpu.step();
	cpu.step();
	REQUIRE(cpu.readReg(3) == 0u);
}

// ── RV64M ────────────────────────────────────────────────────────────────────
TEST_CASE("MUL basic", "[cpu][M]")
{
	Cpu                   cpu(kBase, kRam);
	std::vector<uint32_t> instrs = {
	    makeI(6, 0, 0, 1, 0x13),     // ADDI x1, x0, 6
	    makeI(7, 0, 0, 2, 0x13),     // ADDI x2, x0, 7
	    makeR(1, 2, 1, 0, 3, 0x33),  // MUL  x3, x1, x2
	};
	std::vector<uint8_t> code;
	for (auto i : instrs)
	{
		for (int s = 0; s < 4; ++s)
		{
			code.push_back(static_cast<uint8_t>(i >> (8 * s)));
		}
	}
	cpu.load(code, kBase);
	cpu.step();
	cpu.step();
	cpu.step();
	REQUIRE(cpu.readReg(3) == 42u);
}

TEST_CASE("DIV signed", "[cpu][M]")
{
	Cpu                   cpu(kBase, kRam);
	std::vector<uint32_t> instrs = {
	    makeI(42, 0, 0, 1, 0x13),    // ADDI x1, x0, 42
	    makeI(6, 0, 0, 2, 0x13),     // ADDI x2, x0, 6
	    makeR(1, 2, 1, 4, 3, 0x33),  // DIV  x3, x1, x2
	};
	std::vector<uint8_t> code;
	for (auto i : instrs)
	{
		for (int s = 0; s < 4; ++s)
		{
			code.push_back(static_cast<uint8_t>(i >> (8 * s)));
		}
	}
	cpu.load(code, kBase);
	cpu.step();
	cpu.step();
	cpu.step();
	REQUIRE(cpu.readReg(3) == 7u);
}

TEST_CASE("DIV by zero returns all-ones", "[cpu][M]")
{
	Cpu                   cpu(kBase, kRam);
	std::vector<uint32_t> instrs = {
	    makeI(1, 0, 0, 1, 0x13),     // ADDI x1, x0, 1
	    makeI(0, 0, 0, 2, 0x13),     // ADDI x2, x0, 0
	    makeR(1, 2, 1, 4, 3, 0x33),  // DIV  x3, x1, x2
	};
	std::vector<uint8_t> code;
	for (auto i : instrs)
	{
		for (int s = 0; s < 4; ++s)
		{
			code.push_back(static_cast<uint8_t>(i >> (8 * s)));
		}
	}
	cpu.load(code, kBase);
	cpu.step();
	cpu.step();
	cpu.step();
	REQUIRE(cpu.readReg(3) == ~uint64_t{0});
}

TEST_CASE("REM signed", "[cpu][M]")
{
	Cpu                   cpu(kBase, kRam);
	std::vector<uint32_t> instrs = {
	    makeI(10, 0, 0, 1, 0x13),    // ADDI x1, x0, 10
	    makeI(3, 0, 0, 2, 0x13),     // ADDI x2, x0, 3
	    makeR(1, 2, 1, 6, 3, 0x33),  // REM  x3, x1, x2
	};
	std::vector<uint8_t> code;
	for (auto i : instrs)
	{
		for (int s = 0; s < 4; ++s)
		{
			code.push_back(static_cast<uint8_t>(i >> (8 * s)));
		}
	}
	cpu.load(code, kBase);
	cpu.step();
	cpu.step();
	cpu.step();
	REQUIRE(cpu.readReg(3) == 1u);
}

TEST_CASE("ADDIW sign-extends 32-bit result to 64", "[cpu][IW]")
{
	Cpu cpu(kBase, kRam);
	// Load 0x7FFFFFFF into x1 via LUI + ADDI, then ADDIW x1, x1, 1
	// 0x7FFFFFFF + 1 = 0x80000000 as 32-bit → sign-extended = 0xFFFFFFFF80000000
	std::vector<uint32_t> instrs = {
	    makeU(0x80000, 1, 0x37),      // LUI  x1, 0x80000  → x1 = 0x80000000
	    makeI(0xFFF, 1, 0, 1, 0x1B),  // ADDIW x1, x1, -1  → x1 = sign_ext(0x7FFFFFFF)
	};
	std::vector<uint8_t> code;
	for (auto i : instrs)
	{
		for (int s = 0; s < 4; ++s)
		{
			code.push_back(static_cast<uint8_t>(i >> (8 * s)));
		}
	}
	cpu.load(code, kBase);
	cpu.step();
	cpu.step();
	REQUIRE(cpu.readReg(1) == 0x000000007FFFFFFFull);
}