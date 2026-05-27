#include "cpu.hpp"

#include <format>
#include <stdexcept>

#include "alu.hpp"
#include "decode.hpp"
#include "utils.hpp"

static std::string hex64(uint64_t v)
{
	return std::format("0x{:016x}", v);
}

Cpu::Cpu(uint64_t base, std::size_t memSize)
  : _pc(base)
  , _memory(base, memSize)
{
	_regs.fill(0);

	// Initialise read-only CSRs directly — bypass writeCSR guard
	_csrs[CSR::MISA]      = (uint64_t{2} << 62) | (1 << 8);
	_csrs[CSR::MHARTID]   = 0;
	_csrs[CSR::MVENDORID] = 0;
	_csrs[CSR::MARCHID]   = 0;
	_csrs[CSR::MIMPID]    = 0;

	writeCSR(CSR::MTVEC, 0);
}

bool Cpu::load(std::span<const uint8_t> data, uint64_t address)
{
	return _memory.load(data, address);
}

std::optional<uint64_t> Cpu::peekDoubleWord(uint64_t address) const
{
	uint64_t value{};
	if (_memory.read64(address, value) != memFault::none)
	{
		return std::nullopt;
	}
	return value;
}

std::optional<uint64_t> Cpu::peekWord(uint64_t address) const
{
	uint32_t value{};
	if (_memory.read32(address, value) != memFault::none)
	{
		return std::nullopt;
	}
	return static_cast<uint64_t>(value);
}

uint64_t Cpu::readCSR(uint32_t address) const
{
	const auto it = _csrs.find(address);
	return (it == _csrs.end()) ? 0 : it->second;
}

void Cpu::writeCSR(uint32_t address, uint64_t value)
{
	// Read-only CSRs — writes silently ignored
	if (address == CSR::MHARTID || address == CSR::MVENDORID || address == CSR::MARCHID ||
	    address == CSR::MIMPID || address == CSR::MISA)
	{
		return;
	}

	if (address == CSR::MSTATUS)
	{
		// Mask to writable bits only — SD(63), MBE/SBE/UBE(37:36),
		// SXL/UXL(35:32) read-only, MPIE(7), MIE(3), MPP(12:11), SPP(8)
		static constexpr uint64_t writableMask =
		    (uint64_t{1} << 63) |  // SD (read-only, reflects FS/XS)
		    (0x3ULL << 13) |       // FS
		    (0x3ULL << 11) |       // MPP
		    (1ULL << 7) |          // MPIE
		    (1ULL << 3);           // MIE

		_csrs[address] = (value & writableMask);

		return;
	}

	_csrs[address] = value;
}

uint32_t Cpu::fetch()
{
	uint32_t   instr{};
	const auto fault = _memory.read32(_pc, instr);
	if (fault == memFault::outOfBounds)
	{
		throw std::runtime_error("fetch out of bounds at " + hex64(_pc));
	}
	_pc += 4;
	return instr;
}

std::optional<Cpu::StopReason> Cpu::step()
{
	if (_pc & 0x3)
	{
		takeTrap(0, _pc, _pc);
		return std::nullopt;
	}
	const auto instr = fetch();
	const auto field = decode::decode(instr);
	execute(field);
	return std::nullopt;
}
void Cpu::execute(const InstructionField& field)
{
	const uint64_t currentPc = _pc - 4;
	switch (field.type)
	{
		case InstructionType::R:
			executeR(field);
			break;
		case InstructionType::RW:
			executeRW(field);
			break;
		case InstructionType::I:
			executeI(field);
			break;
		case InstructionType::IW:
			executeIW(field);
			break;
		case InstructionType::IL:
			executeIL(field);
			break;
		case InstructionType::S:
			executeS(field);
			break;
		case InstructionType::B:
			executeB(field, currentPc);
			break;
		case InstructionType::U:
			executeU(field, currentPc);
			break;
		case InstructionType::J:
			executeJ(field, currentPc);
			break;
		case InstructionType::JALR:
			executeJALR(field, currentPc);
			break;
		case InstructionType::SYS:
			executeSYS(field);
			break;
		case InstructionType::FENCE:
			break;
		case InstructionType::ILLEGAL:
			takeTrap(2, static_cast<uint64_t>(field.raw), currentPc);
			break;
	}
	_regs[0] = 0;  // x0 hardwired zero
}

