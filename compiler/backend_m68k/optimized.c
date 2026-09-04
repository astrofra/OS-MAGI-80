#include "compiler/backend_m68k/backend.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler/abi/abi.h"

#define MIGA80_DATA_REGISTER_COUNT 8U
#define MIGA80_SPILL_REGISTER_COUNT 7U
#define MIGA80_SPILL_SCRATCH_REGISTER 7
#define MIGA80_NO_REGISTER (-1)
#define MIGA80_NO_SPILL_SLOT UINT_MAX

struct allocation_plan {
    int registers[MIGA80_MAX_VALUE_INSTRUCTIONS];
    unsigned int spill_slots[MIGA80_MAX_VALUE_INSTRUCTIONS];
    unsigned int last_use[MIGA80_MAX_VALUE_INSTRUCTIONS];
    int saved_registers[MIGA80_DATA_REGISTER_COUNT];
    unsigned int spill_slot_count;
};

static int output_line(FILE *output, const char *format, ...)
{
    int written;
    va_list arguments;

    va_start(arguments, format);
    written = vfprintf(output, format, arguments);
    va_end(arguments);
    return written >= 0;
}

static int fail(struct miga80_diagnostic *diagnostic, unsigned int line,
                unsigned int column, const char *message)
{
    diagnostic->line = line;
    diagnostic->column = column;
    (void)snprintf(diagnostic->message, sizeof(diagnostic->message), "%s",
                   message);
    return 0;
}

static int opcode_has_left(enum miga80_value_opcode opcode)
{
    return opcode == MIGA80_VALUE_NEG || opcode == MIGA80_VALUE_ADD ||
           opcode == MIGA80_VALUE_SUB || opcode == MIGA80_VALUE_MUL ||
           (opcode >= MIGA80_VALUE_EQ && opcode <= MIGA80_VALUE_GE_I32) ||
           opcode == MIGA80_VALUE_PHI;
}

static int opcode_has_right(enum miga80_value_opcode opcode)
{
    return opcode == MIGA80_VALUE_ADD || opcode == MIGA80_VALUE_SUB ||
           opcode == MIGA80_VALUE_MUL ||
           (opcode >= MIGA80_VALUE_EQ && opcode <= MIGA80_VALUE_GE_I32) ||
           opcode == MIGA80_VALUE_PHI;
}

static int opcode_is_commutative(enum miga80_value_opcode opcode)
{
    return opcode == MIGA80_VALUE_ADD || opcode == MIGA80_VALUE_MUL ||
           opcode == MIGA80_VALUE_EQ || opcode == MIGA80_VALUE_NE;
}

static int validate_value_function(
    const struct miga80_value_function *function,
    struct miga80_diagnostic *diagnostic)
{
    unsigned char seen_blocks[MIGA80_MAX_BASIC_BLOCKS];
    unsigned int index;

