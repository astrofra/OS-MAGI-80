#ifndef MIGA80_MUSASHI_CONFIG_H
#define MIGA80_MUSASHI_CONFIG_H

/* Keep the test core deliberately narrow: the stock A1200 CPU only. */
#define M68K_EMULATE_010 0
#define M68K_EMULATE_EC020 1
#define M68K_EMULATE_020 0
#define M68K_EMULATE_030 0
#define M68K_EMULATE_040 0

/* The runner uses the hook to execute and account for one instruction. */
#define M68K_INSTRUCTION_HOOK 1

#include "m68kconf.h"

#endif