// ── R ─────────────────────────────────────────────────────────────────────────
void Cpu::executeR(const InstructionField& field)
{
	const uint64_t rs1    = readReg(field.rs1.value());
	const uint64_t rs2    = readReg(field.rs2.value());
	uint64_t       result = 0;

	switch ((field.funct3.value() << 8) | field.funct7.value())
	{
		case (0x0 << 8) | 0x00:
			result = ALU::ADD(rs1, rs2);
			break;  // ADD
		case (0x0 << 8) | 0x20:
			result = ALU::SUB(rs1, rs2);
			break;  // SUB
		case (0x1 << 8) | 0x00:
			result = ALU::SLL(rs1, rs2);
			break;  // SLL
		case (0x2 << 8) | 0x00:
			result = ALU::SLT(rs1, rs2);
			break;  // SLT
		case (0x3 << 8) | 0x00:
			result = ALU::SLTU(rs1, rs2);
			break;  // SLTU
		case (0x4 << 8) | 0x00:
			result = ALU::XOR(rs1, rs2);
			break;  // XOR
		case (0x5 << 8) | 0x00:
			result = ALU::SRL(rs1, rs2);
			break;  // SRL
		case (0x5 << 8) | 0x20:
			result = ALU::SRA(rs1, rs2);
			break;  // SRA
		case (0x6 << 8) | 0x00:
			result = ALU::OR(rs1, rs2);
			break;  // OR
		case (0x7 << 8) | 0x00:
			result = ALU::AND(rs1, rs2);
			break;  // AND

			// RV64M — funct7 = 0x01
		case (0x0 << 8) | 0x01:
			result = ALU::MUL(rs1, rs2);
			break;  // MUL
		case (0x1 << 8) | 0x01:
			result = ALU::MULH(rs1, rs2);
			break;  // MULH
		case (0x2 << 8) | 0x01:
			result = ALU::MULHSU(rs1, rs2);
			break;  // MULHSU
		case (0x3 << 8) | 0x01:
			result = ALU::MULHU(rs1, rs2);
			break;  // MULHU
		case (0x4 << 8) | 0x01:
			result = ALU::DIV(rs1, rs2);
			break;  // DIV
		case (0x5 << 8) | 0x01:
			result = ALU::DIVU(rs1, rs2);
			break;  // DIVU
		case (0x6 << 8) | 0x01:
			result = ALU::REM(rs1, rs2);
			break;  // REM
		case (0x7 << 8) | 0x01:
			result = ALU::REMU(rs1, rs2);
			break;  // REMU
		default:
			takeTrap(2, static_cast<uint64_t>(field.raw), _pc - 4);
			return;
	}
	writeReg(field.rd.value(), result);
}

// ── RW ────────────────────────────────────────────────────────────────────────
void Cpu::executeRW(const InstructionField& field)
{
	const uint64_t rs1    = readReg(field.rs1.value());
	const uint64_t rs2    = readReg(field.rs2.value());
	int64_t        result = 0;

	switch ((field.funct3.value() << 8) | field.funct7.value())
	{
		case (0x0 << 8) | 0x00:
			result = ALU::ADDW(rs1, rs2);
			break;  // ADDW
		case (0x0 << 8) | 0x20:
			result = ALU::SUBW(rs1, rs2);
			break;  // SUBW
		case (0x1 << 8) | 0x00:
			result = ALU::SLLW(rs1, rs2);
			break;  // SLLW
		case (0x5 << 8) | 0x00:
			result = ALU::SRLW(rs1, rs2);
			break;  // SRLW
		case (0x5 << 8) | 0x20:
			result = ALU::SRAW(rs1, rs2);
			break;  // SRAW
		case (0x0 << 8) | 0x01:
			result = ALU::MULW(rs1, rs2);
			break;  // MULW
		case (0x4 << 8) | 0x01:
			result = ALU::DIVW(rs1, rs2);
			break;  // DIVW
		case (0x5 << 8) | 0x01:
			result = ALU::DIVUW(rs1, rs2);
			break;  // DIVUW
		case (0x6 << 8) | 0x01:
			result = ALU::REMW(rs1, rs2);
			break;  // REMW
		case (0x7 << 8) | 0x01:
			result = ALU::REMUW(rs1, rs2);
			break;  // REMUW
		default:
			takeTrap(2, static_cast<uint64_t>(field.raw), _pc - 4);
			return;
	}
	writeReg(field.rd.value(), static_cast<uint64_t>(result));
}