    if (function->value_count == 0U ||
        function->value_count > MIGA80_MAX_VALUE_INSTRUCTIONS ||
        function->result >= function->value_count ||
        !function->values[function->result].live ||
        function->block_count == 0U ||
        function->block_count > MIGA80_MAX_BASIC_BLOCKS ||
        function->block_order_count != function->block_count ||
        function->entry_block >= function->block_count) {
        return fail(diagnostic, 0U, 0U, "invalid O1 value function");
    }
    (void)memset(seen_blocks, 0, sizeof(seen_blocks));
    for (index = 0U; index < function->block_order_count; ++index) {
        const unsigned int block_index = function->block_order[index];
        const struct miga80_value_basic_block *block;
        unsigned int edge;

        if (block_index >= function->block_count || seen_blocks[block_index]) {
            return fail(diagnostic, 0U, 0U,
                        "invalid O1 basic-block order");
        }
        seen_blocks[block_index] = 1U;
        block = &function->blocks[block_index];
        if (block->first_value > function->value_count ||
            block->value_count > function->value_count - block->first_value ||
            block->successor_count > MIGA80_MAX_BLOCK_SUCCESSORS ||
            block->terminator > MIGA80_VALUE_BRANCH ||
            (block->terminator == MIGA80_VALUE_RETURN &&
             block->successor_count != 0U) ||
            (block->terminator == MIGA80_VALUE_JUMP &&
             block->successor_count != 1U) ||
            (block->terminator == MIGA80_VALUE_BRANCH &&
             (block->successor_count != 2U ||
              block->condition >= function->value_count ||
              function->values[block->condition].type != MIGA80_TYPE_BOOL))) {
            return fail(diagnostic, 0U, 0U, "invalid O1 basic block");
        }
        for (edge = 0U; edge < block->successor_count; ++edge) {
            if (block->successors[edge] >= function->block_count) {
                return fail(diagnostic, 0U, 0U,
                            "invalid O1 basic-block successor");
            }
        }
    }
    for (index = 0U; index < function->value_count; ++index) {
        const struct miga80_value_instruction *value =
            &function->values[index];

        if (!value->live) {
            continue;
        }
        if ((value->type != MIGA80_TYPE_I32 &&
             value->type != MIGA80_TYPE_BOOL) ||
            value->opcode < MIGA80_VALUE_CONSTANT ||
            value->opcode > MIGA80_VALUE_PHI) {
            return fail(diagnostic, value->line, value->column,
                        "invalid O1 value instruction");
        }
        if ((opcode_has_left(value->opcode) && value->left >= index) ||
            (opcode_has_right(value->opcode) && value->right >= index)) {
            return fail(diagnostic, value->line, value->column,
                        "invalid O1 value operand");
        }
        if (value->opcode == MIGA80_VALUE_PARAMETER &&
            (value->parameter_index >= function->parameter_count ||
             value->type !=
                 function->parameter_types[value->parameter_index])) {
            return fail(diagnostic, value->line, value->column,
                        "invalid O1 parameter index");
        }
        if (value->opcode == MIGA80_VALUE_PHI &&
            (value->left_block >= function->block_count ||
             value->right_block >= function->block_count ||
             function->values[value->left].type != value->type ||
             function->values[value->right].type != value->type)) {
            return fail(diagnostic, value->line, value->column,
                        "invalid O1 phi predecessor");
        }
        if (value->opcode == MIGA80_VALUE_CONSTANT &&
            value->type == MIGA80_TYPE_BOOL && value->immediate > 1U) {
            return fail(diagnostic, value->line, value->column,
                        "invalid O1 bool constant");
        }
    }
    if (function->values[function->result].type != function->result_type) {
        return fail(diagnostic, 0U, 0U, "invalid O1 result type");
    }
    return 1;
}

static int find_free_register(const unsigned int *owners,
                              unsigned int register_count,
                              unsigned int preferred)
{
    unsigned int reg;

    if (preferred < register_count &&
        owners[preferred] == MIGA80_INVALID_VALUE) {
        return (int)preferred;
    }
    for (reg = 0U; reg < register_count; ++reg) {
        if (owners[reg] == MIGA80_INVALID_VALUE) {
            return (int)reg;
        }
    }
    return MIGA80_NO_REGISTER;
}

static int build_allocation_plan(const struct miga80_value_function *function,
                                 unsigned int register_count,
                                 struct allocation_plan *plan,
                                 struct miga80_diagnostic *diagnostic)
{
    unsigned int owners[MIGA80_DATA_REGISTER_COUNT];
    unsigned int spill_owners[MIGA80_MAX_VALUE_INSTRUCTIONS];
    unsigned int index;

    plan->spill_slot_count = 0U;
    for (index = 0U; index < MIGA80_MAX_VALUE_INSTRUCTIONS; ++index) {
        plan->registers[index] = MIGA80_NO_REGISTER;
        plan->spill_slots[index] = MIGA80_NO_SPILL_SLOT;
        plan->last_use[index] = index;
        spill_owners[index] = MIGA80_INVALID_VALUE;
    }
    for (index = 0U; index < MIGA80_DATA_REGISTER_COUNT; ++index) {
        owners[index] = MIGA80_INVALID_VALUE;
        plan->saved_registers[index] = 0;
    }

    for (index = 0U; index < function->value_count; ++index) {
        const struct miga80_value_instruction *value =
            &function->values[index];

        if (!value->live) {
            continue;
        }
        if (opcode_has_left(value->opcode)) {
            plan->last_use[value->left] = index;
        }
        if (opcode_has_right(value->opcode)) {
            plan->last_use[value->right] = index;
        }
    }
    for (index = 0U; index < function->block_count; ++index) {
        const struct miga80_value_basic_block *block =
            &function->blocks[index];

        if (block->terminator == MIGA80_VALUE_BRANCH) {
            const unsigned int end = block->first_value + block->value_count;
            const unsigned int use = end == 0U ? 0U : end - 1U;

            if (plan->last_use[block->condition] < use) {
                plan->last_use[block->condition] = use;
            }
        }
    }
    plan->last_use[function->result] = function->value_count;

