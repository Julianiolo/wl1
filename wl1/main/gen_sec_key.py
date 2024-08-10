import secrets

sec = secrets.token_bytes(32)

s = "#ifndef __SEC_KEY_H__\n#define __SEC_KEY_H__\n\n#include <cstdint>\n\n"
initializer_list = ", ".join(hex(x) for x in sec)
s += f"static const uint8_t sec_key[32] = {{{initializer_list}}};"
s += "\n\n"
s += "#endif\n"

with open("sec_key.h", "w") as f:
    f.write(s)