// ── I ─────────────────────────────────────────────────────────────────────────
void Cpu::executeI(const InstructionField& field)
{
	const uint64_t rs1    = readReg(field.rs1.value());
	const int64_t  imm    = field.imm.value();
	uint64_t       result = 0;

	switch (field.funct3.value())
	{
		case 0x0:
			result = ALU::ADDI(rs1, imm);
			break;  // ADDI
		case 0x1:
			result = ALU::SLLI(rs1, imm);
			break;  // SLLI
		case 0x2:
			result = ALU::SLTI(rs1, imm);
			break;  // SLTI
		case 0x3:
			result = ALU::SLTIU(rs1, imm);
			break;  // SLTIU
		case 0x4:
			result = ALU::XORI(rs1, imm);
			break;  // XORI
		case 0x5:
			switch (field.funct7.value() & ~1u)
			{
				case 0x00:
					result = ALU::SRLI(rs1, imm);
					break;  // SRLI
				case 0x20:
					result = ALU::SRAI(rs1, imm);
					break;  // SRAI
				default:
					throw std::runtime_error("unknown funct7 I-shift at " + hex64(_pc - 4));
			}
			break;
		case 0x6:
			result = ALU::ORI(rs1, imm);
			break;  // ORI
		case 0x7:
			result = ALU::ANDI(rs1, imm);
			break;  // ANDI
		default:
			throw std::runtime_error("unknown funct3 I-type at " + hex64(_pc - 4));
	}
	writeReg(field.rd.value(), result);
}

// ── IW ────────────────────────────────────────────────────────────────────────
void Cpu::executeIW(const InstructionField& field)
{
	const uint64_t rs1    = readReg(field.rs1.value());
	const int64_t  imm    = field.imm.value();
	int64_t        result = 0;

	switch (field.funct3.value())
	{
		case 0x0:
			result = ALU::ADDIW(rs1, imm);
			break;  // ADDIW
		case 0x1:
			result = ALU::SLLIW(rs1, imm);
			break;  // SLLIW
		case 0x5:
			switch (field.funct7.value() & ~1u)
			{
				case 0x00:
					result = ALU::SRLIW(rs1, imm);
					break;  // SRLIW
				case 0x20:
					result = ALU::SRAIW(rs1, imm);
					break;  // SRAIW
				default:
					throw std::runtime_error("unknown funct7 IW-shift at " + hex64(_pc - 4));
			}
			break;
		default:
			throw std::runtime_error("unknown funct3 IW-type at " + hex64(_pc - 4));
	}
	writeReg(field.rd.value(), static_cast<uint64_t>(result));
}

// ── IL (loads) ────────────────────────────────────────────────────────────────
void Cpu::executeIL(const InstructionField& field)
{
	const uint64_t addr =
	    static_cast<uint64_t>(static_cast<int64_t>(readReg(field.rs1.value())) + field.imm.value());

	uint64_t result = 0;

	switch (field.funct3.value())
	{
		case 0x0:
		{
			uint8_t v{};
			_memory.read8(addr, v);
			result = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(v)));
			break;
		}  // LB
		case 0x1:
		{
			uint16_t v{};
			_memory.read16(addr, v);
			result = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(v)));
			break;
		}  // LH
		case 0x2:
		{
			uint32_t v{};
			_memory.read32(addr, v);
			result = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v)));
			break;
		}  // LW
		case 0x3:
		{
			_memory.read64(addr, result);
			break;
		}  // LD
		case 0x4:
		{
			uint8_t v{};
			_memory.read8(addr, v);
			result = static_cast<uint64_t>(v);
			break;
		}  // LBU
		case 0x5:
		{
			uint16_t v{};
			_memory.read16(addr, v);
			result = static_cast<uint64_t>(v);
			break;
		}  // LHU
		case 0x6:
		{
			uint32_t v{};
			_memory.read32(addr, v);
			result = static_cast<uint64_t>(v);
			break;
		}  // LWU
		default:
			throw std::runtime_error("unknown funct3 load at " + hex64(_pc - 4));
	}
	writeReg(field.rd.value(), result);
}