    for (index = 0U; index < function->value_count; ++index) {
        const struct miga80_value_instruction *value =
            &function->values[index];

        if (value->live && value->opcode == MIGA80_VALUE_PHI) {
            const unsigned int frame_size =
                (plan->spill_slot_count + 1U) * 4U;

            if (plan->spill_slot_count == MIGA80_MAX_VALUE_INSTRUCTIONS ||
                !miga80_abi_frame_size_is_valid(frame_size)) {
                return fail(diagnostic, value->line, value->column,
                            "O1 phi frame exceeds ABI limit");
            }
            plan->spill_slots[index] = plan->spill_slot_count;
            spill_owners[plan->spill_slot_count++] = index;
            plan->saved_registers[MIGA80_SPILL_SCRATCH_REGISTER] = 1;
        }
    }

    for (index = 0U; index < function->value_count; ++index) {
        const struct miga80_value_instruction *value =
            &function->values[index];

        if (value->live && value->opcode == MIGA80_VALUE_PARAMETER) {
            enum miga80_abi_register abi_register;
            unsigned int reg;

            if (!miga80_abi_scalar_argument_register(value->parameter_index,
                                                       &abi_register) ||
                abi_register < MIGA80_ABI_D0 ||
                abi_register > MIGA80_ABI_D7) {
                return fail(diagnostic, value->line, value->column,
                            "O1 parameter register is invalid");
            }
            reg = (unsigned int)(abi_register - MIGA80_ABI_D0);
            if (owners[reg] != MIGA80_INVALID_VALUE) {
                return fail(diagnostic, value->line, value->column,
                            "O1 parameter register is assigned twice");
            }
            owners[reg] = index;
            plan->registers[index] = (int)reg;
        }
    }

