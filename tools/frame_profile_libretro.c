/* Headless Genesis Plus GX frame profiler (built by tools/frame_profile.py).
 * Extends verify_libretro.c: drains the -DPROF_HINT PC-sample ring each
 * refresh (PROF_*), hashes Plane A / linked SAT / tile VRAM per refresh
 * (HASH_OUT), dumps VRAM at chosen ticks (DUMP_TICKS/DUMP_DIR), saves frames
 * (SHOT_FRAMES/SHOT_DIR, PPM) and can force the region (REGION=pal|ntsc-u). */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
typedef bool (*Environment)(unsigned,void*);
struct Game { const char *path; const void *data; size_t size; const char *meta; };
struct Variable { const char *key,*value; };
struct Geometry { unsigned base_width,base_height,max_width,max_height;float aspect_ratio; };
struct Timing { double fps,sample_rate; };
struct AVInfo { struct Geometry geometry;struct Timing timing; };
static unsigned sample, players;
static bool started;
static bool survival, simulation_input, fixed_ticks;
static uint8_t *input_ram;
static unsigned input_fc, start_tick, input_step;
static uint32_t r32(uint8_t*r,unsigned a);
static unsigned pressure_addr, entity_addr, cpu_addr, large_addr, kpool_addr, main_addr, admission_addr, pacing_addr;
static unsigned pixfmt=0;
static bool environment(unsigned cmd,void *data) {
    if(cmd==10){pixfmt=*(unsigned*)data;return true;}
    if(cmd==9 || cmd==31) { *(const char**)data="/tmp";return true; }
    if(cmd==17) { *(bool*)data=false;return true; }
    if(cmd==15) { struct Variable*v=(struct Variable*)data;
        if(getenv("REGION") && strstr(v->key,"region_detect")){v->value=getenv("REGION");return true;}
        v->value=NULL;return false; }
    if(cmd==52) { *(unsigned*)data=0;return true; }
    return cmd==10 || cmd==16 || cmd==11 || cmd==35;
}
static unsigned frame;
static void video(const void*d,unsigned w,unsigned h,size_t p) {
    const char*sf=getenv("SHOT_FRAMES");if(!sf||!d)return;
    char key[16],hay[4096];snprintf(key,16,",%u,",frame);snprintf(hay,4096,",%s,",sf);if(!strstr(hay,key))return;
    char fn[512];snprintf(fn,512,"%s/shot-%u.ppm",getenv("SHOT_DIR"),frame);FILE*q=fopen(fn,"wb");fprintf(q,"P6 %u %u 255\n",w,h);
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){unsigned char px[3];
      if(pixfmt==1){uint32_t v=((const uint32_t*)((const char*)d+y*p))[x];px[0]=v>>16;px[1]=v>>8;px[2]=v;}
      else{uint16_t v=((const uint16_t*)((const char*)d+y*p))[x];px[0]=(v>>11)<<3;px[1]=((v>>5)&63)<<2;px[2]=(v&31)<<3;}
      fwrite(px,1,3,q);}
    fclose(q);}
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
    input_step=simulation_input?r32(input_ram,input_fc)-start_tick:sample;
    return id==7 || (id==0 && input_step%16<3) || (id==4 && input_step%200<24) || (id==5 && input_step%200>=24 && input_step%200<48);
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
    /* Optional Genesis Plus GX diagnostic export: register16 is the 68000 PC. */
    unsigned (*m68k_get_reg)(int)=dlsym(core,"m68k_get_reg");
    retro_set_environment(environment);retro_set_video_refresh(video);retro_set_audio_sample(audio);retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
    FILE*f=fopen(argv[2],"rb");if(!f)return 2;fseek(f,0,SEEK_END);long bytes=ftell(f);rewind(f);void*rom=malloc(bytes);if(fread(rom,1,bytes,f)!=(size_t)bytes)return 2;fclose(f);
    struct Game game={argv[2],rom,bytes,NULL};if(!retro_load_game(&game))return 2;
    FN(void,retro_get_system_av_info,(struct AVInfo*));
    struct AVInfo av;retro_get_system_av_info(&av);
    printf("CORE_TIMING fps=%.12f sample_rate=%.12f\n",av.timing.fps,av.timing.sample_rate);
    uint8_t*ram=retro_get_memory_data(2);if(!ram)return 2;
    unsigned prof_ptr_a=getenv("PROF_PTR")?strtoul(getenv("PROF_PTR"),NULL,16):0,prof_ring_a=getenv("PROF_RING")?strtoul(getenv("PROF_RING"),NULL,16):0;
    FILE*pf=prof_ptr_a?fopen(getenv("PROF_OUT"),"wb"):NULL;unsigned last_ptr=0;unsigned cnt_a=getenv("PROF_CNT")?strtoul(getenv("PROF_CNT"),NULL,16)&0xffff:0;
    unsigned f_addr=getenv("SONSON_F")?strtoul(getenv("SONSON_F"),NULL,16):0;
    unsigned e_addr=getenv("SONSON_E")?strtoul(getenv("SONSON_E"),NULL,16):0;
    unsigned d_addr=getenv("SONSON_D")?strtoul(getenv("SONSON_D"),NULL,16):0;
    unsigned c_addr=getenv("SONSON_C")?strtoul(getenv("SONSON_C"),NULL,16):0;
    simulation_input=getenv("SONSON_SIM_INPUT")!=NULL;
    fixed_ticks=getenv("SONSON_FIXED_TICKS")!=NULL;
    survival=getenv("SONSON_SURVIVAL")!=NULL;
    if(survival)pressure_addr=strtoul(getenv("SONSON_PRESSURE"),NULL,16);
    if(getenv("SONSON_ENTITIES"))entity_addr=strtoul(getenv("SONSON_ENTITIES"),NULL,16);
    if(getenv("SONSON_CPULOAD"))cpu_addr=strtoul(getenv("SONSON_CPULOAD"),NULL,16);
    if(getenv("SONSON_LARGE"))large_addr=strtoul(getenv("SONSON_LARGE"),NULL,16);
    if(getenv("SONSON_KPOOL"))kpool_addr=strtoul(getenv("SONSON_KPOOL"),NULL,16);
    if(getenv("SONSON_MAIN"))main_addr=strtoul(getenv("SONSON_MAIN"),NULL,16);
    if(getenv("SONSON_PACING"))pacing_addr=strtoul(getenv("SONSON_PACING"),NULL,16);
    if(getenv("SONSON_ADMISSION"))admission_addr=strtoul(getenv("SONSON_ADMISSION"),NULL,16);
    players=atoi(argv[3]);unsigned limit=atoi(argv[4]);unsigned fc=strtoul(argv[5],NULL,16),pc=strtoul(argv[6],NULL,16),lw=strtoul(argv[7],NULL,16),gg=strtoul(argv[8],NULL,16),np=strtoul(argv[9],NULL,16),attr=strtoul(argv[10],NULL,16);
    input_ram=ram;input_fc=fc;
    f=fopen(argv[11],"w");if(!f)return 2;fputs("refresh,tick,present,players,camera,entities,scheduled,cpu,pc,large,k_projectiles,simulation_elapsed,input_step,main_enemies,admission_hits,admission_misses,c_enemies,d_enemies,e_enemies,f_enemies,pending_ticks\n",f);
    const uint8_t *vr=(const uint8_t*)dlsym(core,"vram");FILE*hf=getenv("HASH_OUT")?fopen(getenv("HASH_OUT"),"w"):NULL;
    for(frame=0;frame<(fixed_ticks?limit*4:limit)+6000;frame++) {
        if(!started)w8(ram,np,players);
        else for(unsigned k=0;k<players;k++) {
            if(survival) {w8(ram,pressure_addr,6);w8(ram,gg+k*42+34,2);w8(ram,gg+k*42+36,0xEA);w8(ram,gg+k*42+37,0x60);}
            else w8(ram,gg+k*42+32,99);
        }
        retro_run();
        if(pf){unsigned cur=r32(ram,prof_ptr_a)&0xffff,base=prof_ring_a&0xffff;
         if(started&&last_ptr){unsigned n=0;uint32_t buf[256];unsigned q=last_ptr;
           while(q!=cur&&n<256){buf[n++]=r32(ram,q);q+=4;if(q>=base+1024)q=base;}
           uint32_t hdr[3]={frame,r32(ram,pc),n};fwrite(hdr,4,3,pf);uint16_t cn[8];for(int z=0;z<8;z++){cn[z]=cnt_a?r16(ram,cnt_a+2*z):0;}fwrite(cn,2,8,pf);fwrite(buf,4,n,pf);}
         if(cnt_a)for(int z=0;z<8;z++)if(z!=4&&z!=5){w8(ram,cnt_a+2*z,0);w8(ram,cnt_a+2*z+1,0);}
         last_ptr=cur;}
    if(!started) {
        /* Intro/menu RAM is not evidence of gameplay. Require valid mode and
         * counters before steering gameplay or sampling performance. */
        uint32_t ticks=r32(ram,fc),present=r32(ram,pc);
        if(r16(ram,lw)>0 && r16(ram,lw)<=32768 && !r8(ram,attr) &&
           r8(ram,gg+84)==players && ticks>10 && ticks<1000000 &&
           present>10 && present<1000000){started=true;start_tick=ticks;}
    }
        else {
            unsigned entities=0,scheduled=0,large=0,k_projectiles=0,main_enemies=0,c_enemies=0,d_enemies=0,e_enemies=0,f_enemies=0;
            if(f_addr)for(unsigned k=0;k<4;k++)if(r8(ram,f_addr+k*22+9))f_enemies++;
            if(e_addr)for(unsigned k=0;k<2;k++)if(r8(ram,e_addr+k*20+7))e_enemies++;
            if(d_addr)for(unsigned k=0;k<4;k++)if(r8(ram,d_addr+k*22+9))d_enemies++;
            if(c_addr)for(unsigned k=0;k<6;k++)if(r8(ram,c_addr+k*22+9))c_enemies++;
            if(main_addr)for(unsigned k=0;k<16;k++)if(r8(ram,main_addr+k*22+9))main_enemies++;
            if(large_addr)for(unsigned k=0;k<3;k++)if(r8(ram,large_addr+k*34+12))large++;
            if(kpool_addr)for(unsigned k=0;k<4;k++)if(r8(ram,kpool_addr+k*16+7))k_projectiles++;
            if(entity_addr)for(unsigned k=0;k<48;k++)if(r8(ram,entity_addr+k*42)){entities++;if(r8(ram,entity_addr+k*42+29))scheduled++;}
            fprintf(f,"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",frame,r32(ram,fc),r32(ram,pc),r8(ram,gg+84),r16(ram,gg+88),entities,scheduled,cpu_addr?r16(ram,cpu_addr):0,m68k_get_reg?m68k_get_reg(16):0,large,k_projectiles,r32(ram,fc)-start_tick,input_step,main_enemies,admission_addr?r32(ram,admission_addr+140):0,admission_addr?r32(ram,admission_addr+144):0,c_enemies,d_enemies,e_enemies,f_enemies,pacing_addr?r32(ram,pacing_addr+4):0);
            if(hf&&vr){uint32_t ha=2166136261u,hs=2166136261u,ht=2166136261u;
              for(unsigned z=0xE000;z<0xF000;z++)ha=(ha^vr[z])*16777619u;
              {unsigned sp=0;for(unsigned n=0;n<80;n++){unsigned base=0xF400+sp*8;for(unsigned z=base;z<base+8;z++)if(z!=base+3)hs=(hs^vr[z])*16777619u;sp=vr[base+3]&127;if(!sp)break;}}
              for(unsigned z=0;z<0xC000;z++)ht=(ht^vr[z])*16777619u;
              {static unsigned lastp=0;const char*dt=getenv("DUMP_TICKS");unsigned tk=r32(ram,fc),pr=r32(ram,pc);
               if(dt&&pr!=lastp){char key[16];snprintf(key,16,",%u,",tk);char hay[4096];snprintf(hay,4096,",%s,",dt);
                if(strstr(hay,key)){char fn[512];snprintf(fn,512,"%s/vram-%u.bin",getenv("DUMP_DIR"),tk);FILE*q=fopen(fn,"wb");fwrite(vr,1,65536,q);fclose(q);}}
               lastp=pr;}
              fprintf(hf,"%u,%u,%u,%08x,%08x,%08x\n",frame,r32(ram,fc),r32(ram,pc),ha,hs,ht);}
            ++sample;
            if(fixed_ticks?r32(ram,fc)-start_tick>=limit:sample>=limit)break;
        }
    }
    if(getenv("SONSON_VRAM_DUMP")){
        const void *v=dlsym(core,"vram"),*registers=dlsym(core,"reg");
        if(!v || !registers)return 3;
        FILE*dump=fopen(getenv("SONSON_VRAM_DUMP"),"wb");if(!dump)return 3;
        if(fwrite(v,1,65536,dump)!=65536 || fwrite(registers,1,32,dump)!=32)return 3;
        fclose(dump);
    }
    if(hf)fclose(hf);if(pf)fclose(pf);fclose(f);printf("samples=%u players=%u level_width=%u ticks=%u attract=%u public_players=%u\n",sample,players,r16(ram,lw),r32(ram,fc),r8(ram,attr),r8(ram,gg+84));return (fixed_ticks?r32(ram,fc)-start_tick>=limit:sample==limit)?0:1;
}
