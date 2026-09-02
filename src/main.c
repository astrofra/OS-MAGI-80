#include <stddef.h>

#include <dos/dos.h>
#include <proto/dos.h>

static int write_message(BPTR output, const char *message, size_t length)
{
    LONG written;

    if (output == (BPTR)0 || length > 0x7fffffffUL) {
        return 0;
    }

    written = Write(output, message, (LONG)length);
    return written == (LONG)length;
}

int main(void)
{
    static const char banner[] =
        "MAGI-80 Phase 0 hosted bootstrap.\n"
        "AmigaOS API and 68020 soft-float build are operational.\n";

    if (!write_message(Output(), banner, sizeof(banner) - 1U)) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}