    for (index = 0U; index < function->value_count; ++index) {
        const struct miga80_value_instruction *value =
            &function->values[index];
        int left_register = MIGA80_NO_REGISTER;
        int right_register = MIGA80_NO_REGISTER;
        int destination = MIGA80_NO_REGISTER;
        unsigned int destination_spill = MIGA80_NO_SPILL_SLOT;
        unsigned int preferred = MIGA80_DATA_REGISTER_COUNT;
        unsigned int owner_index;

        for (owner_index = 0U; owner_index < register_count; ++owner_index) {
            const unsigned int owner = owners[owner_index];

            if (owner != MIGA80_INVALID_VALUE &&
                plan->last_use[owner] < index) {
                owners[owner_index] = MIGA80_INVALID_VALUE;
            }
        }
        for (owner_index = 0U; owner_index < plan->spill_slot_count;
             ++owner_index) {
            const unsigned int owner = spill_owners[owner_index];

            if (owner != MIGA80_INVALID_VALUE &&
                plan->last_use[owner] < index) {
                spill_owners[owner_index] = MIGA80_INVALID_VALUE;
            }
        }

        if (!value->live || value->opcode == MIGA80_VALUE_CONSTANT ||
            value->opcode == MIGA80_VALUE_PARAMETER) {
            continue;
        }
        if (opcode_has_left(value->opcode)) {
            left_register = plan->registers[value->left];
        }
        if (opcode_has_right(value->opcode)) {
            right_register = plan->registers[value->right];
        }

        if (value->opcode == MIGA80_VALUE_PHI) {
            if (left_register != MIGA80_NO_REGISTER &&
                plan->last_use[value->left] == index) {
                owners[left_register] = MIGA80_INVALID_VALUE;
            }
            if (right_register != MIGA80_NO_REGISTER &&
                plan->last_use[value->right] == index) {
                owners[right_register] = MIGA80_INVALID_VALUE;
            }
            if (plan->spill_slots[value->left] != MIGA80_NO_SPILL_SLOT &&
                plan->last_use[value->left] == index) {
                spill_owners[plan->spill_slots[value->left]] =
                    MIGA80_INVALID_VALUE;
            }
            if (plan->spill_slots[value->right] != MIGA80_NO_SPILL_SLOT &&
                plan->last_use[value->right] == index) {
                spill_owners[plan->spill_slots[value->right]] =
                    MIGA80_INVALID_VALUE;
            }
            continue;
        }

        if (function->result == index) {
            preferred = 0U;
        }
        if (value->opcode == MIGA80_VALUE_MUL &&
            function->values[value->right].opcode == MIGA80_VALUE_CONSTANT &&
            function->values[value->right].immediate == 3U) {
            destination =
                find_free_register(owners, register_count, preferred);
        }
        if (destination == MIGA80_NO_REGISTER &&
            left_register != MIGA80_NO_REGISTER &&
            plan->last_use[value->left] == index) {
            destination = left_register;
        }
        if (destination == MIGA80_NO_REGISTER &&
            opcode_is_commutative(value->opcode) &&
            right_register != MIGA80_NO_REGISTER &&
            plan->last_use[value->right] == index) {
            destination = right_register;
        }
        if (destination == MIGA80_NO_REGISTER) {
            destination =
                find_free_register(owners, register_count, preferred);
        }
        if (destination == MIGA80_NO_REGISTER) {
            unsigned int slot;

            if (opcode_has_left(value->opcode) &&
                plan->spill_slots[value->left] != MIGA80_NO_SPILL_SLOT &&
                plan->last_use[value->left] == index) {
                destination_spill = plan->spill_slots[value->left];
            } else if (opcode_has_right(value->opcode) &&
                       plan->spill_slots[value->right] !=
                           MIGA80_NO_SPILL_SLOT &&
                       plan->last_use[value->right] == index) {
                destination_spill = plan->spill_slots[value->right];
            } else {
                for (slot = 0U; slot < plan->spill_slot_count; ++slot) {
                    if (spill_owners[slot] == MIGA80_INVALID_VALUE) {
                        destination_spill = slot;
                        break;
                    }
                }
                if (destination_spill == MIGA80_NO_SPILL_SLOT) {
                    const unsigned int frame_size =
                        (plan->spill_slot_count + 1U) * 4U;

                    if (plan->spill_slot_count ==
                            MIGA80_MAX_VALUE_INSTRUCTIONS ||
                        !miga80_abi_frame_size_is_valid(frame_size)) {
                        return fail(diagnostic, value->line, value->column,
                                    "O1 spill frame exceeds ABI limit");
                    }
                    destination_spill = plan->spill_slot_count++;
                }
            }
        }

        if (left_register != MIGA80_NO_REGISTER &&
            plan->last_use[value->left] == index &&
            left_register != destination) {
            owners[left_register] = MIGA80_INVALID_VALUE;
        }
        if (right_register != MIGA80_NO_REGISTER &&
            plan->last_use[value->right] == index &&
            right_register != destination) {
            owners[right_register] = MIGA80_INVALID_VALUE;
        }
        if (opcode_has_left(value->opcode) &&
            plan->spill_slots[value->left] != MIGA80_NO_SPILL_SLOT &&
            plan->last_use[value->left] == index &&
            plan->spill_slots[value->left] != destination_spill) {
            spill_owners[plan->spill_slots[value->left]] =
                MIGA80_INVALID_VALUE;
        }
        if (opcode_has_right(value->opcode) &&
            plan->spill_slots[value->right] != MIGA80_NO_SPILL_SLOT &&
            plan->last_use[value->right] == index &&
            plan->spill_slots[value->right] != destination_spill) {
            spill_owners[plan->spill_slots[value->right]] =
                MIGA80_INVALID_VALUE;
        }
        if (destination != MIGA80_NO_REGISTER) {
            owners[destination] = index;
            plan->registers[index] = destination;
            if (destination >= (int)(MIGA80_ABI_D3 - MIGA80_ABI_D0)) {
                plan->saved_registers[destination] = 1;
            }
        } else {
            plan->spill_slots[index] = destination_spill;
            spill_owners[destination_spill] = index;
            plan->saved_registers[MIGA80_SPILL_SCRATCH_REGISTER] = 1;
        }
    }
    return 1;
}

static const char *data_register_name(int reg)
{
    return miga80_abi_gnu_register_name(
        (enum miga80_abi_register)(MIGA80_ABI_D0 + reg));
}

static unsigned int spill_offset(const struct allocation_plan *plan,
                                 unsigned int source)
{
    return (plan->spill_slots[source] + 1U) * 4U;
}

