#include "compiler/abi/abi.h"

#include <stddef.h>

int miga80_abi_scalar_argument_register(
    unsigned int index, enum miga80_abi_register *result)
{
    if (result == NULL || index >= MIGA80_ABI_MAX_SCALAR_ARGUMENTS) {
        return 0;
    }
    *result = (enum miga80_abi_register)(MIGA80_ABI_D0 + index);
    return 1;
}

int miga80_abi_address_argument_register(
    unsigned int index, enum miga80_abi_register *result)
{
    if (result == NULL || index >= MIGA80_ABI_MAX_ADDRESS_ARGUMENTS) {
        return 0;
    }
    *result = (enum miga80_abi_register)(MIGA80_ABI_A0 + index);
    return 1;
}

int miga80_abi_register_is_caller_saved(enum miga80_abi_register reg)
{
    return (reg >= MIGA80_ABI_D0 && reg <= MIGA80_ABI_D2) ||
           (reg >= MIGA80_ABI_A0 && reg <= MIGA80_ABI_A1);
}

int miga80_abi_register_is_callee_saved(enum miga80_abi_register reg)
{
    return (reg >= MIGA80_ABI_D3 && reg <= MIGA80_ABI_D7) ||
           (reg >= MIGA80_ABI_A2 && reg <= MIGA80_ABI_A6);
}

int miga80_abi_frame_size_is_valid(unsigned int size)
{
    return size <= MIGA80_ABI_MAX_FRAME_SIZE &&
           size % MIGA80_ABI_STACK_ALIGNMENT == 0U;
}

const char *miga80_abi_gnu_register_name(enum miga80_abi_register reg)
{
    static const char *const names[MIGA80_ABI_REGISTER_COUNT] = {
        "%d0", "%d1", "%d2", "%d3", "%d4", "%d5", "%d6", "%d7",
        "%a0", "%a1", "%a2", "%a3", "%a4", "%a5", "%a6", "%a7"
    };

    if (reg < MIGA80_ABI_D0 || reg >= MIGA80_ABI_REGISTER_COUNT) {
        return NULL;
    }
    return names[reg];
}
