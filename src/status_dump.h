#if STATUS_DUMP
#include "hwstate.h"

void type_string(const char* str);
void type_number(uint32_t val);
bool status_dump_check(const hwstate& rightSide, const hwstate& leftSide);

#endif
