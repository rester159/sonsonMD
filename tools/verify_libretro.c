/* Headless Genesis Plus GX check. No emulator window or graphics device. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
typedef bool (*Environment)(unsigned,void*);
struct Game { const char *path; const void *data; size_t size; const char *meta; };
struct Variable { const char *key,*value; };
static unsigned frame, sample, players;
static bool started;
static bool survival;
static unsigned pressure_addr, entity_addr, cpu_addr;
static bool environment(unsigned cmd,void *data) {
    if(cmd==9 || cmd==31) { *(const char**)data="/tmp";return true; }
    if(cmd==17) { *(bool*)data=false;return true; }
    if(cmd==15) { ((struct Variable*)data)->value=NULL;return false; }
    if(cmd==52) { *(unsigned*)data=0;return true; }
    return cmd==10 || cmd==16 || cmd==11 || cmd==35;
}
static void video(const void*d,unsigned w,unsigned h,size_t p) {(void)d;(void)w;(void)h;(void)p;}
static void audio(int16_t l,int16_t r) {(void)l;(void)r;}
static size_t batch(const int16_t*d,size_t n) {(void)d;return n;}
static void poll(void) {}
static int16_t input(unsigned port,unsigned device,unsigned index,unsigned id) {
    (void)device;(void)index;
    if(!started && survival) {
        if(port)return 0;
        return (id==5 && ((frame>=900 && frame<912)||(frame>=1040 && frame<1052)||(frame>=1080 && frame<1092)||(players==2 && frame>=1120 && frame<1132))) ||
               (id==3 && ((frame>=940 && frame<952)||(frame>=1160 && frame<1190)));
    }
    if(!started) {
        if(frame<740) return id==3 && frame%24<12 && port==0;
        if(port>=players)return 0;
        return (id==8 && frame%48<8) || (id==3 && frame%48>=24 && frame%48<36);
    }
    if(port>=players)return 0;
    return id==7 || (id==0 && sample%16<3) || (id==4 && sample%200<24) || (id==5 && sample%200>=24 && sample%200<48);
}
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n)return 2;
static uint8_t r8(uint8_t*r,unsigned a) {return r[(a&65535)^1];}
static uint32_t r32(uint8_t*r,unsigned a) {return (uint32_t)r8(r,a)<<24 | (uint32_t)r8(r,a+1)<<16 | r8(r,a+2)<<8 | r8(r,a+3);}
static unsigned r16(uint8_t*r,unsigned a) {return r8(r,a)*256+r8(r,a+1);}
static void w8(uint8_t*r,unsigned a,unsigned v) {r[(a&65535)^1]=v;}
int main(int argc,char**argv) {
    if(argc!=12) return 2;
    void*core=dlopen(argv[1],RTLD_NOW);if(!core){puts(dlerror());return 2;}
    FN(void,retro_set_environment,(Environment));FN(void,retro_set_video_refresh,(void(*)(const void*,unsigned,unsigned,size_t)));
    FN(void,retro_set_audio_sample,(void(*)(int16_t,int16_t)));FN(void,retro_set_audio_sample_batch,(size_t(*)(const int16_t*,size_t)));
    FN(void,retro_set_input_poll,(void(*)(void)));FN(void,retro_set_input_state,(int16_t(*)(unsigned,unsigned,unsigned,unsigned)));
    FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct Game*));FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));
    retro_set_environment(environment);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
    FILE*f=fopen(argv[2],"rb");if(!f)return 2;fseek(f,0,SEEK_END);long bytes=ftell(f);rewind(f);void*rom=malloc(bytes);if(fread(rom,1,bytes,f)!=(size_t)bytes)return 2;fclose(f);
    struct Game game={argv[2],rom,bytes,NULL};if(!retro_load_game(&game))return 2;
    uint8_t*ram=retro_get_memory_data(2);if(!ram)return 2;
    survival=getenv("SONSON_SURVIVAL")!=NULL;
    if(survival){pressure_addr=strtoul(getenv("SONSON_PRESSURE"),NULL,16);entity_addr=strtoul(getenv("SONSON_ENTITIES"),NULL,16);cpu_addr=strtoul(getenv("SONSON_CPULOAD"),NULL,16);}
    players=atoi(argv[3]);unsigned limit=atoi(argv[4]);unsigned fc=strtoul(argv[5],NULL,16),pc=strtoul(argv[6],NULL,16),lw=strtoul(argv[7],NULL,16),gg=strtoul(argv[8],NULL,16),np=strtoul(argv[9],NULL,16),attr=strtoul(argv[10],NULL,16);
    f=fopen(argv[11],"w");if(!f)return 2;fputs("refresh,tick,present,players,camera,entities,scheduled,cpu\n",f);
    for(frame=0;frame<limit+6000;frame++) {
        if(!started)w8(ram,np,players);
        else for(unsigned k=0;k<players;k++) {
            if(survival) {w8(ram,pressure_addr,6);w8(ram,gg+k*42+34,2);w8(ram,gg+k*42+36,0xEA);w8(ram,gg+k*42+37,0x60);}
            else w8(ram,gg+k*42+32,99);
        }
        retro_run();
    if(!started) {if(r16(ram,lw)>0 && !r8(ram,attr) && r32(ram,fc)>10)started=true;}
        else {
            unsigned entities=0,scheduled=0;
            if(survival)for(unsigned k=0;k<48;k++)if(r8(ram,entity_addr+k*42)){entities++;if(r8(ram,entity_addr+k*42+29))scheduled++;}
            fprintf(f,"%u,%u,%u,%u,%u,%u,%u,%u\n",frame,r32(ram,fc),r32(ram,pc),r8(ram,gg+84),r16(ram,gg+88),entities,scheduled,survival?r16(ram,cpu_addr):0);
            if(++sample>=limit)break;
        }
    }
    fclose(f);printf("samples=%u players=%u\n",sample,players);return sample==limit?0:1;
}