static int emit_move(FILE *output,
                     const struct miga80_value_function *function,
                     const struct allocation_plan *plan, unsigned int source,
                     int destination)
{
    const struct miga80_value_instruction *value = &function->values[source];

    if (value->opcode == MIGA80_VALUE_CONSTANT) {
        const uint32_t constant = value->immediate;

        if (constant <= UINT32_C(127)) {
            return output_line(output, "        moveq   #%u,%s\n",
                               (unsigned int)constant,
                               data_register_name(destination));
        }
        if (constant >= UINT32_C(0xffffff80)) {
            return output_line(output, "        moveq   #-%u,%s\n",
                               (unsigned int)(0U - constant),
                               data_register_name(destination));
        }
        return output_line(output, "        move.l  #0x%08x,%s\n",
                           (unsigned int)constant,
                           data_register_name(destination));
    }
    if (plan->registers[source] == destination) {
        return 1;
    }
    if (plan->registers[source] != MIGA80_NO_REGISTER) {
        return output_line(output, "        move.l  %s,%s\n",
                           data_register_name(plan->registers[source]),
                           data_register_name(destination));
    }
    if (plan->spill_slots[source] != MIGA80_NO_SPILL_SLOT) {
        return output_line(output, "        move.l  -%u(%%a6),%s\n",
                           spill_offset(plan, source),
                           data_register_name(destination));
    }
    return 0;
}

static int emit_register_source(FILE *output, const char *operation,
                                const struct allocation_plan *plan,
                                unsigned int source, int destination)
{
    if (plan->registers[source] != MIGA80_NO_REGISTER) {
        return output_line(output, "        %-7s %s,%s\n", operation,
                           data_register_name(plan->registers[source]),
                           data_register_name(destination));
    }
    if (plan->spill_slots[source] != MIGA80_NO_SPILL_SLOT) {
        return output_line(output, "        %-7s -%u(%%a6),%s\n", operation,
                           spill_offset(plan, source),
                           data_register_name(destination));
    }
    return 0;
}

static int emit_add_immediate(FILE *output, uint32_t constant,
                              int destination)
{
    if (constant >= 1U && constant <= 8U) {
        return output_line(output, "        addq.l  #%u,%s\n",
                           (unsigned int)constant,
                           data_register_name(destination));
    }
    if (constant >= UINT32_C(0xfffffff8)) {
        return output_line(output, "        subq.l  #%u,%s\n",
                           (unsigned int)(0U - constant),
                           data_register_name(destination));
    }
    return output_line(output, "        add.l   #0x%08x,%s\n",
                       (unsigned int)constant,
                       data_register_name(destination));
}

static int emit_sub_immediate(FILE *output, uint32_t constant,
                              int destination)
{
    if (constant >= 1U && constant <= 8U) {
        return output_line(output, "        subq.l  #%u,%s\n",
                           (unsigned int)constant,
                           data_register_name(destination));
    }
    if (constant >= UINT32_C(0xfffffff8)) {
        return output_line(output, "        addq.l  #%u,%s\n",
                           (unsigned int)(0U - constant),
                           data_register_name(destination));
    }
    return output_line(output, "        sub.l   #0x%08x,%s\n",
                       (unsigned int)constant,
                       data_register_name(destination));
}

static unsigned int power_of_two_shift(uint32_t constant)
{
    unsigned int shift = 0U;

    if (constant == 0U || (constant & (constant - 1U)) != 0U) {
        return 0U;
    }
    while (constant > 1U) {
        constant >>= 1;
        ++shift;
    }
    return shift;
}

static int emit_multiply(FILE *output,
                         const struct miga80_value_function *function,
                         const struct allocation_plan *plan,
                         const struct miga80_value_instruction *value,
                         int destination, unsigned int source)
{
    const struct miga80_value_instruction *operand = &function->values[source];

    if (operand->opcode == MIGA80_VALUE_CONSTANT) {
        const uint32_t constant = operand->immediate;
        const unsigned int shift = power_of_two_shift(constant);

        if (constant == 2U) {
            return output_line(output, "        add.l   %s,%s\n",
                               data_register_name(destination),
                               data_register_name(destination));
        }
        if (constant == 3U &&
            destination != plan->registers[value->left]) {
            return output_line(output, "        add.l   %s,%s\n",
                               data_register_name(destination),
                               data_register_name(destination)) &&
                   emit_register_source(output, "add.l", plan, value->left,
                                        destination);
        }
        if (shift >= 1U && shift <= 8U) {
            return output_line(output, "        lsl.l   #%u,%s\n", shift,
                               data_register_name(destination));
        }
        return output_line(output, "        muls.l  #0x%08x,%s\n",
                           (unsigned int)constant,
                           data_register_name(destination));
    }
    return emit_register_source(output, "muls.l", plan, source,
                                destination);
}

