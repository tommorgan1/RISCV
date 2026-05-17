#pragma once

#include <cstdint>

#include "defs.hpp"

namespace decode
{
InstructionField decode(uint32_t instruction);
}