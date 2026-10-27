#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <math.h>
struct Game{const char*path;const void*data;size_t size;const char*meta;};
struct Var{const char*key,*value;};
static unsigned slot; static uint64_t n[45],clip[45];static double sum[45],sq[45];static unsigned peak[45];
static bool env(unsigned c,void*d){if(c==9||c==31){*(const char**)d="/tmp";return true;}if(c==17){*(bool*)d=false;return true;}if(c==15){((struct Var*)d)->value=NULL;return false;}if(c==52){*(unsigned*)d=0;return true;}return c==10||c==16||c==11||c==35;}
static void video(const void*d,unsigned w,unsigned h,size_t p){}
static void audio(int16_t l,int16_t r){int v[2]={l,r};if(slot>44)return;for(int i=0;i<2;i++){unsigned a=abs(v[i]);n[slot]++;sum[slot]+=v[i];sq[slot]+=(double)v[i]*v[i];if(a>peak[slot])peak[slot]=a;if(a>=32767)clip[slot]++;}}
static size_t batch(const int16_t*d,size_t n){for(size_t i=0;i<n;i++)audio(d[i*2],d[i*2+1]);return n;}
static void poll(void){} static int16_t input(unsigned p,unsigned d,unsigned i,unsigned b){return 0;}
#define FN(t,n,a) t(*n)a=dlsym(core,#n);if(!n)return 2
int main(int argc,char**argv){if(argc!=4)return 2;void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
FN(void,retro_set_environment,(bool(*)(unsigned,void*)));FN(void,retro_set_video_refresh,(void(*)(const void*,unsigned,unsigned,size_t)));FN(void,retro_set_audio_sample,(void(*)(int16_t,int16_t)));FN(void,retro_set_audio_sample_batch,(size_t(*)(const int16_t*,size_t)));FN(void,retro_set_input_poll,(void(*)(void)));FN(void,retro_set_input_state,(int16_t(*)(unsigned,unsigned,unsigned,unsigned)));FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct Game*));FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));
retro_set_environment(env);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
FILE*f=fopen(argv[2],"rb");if(!f)return 2;fseek(f,0,SEEK_END);long len=ftell(f);rewind(f);void*b=malloc(len);if(fread(b,1,len,f)!=len)return 2;fclose(f);struct Game g={argv[2],b,len,NULL};if(!retro_load_game(&g))return 2;
uint8_t*ram=retro_get_memory_data(2);unsigned a=strtoul(argv[3],0,16)&65535, done=0;
for(unsigned i=0;i<26000;i++){slot=ram[a^1]*256+ram[(a+1)^1];retro_run();if(slot==44&&++done==120)break;}
printf("case,samples,rms,peak,clipped\n");for(unsigned i=0;i<45;i++)if(n[i])printf("%u,%llu,%.3f,%u,%llu\n",i,(unsigned long long)n[i],sqrt(sq[i]/n[i]-pow(sum[i]/n[i],2)),peak[i],(unsigned long long)clip[i]);return done==120?0:3;}