static int store_spilled_result(FILE *output,
                                const struct allocation_plan *plan,
                                unsigned int index, int source)
{
    if (plan->spill_slots[index] == MIGA80_NO_SPILL_SLOT) {
        return 1;
    }
    return output_line(output, "        move.l  %s,-%u(%%a6)\n",
                       data_register_name(source), spill_offset(plan, index));
}

static int emit_comparison(FILE *output,
                           const struct miga80_value_function *function,
                           const struct allocation_plan *plan,
                           enum miga80_value_opcode opcode,
                           unsigned int source, int destination)
{
    const struct miga80_value_instruction *operand = &function->values[source];
    const char *condition = "seq";
    int compared;

    if (opcode == MIGA80_VALUE_NE) {
        condition = "sne";
    } else if (opcode == MIGA80_VALUE_LT_I32) {
        condition = "slt";
    } else if (opcode == MIGA80_VALUE_LE_I32) {
        condition = "sle";
    } else if (opcode == MIGA80_VALUE_GT_I32) {
        condition = "sgt";
    } else if (opcode == MIGA80_VALUE_GE_I32) {
        condition = "sge";
    }
    if (operand->opcode == MIGA80_VALUE_CONSTANT) {
        compared = output_line(output, "        cmp.l   #0x%08x,%s\n",
                               (unsigned int)operand->immediate,
                               data_register_name(destination));
    } else {
        compared = emit_register_source(output, "cmp.l", plan, source,
                                        destination);
    }
    return compared &&
           output_line(output,
                       "        %-7s %s\n"
                       "        and.l   #1,%s\n",
                       condition, data_register_name(destination),
                       data_register_name(destination));
}

static int emit_value(FILE *output,
                      const struct miga80_value_function *function,
                      const struct allocation_plan *plan, unsigned int index)
{
    const struct miga80_value_instruction *value = &function->values[index];
    const int destination = plan->registers[index] != MIGA80_NO_REGISTER
                                ? plan->registers[index]
                                : MIGA80_SPILL_SCRATCH_REGISTER;
    unsigned int source = value->right;
    int emitted;

    if (value->opcode == MIGA80_VALUE_NEG) {
        emitted =
            emit_move(output, function, plan, value->left, destination) &&
            output_line(output, "        neg.l   %s\n",
                        data_register_name(destination));
        return emitted &&
               store_spilled_result(output, plan, index, destination);
    }

    if (opcode_is_commutative(value->opcode) &&
        plan->registers[value->right] == destination &&
        function->values[value->right].opcode != MIGA80_VALUE_CONSTANT) {
        source = value->left;
    } else if (!emit_move(output, function, plan, value->left, destination)) {
        return 0;
    }

    if (value->opcode >= MIGA80_VALUE_EQ &&
        value->opcode <= MIGA80_VALUE_GE_I32) {
        emitted = emit_comparison(output, function, plan, value->opcode,
                                  source, destination);
        return emitted &&
               store_spilled_result(output, plan, index, destination);
    }
    if (function->values[source].opcode == MIGA80_VALUE_CONSTANT) {
        if (value->opcode == MIGA80_VALUE_ADD) {
            emitted = emit_add_immediate(
                output, function->values[source].immediate, destination);
            return emitted &&
                   store_spilled_result(output, plan, index, destination);
        }
        if (value->opcode == MIGA80_VALUE_SUB) {
            emitted = emit_sub_immediate(
                output, function->values[source].immediate, destination);
            return emitted &&
                   store_spilled_result(output, plan, index, destination);
        }
    }
    if (value->opcode == MIGA80_VALUE_ADD) {
        emitted = emit_register_source(output, "add.l", plan, source,
                                       destination);
    } else if (value->opcode == MIGA80_VALUE_SUB) {
        emitted = emit_register_source(output, "sub.l", plan, source,
                                       destination);
    } else {
        emitted =
            emit_multiply(output, function, plan, value, destination, source);
    }
    return emitted && store_spilled_result(output, plan, index, destination);
}

static int build_saved_register_list(const struct allocation_plan *plan,
                                     char *text, size_t text_size)
{
    size_t used = 0U;
    unsigned int reg;

    text[0] = '\0';
    for (reg = 3U; reg < MIGA80_DATA_REGISTER_COUNT; ++reg) {
        int written;

        if (!plan->saved_registers[reg]) {
            continue;
        }
        written = snprintf(text + used, text_size - used, "%s%s",
                           used == 0U ? "" : "/", data_register_name((int)reg));
        if (written < 0 || (size_t)written >= text_size - used) {
            return 0;
        }
        used += (size_t)written;
    }
    return 1;
}

