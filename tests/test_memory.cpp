#include <catch2/catch_test_macros.hpp>

#include "memory.hpp"

static constexpr uint64_t    base = 0x80000000ULL;
static constexpr std::size_t MiB  = 1024 * 1024;

TEST_CASE("Memory size is correct", "[memory]")
{
	Memory mem(base, 4 * MiB);
	REQUIRE(mem.size() == 4 * MiB);
}

TEST_CASE("Read-after-write round trips all widths", "[memory]")
{
	Memory mem(base, 4 * MiB);
	mem.write8(base + 0, 0xAB);
	mem.write16(base + 2, 0xDEAD);
	mem.write32(base + 4, 0xDEADBEEF);
	mem.write64(base + 8, 0xCAFEBABEDEADBEEFULL);

	uint8_t  v8{};
	uint16_t v16{};
	uint32_t v32{};
	uint64_t v64{};
	mem.read8(base + 0, v8);
	mem.read16(base + 2, v16);
	mem.read32(base + 4, v32);
	mem.read64(base + 8, v64);

	REQUIRE(v8 == 0xAB);
	REQUIRE(v16 == 0xDEAD);
	REQUIRE(v32 == 0xDEADBEEF);
	REQUIRE(v64 == 0xCAFEBABEDEADBEEFULL);
}

TEST_CASE("Little-endian byte order", "[memory]")
{
	Memory mem(base, 4 * MiB);
	mem.write32(base, 0x04030201);
	uint8_t b{};
	mem.read8(base + 0, b);
	REQUIRE(b == 0x01);
	mem.read8(base + 1, b);
	REQUIRE(b == 0x02);
	mem.read8(base + 2, b);
	REQUIRE(b == 0x03);
	mem.read8(base + 3, b);
	REQUIRE(b == 0x04);
}

TEST_CASE("Out-of-bounds access returns fault", "[memory]")
{
	Memory  mem(base, 4 * MiB);
	uint8_t v{};
	REQUIRE(mem.read8(base - 1, v) == memFault::outOfBounds);
	REQUIRE(mem.read8(base + 4 * MiB, v) == memFault::outOfBounds);
	uint64_t v64{};
	REQUIRE(mem.read64(base + 4 * MiB - 4, v64) == memFault::outOfBounds);
}