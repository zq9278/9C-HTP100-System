#include "config_journal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t eeprom[256];static int writes_left;
static bool rd(uint8_t a,uint8_t*v){*v=eeprom[a];return true;}
static bool wr(uint8_t a,uint8_t v){if(writes_left--==0)return false;eeprom[a]=v;return true;}
int main(void)
{
    memset(eeprom,0xff,sizeof(eeprom));ConfigJournal j={0};assert(Cj_Load(&j,rd));
    writes_left=1000;assert(Cj_Set(&j,rd,wr,0,100));uint8_t baseline[256];memcpy(baseline,eeprom,256);
    for(int fail=0;fail<50;fail++){
        memcpy(eeprom,baseline,256);assert(Cj_Load(&j,rd));writes_left=fail;
        bool committed=Cj_Set(&j,rd,wr,0,101);ConfigJournal reboot={0};assert(Cj_Load(&reboot,rd));
        assert(Cj_Get(&reboot,0)==100||Cj_Get(&reboot,0)==101);
        if(committed)assert(Cj_Get(&reboot,0)==101);
        assert(Cj_Get(&reboot,1)==0xffff);
    }
    memcpy(eeprom,baseline,256);assert(Cj_Load(&j,rd));writes_left=1000;assert(Cj_Set(&j,rd,wr,0,101));
    eeprom[0xb0+8]^=1;assert(Cj_Load(&j,rd));assert(Cj_Get(&j,0)==100);
    const uint8_t keys[]={0,1};const uint16_t values[]={200,300};
    for(int fail=0;fail<50;fail++){
        memcpy(eeprom,baseline,256);assert(Cj_Load(&j,rd));writes_left=fail;
        (void)Cj_SetMany(&j,rd,wr,keys,values,2);assert(Cj_Load(&j,rd));
        assert((Cj_Get(&j,0)==100&&Cj_Get(&j,1)==0xffff)||(Cj_Get(&j,0)==200&&Cj_Get(&j,1)==300));
    }
    puts("PASS: journal power loss at every write byte and CRC fallback");
}
