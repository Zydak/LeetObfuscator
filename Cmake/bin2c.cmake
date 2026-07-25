file(READ ${IN_FILE} hex_content HEX)
string(LENGTH "${hex_content}" hex_len)
math(EXPR byte_count "${hex_len} / 2")

string(REGEX MATCHALL "([A-Fa-f0-9][A-Fa-f0-9])" bytes "${hex_content}")
list(JOIN bytes ", 0x" byte_list)
set(byte_list "0x${byte_list}")

file(WRITE ${OUT_FILE}
"unsigned char ${VAR_NAME}[] = {\n${byte_list}\n};\nunsigned int ${VAR_NAME}_len = ${byte_count};\n")