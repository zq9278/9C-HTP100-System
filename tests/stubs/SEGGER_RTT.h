#include <stdint.h>
#define SEGGER_RTT_MODE_NO_BLOCK_SKIP 0
void SEGGER_RTT_Init(void);
int SEGGER_RTT_ConfigUpBuffer(unsigned,const char*,void*,unsigned,unsigned);
int SEGGER_RTT_ConfigDownBuffer(unsigned,const char*,void*,unsigned,unsigned);
unsigned SEGGER_RTT_Read(unsigned,void*,unsigned);
unsigned SEGGER_RTT_Write(unsigned,const void*,unsigned);

typedef struct { unsigned SizeOfBuffer; volatile unsigned RdOff,WrOff; } TestRttRing;
typedef struct { TestRttRing aUp[3],aDown[3]; } TestRttCB;
extern TestRttCB _SEGGER_RTT;
int SEGGER_RTT_SetNameDownBuffer(unsigned,const char*);