static int emit_phi_edge(FILE *output,
                         const struct miga80_value_function *function,
                         const struct allocation_plan *plan,
                         unsigned int predecessor,
                         unsigned int successor)
{
    const struct miga80_value_basic_block *block =
        &function->blocks[successor];
    unsigned int offset;

    for (offset = 0U; offset < block->value_count; ++offset) {
        const unsigned int phi_index = block->first_value + offset;
        const struct miga80_value_instruction *phi =
            &function->values[phi_index];
        unsigned int source;

        if (!phi->live || phi->opcode != MIGA80_VALUE_PHI) {
            continue;
        }
        if (phi->left_block == predecessor) {
            source = phi->left;
        } else if (phi->right_block == predecessor) {
            source = phi->right;
        } else {
            return 0;
        }
        if (plan->spill_slots[phi_index] == MIGA80_NO_SPILL_SLOT) {
            return 0;
        }
        if (function->values[source].opcode == MIGA80_VALUE_CONSTANT) {
            if (!output_line(output, "        move.l  #0x%08x,-%u(%%a6)\n",
                             (unsigned int)function->values[source].immediate,
                             spill_offset(plan, phi_index))) {
                return 0;
            }
        } else if (plan->registers[source] != MIGA80_NO_REGISTER) {
            if (!output_line(output, "        move.l  %s,-%u(%%a6)\n",
                             data_register_name(plan->registers[source]),
                             spill_offset(plan, phi_index))) {
                return 0;
            }
        } else if (plan->spill_slots[source] ==
                   plan->spill_slots[phi_index]) {
            continue;
        } else if (plan->spill_slots[source] != MIGA80_NO_SPILL_SLOT) {
            if (!output_line(output,
                             "        move.l  -%u(%%a6),%%d7\n"
                             "        move.l  %%d7,-%u(%%a6)\n",
                             spill_offset(plan, source),
                             spill_offset(plan, phi_index))) {
                return 0;
            }
        } else {
            return 0;
        }
    }
    return 1;
}

static int emit_epilogue(FILE *output, const char *saved_registers,
                         unsigned int frame_size)
{
    if (saved_registers[0] != '\0' &&
        !output_line(output, "        movem.l (%%a7)+,%s\n",
                     saved_registers)) {
        return 0;
    }
    if (frame_size != 0U && !output_line(output, "        unlk    %%a6\n")) {
        return 0;
    }
    return output_line(output, "        rts\n");
}

static int emit_jump_edge(FILE *output,
                          const struct miga80_value_function *function,
                          const struct allocation_plan *plan,
                          unsigned int predecessor,
                          unsigned int successor)
{
    return emit_phi_edge(output, function, plan, predecessor, successor) &&
           output_line(output, "        bra     .L_%s_b%u\n", function->name,
                       successor);
}

static int emit_branch(FILE *output,
                       const struct miga80_value_function *function,
                       const struct allocation_plan *plan,
                       unsigned int block_index)
{
    const struct miga80_value_basic_block *block =
        &function->blocks[block_index];
    const struct miga80_value_instruction *condition =
        &function->values[block->condition];

    if (condition->opcode == MIGA80_VALUE_CONSTANT) {
        const unsigned int successor =
            condition->immediate != 0U ? block->successors[0]
                                       : block->successors[1];

        return emit_jump_edge(output, function, plan, block_index, successor);
    }
    if (plan->registers[block->condition] != MIGA80_NO_REGISTER) {
        if (!output_line(output, "        tst.l   %s\n",
                         data_register_name(
                             plan->registers[block->condition]))) {
            return 0;
        }
    } else if (plan->spill_slots[block->condition] != MIGA80_NO_SPILL_SLOT) {
        if (!output_line(output,
                         "        move.l  -%u(%%a6),%%d7\n"
                         "        tst.l   %%d7\n",
                         spill_offset(plan, block->condition))) {
            return 0;
        }
    } else {
        return 0;
    }
    return output_line(output, "        beq     .L_%s_b%u_false\n",
                       function->name, block_index) &&
           emit_jump_edge(output, function, plan, block_index,
                          block->successors[0]) &&
           output_line(output, ".L_%s_b%u_false:\n", function->name,
                       block_index) &&
           emit_jump_edge(output, function, plan, block_index,
                          block->successors[1]);
}

static int emit_allocated_function(
    FILE *output, const struct miga80_value_function *function,
    const struct allocation_plan *plan,
    struct miga80_diagnostic *diagnostic)
{
    char saved_registers[64];
    unsigned int frame_size;
    unsigned int order_index;

