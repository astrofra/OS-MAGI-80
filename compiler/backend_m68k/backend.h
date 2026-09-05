#ifndef MIGA80_COMPILER_BACKEND_M68K_H
#define MIGA80_COMPILER_BACKEND_M68K_H

#include <stdio.h>

#include "compiler/ir/ir.h"
#include "compiler/value_ir/value_ir.h"

int miga80_emit_gnu_m68k(FILE *output,
                         const struct miga80_ir_function *function,
                         struct miga80_diagnostic *diagnostic);
int miga80_emit_gnu_m68k_o1(FILE *output,
                            const struct miga80_value_function *function,
                            struct miga80_diagnostic *diagnostic);
int miga80_emit_gnu_m68k_fault_site(FILE *output, const char *function_name,
                                    unsigned int site,
                                    unsigned int line,
                                    unsigned int column);
int miga80_emit_gnu_m68k_fault_tail(FILE *output,
                                    const char *function_name);

#endif
