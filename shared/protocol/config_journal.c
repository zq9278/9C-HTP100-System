#include "config_journal.h"
#include "system_wire.h"
#include <string.h>
const uint8_t Cj_Addresses[CJ_KEYS] = {0,2,4,6,8,10,12,16,18,20,24,26,28,0xf2,0xf8,0xfc,0xfe};
static const uint8_t banks[2] = {0x80,0xb0};
int Cj_Key(uint8_t address) { for (unsigned i=0;i<CJ_KEYS;i++) if(Cj_Addresses[i]==address)return (int)i;return -1; }
static bool read_record(CjRead read,int bank,uint8_t *record)
{
    for(unsigned i=0;i<CJ_RECORD_BYTES;i++)if(!read((uint8_t)(banks[bank]+i),record+i))return false;
    return record[0]==0x53 && record[1]==0x48 && record[2]==1 &&
           Sw_Get16(record+39)==Sw_Crc(record+2,37);
}
bool Cj_Load(ConfigJournal *j,CjRead read)
{
    uint8_t a[CJ_RECORD_BYTES],b[CJ_RECORD_BYTES];bool va=read_record(read,0,a),vb=read_record(read,1,b);
    j->active=-1;j->sequence=0;j->loaded=false;
    if(va||vb){
        bool use_b=vb&&(!va||Sw_IsNewer(Sw_Get16(b+3),Sw_Get16(a+3)));
        const uint8_t *r=use_b?b:a;j->active=use_b?1:0;j->sequence=Sw_Get16(r+3);memcpy(j->values,r+5,sizeof(j->values));
    }else{
        /* One-time legacy import. The legacy addresses remain unchanged. */
        for(unsigned i=0;i<CJ_KEYS;i++){
            uint8_t hi,lo;
            if(!read(Cj_Addresses[i],&hi)||!read((uint8_t)(Cj_Addresses[i]+1),&lo))return false;
            Sw_Put16(j->values+2*i,(uint16_t)((uint16_t)hi<<8)|lo);
        }
    }
    j->loaded=true;return true;
}
uint16_t Cj_Get(const ConfigJournal *j,uint8_t key){return key<CJ_KEYS?Sw_Get16(j->values+2*key):0xffff;}
bool Cj_Set(ConfigJournal *j,CjRead read,CjWrite write,uint8_t key,uint16_t value)
{
    return Cj_SetMany(j,read,write,&key,&value,1);
}
bool Cj_SetMany(ConfigJournal *j,CjRead read,CjWrite write,const uint8_t *keys,const uint16_t *values,unsigned count)
{
    if(!keys||!values||!count||count>CJ_KEYS||(!j->loaded&&!Cj_Load(j,read)))return false;
    bool changed=j->active<0;
    for(unsigned i=0;i<count;i++){if(keys[i]>=CJ_KEYS)return false;if(Cj_Get(j,keys[i])!=values[i])changed=true;}
    if(!changed)return true;
    uint8_t record[CJ_RECORD_BYTES]={0x53,0x48,1},verify[CJ_RECORD_BYTES];
    uint16_t seq=(uint16_t)(j->sequence+1);Sw_Put16(record+3,seq);
    memcpy(record+5,j->values,sizeof(j->values));
    for(unsigned i=0;i<count;i++)Sw_Put16(record+5+2*keys[i],values[i]);
    Sw_Put16(record+39,Sw_Crc(record+2,37));
    int bank=j->active==0?1:0;uint8_t base=banks[bank];
    if(!write(base,0)||!write((uint8_t)(base+1),0))return false;
    for(unsigned i=2;i<CJ_RECORD_BYTES;i++)if(!write((uint8_t)(base+i),record[i]))return false;
    if(!write(base,record[0])||!write((uint8_t)(base+1),record[1])||!read_record(read,bank,verify)||memcmp(record,verify,sizeof(record)))return false;
    memcpy(j->values,record+5,sizeof(j->values));j->sequence=seq;j->active=bank;return true;
}