    frame_size = plan->spill_slot_count * 4U;
    if (!miga80_abi_frame_size_is_valid(frame_size) ||
        !build_saved_register_list(plan, saved_registers,
                                   sizeof(saved_registers))) {
        if (diagnostic->message[0] == '\0') {
            (void)fail(diagnostic, 0U, 0U,
                       "unable to build O1 saved-register set");
        }
        return 0;
    }

    if (!output_line(output,
                     "/* Generated by miga80c -O1 for native ABI %u.%u; "
                     "development oracle only. */\n"
                     "        .text\n"
                     "        .even\n"
                     "        .globl  %s\n"
                     "%s:\n",
                     MIGA80_ABI_VERSION_MAJOR, MIGA80_ABI_VERSION_MINOR,
                     function->name, function->name)) {
        return fail(diagnostic, 0U, 0U, "unable to write O1 assembly");
    }
    if (frame_size != 0U &&
        !output_line(output, "        link.w  %%a6,#-%u\n", frame_size)) {
        return fail(diagnostic, 0U, 0U, "unable to write O1 spill frame");
    }
    if (saved_registers[0] != '\0' &&
        !output_line(output, "        movem.l %s,-(%%a7)\n",
                     saved_registers)) {
        return fail(diagnostic, 0U, 0U, "unable to write O1 prologue");
    }

    for (order_index = 0U; order_index < function->block_order_count;
         ++order_index) {
        const unsigned int block_index = function->block_order[order_index];
        const struct miga80_value_basic_block *block =
            &function->blocks[block_index];
        unsigned int offset;

        if (!output_line(output, ".L_%s_b%u:\n", function->name,
                         block_index)) {
            return fail(diagnostic, 0U, 0U, "unable to write O1 block label");
        }
        for (offset = 0U; offset < block->value_count; ++offset) {
            const unsigned int index = block->first_value + offset;
            const struct miga80_value_instruction *value =
                &function->values[index];

            if (value->live && value->opcode != MIGA80_VALUE_CONSTANT &&
                value->opcode != MIGA80_VALUE_PARAMETER &&
                value->opcode != MIGA80_VALUE_PHI &&
                !emit_value(output, function, plan, index)) {
                return fail(diagnostic, value->line, value->column,
                            "unable to write O1 value instruction");
            }
        }
        if (block->terminator == MIGA80_VALUE_BRANCH) {
            if (!emit_branch(output, function, plan, block_index)) {
                return fail(diagnostic, 0U, 0U,
                            "unable to write O1 conditional branch");
            }
        } else if (block->terminator == MIGA80_VALUE_JUMP) {
            if (!emit_jump_edge(output, function, plan, block_index,
                                block->successors[0])) {
                return fail(diagnostic, 0U, 0U,
                            "unable to write O1 jump edge");
            }
        } else if (!emit_move(output, function, plan, function->result, 0) ||
                   !emit_epilogue(output, saved_registers, frame_size)) {
            return fail(diagnostic, 0U, 0U,
                        "unable to write O1 return");
        }
    }
    if (fflush(output) != 0 || ferror(output)) {
        return fail(diagnostic, 0U, 0U, "unable to finish O1 assembly");
    }
    return 1;
}

int miga80_emit_gnu_m68k_o1(FILE *output,
                            const struct miga80_value_function *function,
                            struct miga80_diagnostic *diagnostic)
{
    struct allocation_plan *plan;
    int emitted;

    if (output == NULL || function == NULL || diagnostic == NULL) {
        return 0;
    }
    (void)memset(diagnostic, 0, sizeof(*diagnostic));
    if (!validate_value_function(function, diagnostic)) {
        return 0;
    }
    plan = (struct allocation_plan *)malloc(sizeof(*plan));
    if (plan == NULL) {
        return fail(diagnostic, 0U, 0U,
                    "unable to allocate O1 register plan");
    }
    if (!build_allocation_plan(function, MIGA80_DATA_REGISTER_COUNT, plan,
                               diagnostic)) {
        free(plan);
        return 0;
    }
    if (plan->spill_slot_count != 0U &&
        !build_allocation_plan(function, MIGA80_SPILL_REGISTER_COUNT, plan,
                               diagnostic)) {
        free(plan);
        return 0;
    }
    emitted = emit_allocated_function(output, function, plan, diagnostic);
    free(plan);
    return emitted;
}
