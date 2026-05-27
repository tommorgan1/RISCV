#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

enum class memFault
{
	none,
	outOfBounds
};

class Memory
{
 public:
	Memory(uint64_t base, size_t size);

	bool load(std::span<const uint8_t> data, uint64_t address);

	memFault read8(uint64_t address, uint8_t& out) const;
	memFault read16(uint64_t address, uint16_t& out) const;
	memFault read32(uint64_t address, uint32_t& out) const;
	memFault read64(uint64_t address, uint64_t& out) const;

	memFault write8(uint64_t address, uint8_t value);
	memFault write16(uint64_t address, uint16_t value);
	memFault write32(uint64_t address, uint32_t value);
	memFault write64(uint64_t address, uint64_t value);

	size_t size() const { return _data.size(); }

 private:
	bool translate(uint64_t address, size_t accessSize, size_t& index) const;

	uint64_t             _base;
	std::vector<uint8_t> _data;
};