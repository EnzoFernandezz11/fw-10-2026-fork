
#include "demo.h"
#include "print.h"
#include "strutils.h"

void demo_str(void)
{
    strbuf_t buf;
    strbuf_init(&buf);
    strbuf_set(&buf, "value=");
    strbuf_append(&buf, "42");
    print_str(strbuf_cstr(&buf));
    print_str("\n");

    char trimmed[STR_MAX_LEN];
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    str_trim(trimmed, "  hello  ", sizeof(trimmed));
    uint8_t crc = str_crc8(data, sizeof(data));
    print_str("trimmed: '");
    print_str(trimmed);
    print_str("'\ncrc8: 0x");
    print_hex(crc, 2);
    print_str("\n");
}