// ── S (stores) ────────────────────────────────────────────────────────────────
void Cpu::executeS(const InstructionField& field)
{
	const uint64_t addr =
	    static_cast<uint64_t>(static_cast<int64_t>(readReg(field.rs1.value())) + field.imm.value());
	const uint64_t src = readReg(field.rs2.value());

	switch (field.funct3.value())
	{
		case 0x0:
			_memory.write8(addr, static_cast<uint8_t>(src));
			break;  // SB
		case 0x1:
			_memory.write16(addr, static_cast<uint16_t>(src));
			break;  // SH
		case 0x2:
			_memory.write32(addr, static_cast<uint32_t>(src));
			break;  // SW
		case 0x3:
			_memory.write64(addr, src);
			break;  // SD
		default:
			throw std::runtime_error("unknown funct3 store at " + hex64(_pc - 4));
	}
}

// ── B (branches) ─────────────────────────────────────────────────────────────
void Cpu::executeB(const InstructionField& field, uint64_t currentPc)
{
	const uint64_t rs1   = readReg(field.rs1.value());
	const uint64_t rs2   = readReg(field.rs2.value());
	bool           taken = false;

	switch (field.funct3.value())
	{
		case 0x0:
			taken = (rs1 == rs2);
			break;  // BEQ
		case 0x1:
			taken = (rs1 != rs2);
			break;  // BNE
		case 0x4:
			taken = (static_cast<int64_t>(rs1) < static_cast<int64_t>(rs2));
			break;  // BLT
		case 0x5:
			taken = (static_cast<int64_t>(rs1) >= static_cast<int64_t>(rs2));
			break;  // BGE
		case 0x6:
			taken = (rs1 < rs2);
			break;  // BLTU
		case 0x7:
			taken = (rs1 >= rs2);
			break;  // BGEU
		default:
			throw std::runtime_error("unknown funct3 branch at " + hex64(_pc - 4));
	}

	if (taken)
	{
		_pc = static_cast<uint64_t>(static_cast<int64_t>(currentPc) + field.imm.value());
	}
}

// ── U (LUI / AUIPC) ───────────────────────────────────────────────────────────
void Cpu::executeU(const InstructionField& field, uint64_t currentPc)
{
	switch (field.opcode)
	{
		case 0x37:
			writeReg(field.rd.value(), static_cast<uint64_t>(field.imm.value()));
			break;  // LUI
		case 0x17:
			writeReg(field.rd.value(), static_cast<uint64_t>(  // AUIPC
			                               static_cast<int64_t>(currentPc) + field.imm.value()));
			break;
		default:
			throw std::runtime_error("unknown U-type opcode at " + hex64(_pc - 4));
	}
}

// ── J (JAL) ───────────────────────────────────────────────────────────────────
void Cpu::executeJ(const InstructionField& field, uint64_t currentPc)
{
	writeReg(field.rd.value(), currentPc + 4);
	_pc = static_cast<uint64_t>(static_cast<int64_t>(currentPc) + field.imm.value());
}

// ── JALR ─────────────────────────────────────────────────────────────────────
void Cpu::executeJALR(const InstructionField& field, uint64_t currentPc)
{
	const uint64_t target =
	    static_cast<uint64_t>(static_cast<int64_t>(readReg(field.rs1.value())) + field.imm.value()) &
	    ~uint64_t{1};
	writeReg(field.rd.value(), currentPc + 4);
	_pc = target;
}

