#if !defined(DBGCFG_H)
#define DBGCFG_H

#if DEBUG
#define DBG(a) a

void dumpVal(unsigned long v, const char* header = nullptr);
void dumpHex(unsigned long v, const char* header = nullptr);

#if DEBUG == 2
#define DBG2(a) a
#else
#define DBG2(a)
#endif
#else
#define DBG(a)
#define DBG2(a)
#endif

#endif
