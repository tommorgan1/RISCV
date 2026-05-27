#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>

#include "defs.hpp"
#include "memory.hpp"
class Cpu
{
 public:
	enum class StopReason
	{
		Halt,
		Ecall,
		Ebreak
	};

	explicit Cpu(uint64_t base = 0x80000000, std::size_t memSize = 0x10000000);

	bool load(std::span<const uint8_t> data, uint64_t address);

	uint64_t readReg(uint32_t index) const { return _regs[index]; }
	void     writeReg(uint32_t index, uint64_t value)
	{
		if (index != 0)
		{
			_regs[index] = value;
		}
	}

	std::optional<uint64_t>   peekDoubleWord(uint64_t address) const;
	std::optional<uint64_t>   peekWord(uint64_t address) const;
	std::optional<StopReason> step();

	void setPc(uint64_t pc) { _pc = pc; }

 private:
	std::array<uint64_t, 32>               _regs{};
	std::unordered_map<uint32_t, uint64_t> _csrs;
	uint64_t                               _pc{};
	uint32_t                               _privilege{3};
	Memory                                 _memory;

	uint32_t fetch();
	uint64_t readCSR(uint32_t address) const;
	void     writeCSR(uint32_t address, uint64_t value);
	void     takeTrap(uint64_t cause, uint64_t tvalue, uint64_t trapPc);
	void     checkInterrupts();

	void execute(const InstructionField& field);
	void executeR(const InstructionField& field);
	void executeRW(const InstructionField& field);
	void executeI(const InstructionField& field);
	void executeIW(const InstructionField& field);
	void executeIL(const InstructionField& field);
	void executeS(const InstructionField& field);
	void executeB(const InstructionField& field, uint64_t currentPc);
	void executeU(const InstructionField& field, uint64_t currentPc);
	void executeJ(const InstructionField& field, uint64_t currentPc);
	void executeJALR(const InstructionField& field, uint64_t currentPc);
	void executeSYS(const InstructionField& field);

	// ── Loads (IL-type) ──────────────────────────────────────────────────────
	uint64_t LB(const uint64_t addr);
  uint64_t LH(const uint64_t addr);
  uint64_t LW(const uint64_t addr);
  uint64_t LD(const uint64_t addr);
  uint64_t LBU(const uint64_t addr);
  uint64_t LHU(const uint64_t addr);
  uint64_t LWU(const uint64_t addr);

	// ── Stores (S-type) ──────────────────────────────────────────────────────
	void instrSB(const InstructionField& field);
	void instrSH(const InstructionField& field);
	void instrSW(const InstructionField& field);
	void instrSD(const InstructionField& field);

	// ── Branches (B-type) ────────────────────────────────────────────────────
	void instrBEQ(const InstructionField& field, uint64_t currentPc);
	void instrBNE(const InstructionField& field, uint64_t currentPc);
	void instrBLT(const InstructionField& field, uint64_t currentPc);
	void instrBGE(const InstructionField& field, uint64_t currentPc);
	void instrBLTU(const InstructionField& field, uint64_t currentPc);
	void instrBGEU(const InstructionField& field, uint64_t currentPc);

	// ── U-type ───────────────────────────────────────────────────────────────
	void instrLUI(const InstructionField& field);
	void instrAUIPC(const InstructionField& field, uint64_t currentPc);

	// ── J-type / JALR ────────────────────────────────────────────────────────
	void instrJAL(const InstructionField& field, uint64_t currentPc);
	void instrJALR(const InstructionField& field, uint64_t currentPc);

	// ── System ───────────────────────────────────────────────────────────────
	void instrECALL(const InstructionField& field);
	void instrEBREAK(const InstructionField& field);
	void instrMRET(const InstructionField& field);
	void instrSRET(const InstructionField& field);
	void instrWFI(const InstructionField& field);
	void instrCSRRW(const InstructionField& field);
	void instrCSRRS(const InstructionField& field);
	void instrCSRRC(const InstructionField& field);
	void instrCSRRWI(const InstructionField& field);
	void instrCSRRSI(const InstructionField& field);
	void instrCSRRCI(const InstructionField& field);
};