// ── SYS ───────────────────────────────────────────────────────────────────────
void Cpu::executeSYS(const InstructionField& field)
{
	const uint32_t csr  = static_cast<uint32_t>(field.imm.value());
	const uint64_t rs1v = readReg(field.rs1.value());
	const uint64_t zimm = static_cast<uint64_t>(field.rs1.value());

	switch (field.funct3.value())
	{
		case 0x0:
			if (csr == 0x000)
			{
				const uint32_t cause = (_privilege == 3) ? 11 : (_privilege == 1) ? 9 : 8;
				takeTrap(cause, 0, _pc - 4);
			}
			else if (csr == 0x001)
			{
				takeTrap(3, _pc - 4, _pc - 4);  // EBREAK
			}
			else if (csr == 0x302)  // MRET
			{
				uint64_t       mstatus = readCSR(CSR::MSTATUS);
				const uint64_t mpie    = (mstatus >> 7) & 0x1;
				const uint32_t mpp     = static_cast<uint32_t>((mstatus >> 11) & 0x3);
				mstatus &= ~(uint64_t{1} << 3);
				mstatus |= (mpie << 3);
				mstatus |= (uint64_t{1} << 7);
				mstatus &= ~(uint64_t{3} << 11);
				writeCSR(CSR::MSTATUS, mstatus);
				_privilege = mpp;
				_pc        = readCSR(CSR::MEPC);
			}
			else if (csr == 0x102)  // SRET
			{
				uint64_t       mstatus = readCSR(CSR::MSTATUS);
				const uint64_t spie    = (mstatus >> 5) & 0x1;
				const uint32_t spp     = static_cast<uint32_t>((mstatus >> 8) & 0x1);
				mstatus &= ~(uint64_t{1} << 1);
				mstatus |= (spie << 1);          // SIE = SPIE
				mstatus |= (uint64_t{1} << 5);   // SPIE = 1
				mstatus &= ~(uint64_t{1} << 8);  // SPP = 0
				writeCSR(CSR::MSTATUS, mstatus);
				_privilege = spp;
				_pc        = readCSR(CSR::SEPC);
			}
			else if (csr == 0x105)
			{ /* WFI — NOP */
			}
			else if ((field.raw >> 25) == 0x09)
			{ /* SFENCE.VMA — NOP for now */
			}
			else
			{
				takeTrap(2, _pc - 4, _pc - 4);
			}
			break;

		case 0x1:
		{
			uint64_t old = readCSR(csr);
			writeCSR(csr, rs1v);
			writeReg(field.rd.value(), old);
			break;
		}  // CSRRW
		case 0x2:
		{
			uint64_t old = readCSR(csr);
			if (rs1v)
			{
				writeCSR(csr, old | rs1v);
			}
			writeReg(field.rd.value(), old);
			break;
		}  // CSRRS
		case 0x3:
		{
			uint64_t old = readCSR(csr);
			if (rs1v)
			{
				writeCSR(csr, old & ~rs1v);
			}
			writeReg(field.rd.value(), old);
			break;
		}  // CSRRC
		case 0x5:
		{
			uint64_t old = readCSR(csr);
			writeCSR(csr, zimm);
			writeReg(field.rd.value(), old);
			break;
		}  // CSRRWI
		case 0x6:
		{
			uint64_t old = readCSR(csr);
			if (zimm)
			{
				writeCSR(csr, old | zimm);
			}
			writeReg(field.rd.value(), old);
			break;
		}  // CSRRSI
		case 0x7:
		{
			uint64_t old = readCSR(csr);
			if (zimm)
			{
				writeCSR(csr, old & ~zimm);
			}
			writeReg(field.rd.value(), old);
			break;
		}  // CSRRCI
		default:
			throw std::runtime_error("unknown funct3 SYS at " + hex64(_pc - 4));
	}
}

