#include <stddef.h>
#include <stdint.h>

#include <dos/dos.h>
#include <proto/dos.h>

static uint32_t checksum(const uint8_t *data, size_t length)
{
    uint32_t result = 0;

    for (size_t index = 0; index < length; ++index) {
        result += data[index];
    }

    return result;
}

int main(void)
{
    static const uint8_t test_vector[] = {1, 2, 3, 4};
    static const char message[] = "MAGI-80 C99/68020 smoke test passed.\n";
    BPTR output = Output();

    if (output == (BPTR)0 || checksum(test_vector, sizeof(test_vector)) != 10) {
        return RETURN_FAIL;
    }

    if (Write(output, message, (LONG)(sizeof(message) - 1)) < 0) {
        return RETURN_ERROR;
    }

    return RETURN_OK;
}
