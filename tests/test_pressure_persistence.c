#include <assert.h>
#include <string.h>
#include <math.h>
#include "pressure_tuning.h"
static uint8_t memory[256];
static int writes, fail_after=-1;
static bool read_fail;
void AppLog_Write(const char *l,const char *f,...){(void)l;(void)f;}
static bool rd(uint8_t a,uint8_t *p,size_t n){if(read_fail)return false;assert(a+n<=256);memcpy(p,memory+a,n);return true;}
static bool wr(uint8_t a,const uint8_t *p,size_t n){assert(a>=0x20 && a+n<=0x7f);for(size_t i=0;i<n;i++){if(fail_after>=0&&writes++==fail_after)return false;memory[a+i]=p[i];}return true;}
static float kp(void){PressureTuningProfile p;assert(PressureTuning_Get(1,&p));return p.kp;}
int main(void){
 memset(memory,0xff,sizeof(memory));PressureTuning_Init();assert(PressureTuning_Load(rd,wr));assert(PressureTuning_Persisted());
 uint8_t initialized[256];memcpy(initialized,memory,256);
 fail_after=0;writes=0;PressureTuning_Init();assert(PressureTuning_Load(rd,wr));assert(writes==0);fail_after=-1;
 assert(!memcmp(initialized,memory,256));
 PressureTuningProfile p;assert(PressureTuning_Get(1,&p));float original=p.kp;
 p.kp=123.4f;assert(PressureTuning_Apply(1,&p));assert(fabsf(kp()-123.4f)<.01f);
 PressureTuning_ResetDefaults();assert(kp()==original);
 assert(PressureTuning_SaveProfile(1,&p));assert(PressureTuning_Persisted());
 PressureTuning_Init();assert(PressureTuning_Load(rd,wr));assert(fabsf(kp()-123.4f)<.01f);
 uint8_t saved[256];memcpy(saved,memory,256);p.kp=222.2f;
 for(int cut=0;cut<50;cut++){
  memcpy(memory,saved,256);PressureTuning_Init();assert(PressureTuning_Load(rd,wr));writes=0;fail_after=cut;
  (void)PressureTuning_SaveProfile(1,&p);fail_after=-1;
  PressureTuning_Init();assert(PressureTuning_Load(rd,wr));assert(fabsf(kp()-123.4f)<.01f||fabsf(kp()-222.2f)<.01f);
  assert(!memcmp(memory,saved,0x20));assert(!memcmp(memory+0x80,saved+0x80,128));
 }
 PressureTuningProfile unchanged;assert(PressureTuning_Get(4,&unchanged));assert(unchanged.kp==50);
 p.kp=NAN;assert(!PressureTuning_SaveProfile(1,&p));
 read_fail=true;PressureTuning_Init();assert(!PressureTuning_Load(rd,wr));p.kp=20;assert(!PressureTuning_SaveProfile(1,&p));read_fail=false;
 /* Interrupted first initialization must recover on the following boot. */
 for(int cut=0;cut<50;cut++){
  memset(memory,0xff,sizeof(memory));PressureTuning_Init();writes=0;fail_after=cut;
  bool ok=PressureTuning_Load(rd,wr);assert(PressureTuning_Persisted()==ok);assert(kp()==original);
  fail_after=-1;PressureTuning_Init();assert(PressureTuning_Load(rd,wr));assert(PressureTuning_Persisted());assert(kp()==original);
 }
 memset(memory,0xff,sizeof(memory));read_fail=true;writes=0;fail_after=0;PressureTuning_Init();
 assert(!PressureTuning_Load(rd,wr));assert(writes==0);assert(!PressureTuning_Persisted());read_fail=false;fail_after=-1;
 PressureTuning_SetLease(true);assert(!PressureTuning_Apply(1,&p));
 return 0;
}