// ── Trap ─────────────────────────────────────────────────────────────────────
void Cpu::takeTrap(uint64_t cause, uint64_t tvalue, uint64_t trapPc)
{
	const bool     isInterrupt = (cause >> 63) != 0;
	const uint32_t excCode     = static_cast<uint32_t>(cause & 0x7FFFFFFF);

	// Check if this exception should be delegated to S-mode
	const uint64_t medeleg  = readCSR(CSR::MEDELEG);
	const uint64_t mideleg  = readCSR(CSR::MIDELEG);
	const bool     delegate = (_privilege <= 1) &&  // currently in S or U mode
	                      (isInterrupt ? ((mideleg >> excCode) & 1) : ((medeleg >> excCode) & 1));

	if (delegate)
	{
		writeCSR(CSR::SEPC, trapPc);
		writeCSR(CSR::SCAUSE, static_cast<uint64_t>(cause));
		writeCSR(CSR::STVAL, tvalue);

		uint64_t       mstatus = readCSR(CSR::MSTATUS);
		const uint64_t sie     = (mstatus >> 1) & 0x1;
		mstatus &= ~(uint64_t{1} << 5);
		mstatus |= (sie << 5);           // SPIE = SIE
		mstatus &= ~(uint64_t{1} << 1);  // SIE = 0
		mstatus &= ~(uint64_t{1} << 8);
		mstatus |= (static_cast<uint64_t>(_privilege) << 8);  // SPP = current privilege
		writeCSR(CSR::MSTATUS, mstatus);

		_privilege = 1;  // enter S-mode

		const uint64_t stvec = readCSR(CSR::STVEC);
		const uint64_t base  = stvec & ~uint64_t{3};
		const uint64_t mode  = stvec & 0x3;
		_pc                  = (mode == 1 && isInterrupt) ? base + 4 * excCode : base;
	}
	else
	{
		// M-mode trap — existing code
		writeCSR(CSR::MEPC, trapPc);
		writeCSR(CSR::MCAUSE, cause);
		writeCSR(CSR::MTVAL, tvalue);

		uint64_t       mstatus = readCSR(CSR::MSTATUS);
		const uint64_t mie     = (mstatus >> 3) & 0x1;
		mstatus &= ~(uint64_t{1} << 7);
		mstatus |= (mie << 7);
		mstatus &= ~(uint64_t{1} << 3);
		mstatus &= ~(uint64_t{3} << 11);
		mstatus |= (static_cast<uint64_t>(_privilege) << 11);
		writeCSR(CSR::MSTATUS, mstatus);

		_privilege = 3;

		const uint64_t mtvec = readCSR(CSR::MTVEC);
		const uint64_t base  = mtvec & ~uint64_t{3};
		const uint64_t mode  = mtvec & 0x3;
		_pc                  = (mode == 1 && isInterrupt) ? base + 4 * excCode : base;
	}
}

void Cpu::checkInterrupts()
{
	const uint64_t mstatus = readCSR(CSR::MSTATUS);
	const uint64_t mie     = readCSR(CSR::MIE);
	uint64_t       mip     = readCSR(CSR::MIP);

	// Update mip from CLINT
	if (_clint.msipPending())
	{
		mip |= (uint64_t{1} << 3);
	}
	else
	{
		mip &= ~(uint64_t{1} << 3);
	}
	if (_clint.mtimePending())
	{
		mip |= (uint64_t{1} << 7);
	}
	else
	{
		mip &= ~(uint64_t{1} << 7);
	}
	_csrs[CSR::MIP] = mip;

	const bool mieEnabled = (_privilege < 3) || ((mstatus >> 3) & 1);
	if (!mieEnabled)
	{
		return;
	}

	const uint64_t pending = mip & mie;
	if (!pending)
	{
		return;
	}

	// Standard interrupt priority: MEI > MSI > MTI > SEI > SSI > STI
	static constexpr std::pair<uint32_t, uint64_t> priorities[] = {
	    {11, (uint64_t{1} << 63) | 11}, {3, (uint64_t{1} << 63) | 3}, {7, (uint64_t{1} << 63) | 7},
	    {9, (uint64_t{1} << 63) | 9},   {1, (uint64_t{1} << 63) | 1}, {5, (uint64_t{1} << 63) | 5},
	};
	for (const auto& [bit, cause] : priorities)
	{
		if (pending & (uint64_t{1} << bit))
		{
			takeTrap(cause, 0, _pc);
			return;
		}
	}
}

uint64_t Cpu::LB(const uint64_t addr)
{
  uint8_t value{};
  _memory.read8(addr, value);
  return sign_extend(static_cast<uint8_t>(value));

}

 uint64_t Cpu::LH(const uint64_t addr)
 {
  
 }
void LW(const uint64_t addr)
void LD(const uint64_t addr)
void LBU(const uint64_t addr)
void LHU(const uint64_t addr)
void LWU(const uint64_t addr)