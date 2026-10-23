"""Run a diagnostic cartridge and read named u16 results without a frontend."""
import json,os,subprocess
from pathlib import Path

def probe(rom,symbols,out,limit=600,capture_frames=()):
    core=Path(os.environ.get('GENPLUS_CORE',str(Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib')))
    # Diagnostic cartridges publish test_done last, then yield to VBlank.
    names=['test_done','test_error','test_batches','test_words','test_split']
    addresses=','.join(str(symbols[n]&65535) for n in names)
    source=r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
struct Game {const char *path;const void *data;size_t size;const char *meta;};
struct Variable {const char *key,*value;};
static unsigned pixel_format,frames;
static bool environment(unsigned cmd,void *p){
 if(cmd==10){pixel_format=*(unsigned*)p;return true;}
 if(cmd==9||cmd==31){*(const char**)p="/tmp";return true;}
 if(cmd==17){*(bool*)p=false;return true;}
 if(cmd==15){((struct Variable*)p)->value=NULL;return false;}
 if(cmd==52){*(unsigned*)p=0;return true;}
 return cmd==10||cmd==16||cmd==11||cmd==35;
}
static void video(const void*d,unsigned w,unsigned h,size_t p){(void)d;(void)w;(void)h;(void)p;}
static void audio(int16_t l,int16_t r){(void)l;(void)r;}
static size_t batch(const int16_t*d,size_t n){(void)d;return n;}
static void poll(void){}
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id){(void)p;(void)d;(void)i;(void)id;return 0;}
#define FN(ret,n,args) ret(*n)args=dlsym(core,#n);if(!n)return 2;
static unsigned word(uint8_t*r,unsigned a){return (r[a^1]<<8)|r[(a+1)^1];}
int main(int argc,char**argv){
 if(argc!=3)return 2;void *core=dlopen(argv[1],RTLD_NOW);if(!core)return 2;
 FN(void,retro_set_environment,(bool(*)(unsigned,void*)));
 FN(void,retro_set_video_refresh,(void(*)(const void*,unsigned,unsigned,size_t)));
 FN(void,retro_set_audio_sample,(void(*)(int16_t,int16_t)));
 FN(void,retro_set_audio_sample_batch,(size_t(*)(const int16_t*,size_t)));
 FN(void,retro_set_input_poll,(void(*)(void)));
 FN(void,retro_set_input_state,(int16_t(*)(unsigned,unsigned,unsigned,unsigned)));
 FN(void,retro_init,(void));FN(bool,retro_load_game,(const struct Game*));
 FN(void,retro_run,(void));FN(void*,retro_get_memory_data,(unsigned));
 FN(void,retro_unload_game,(void));FN(void,retro_deinit,(void));
 retro_set_environment(environment);retro_set_video_refresh(video);retro_set_audio_sample(audio);
 retro_set_audio_sample_batch(batch);retro_set_input_poll(poll);retro_set_input_state(input);retro_init();
 FILE*f=fopen(argv[2],"rb");if(!f)return 2;fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
 void*data=malloc(n);if(!data||fread(data,1,n,f)!=(size_t)n)return 2;fclose(f);
 struct Game g={argv[2],data,n,NULL};if(!retro_load_game(&g))return 2;
 uint8_t*ram=retro_get_memory_data(2);if(!ram)return 2;
 unsigned addresses[]={ADDRESSES};
 for(frames=0;frames<LIMIT;frames++){retro_run();if(word(ram,addresses[0])==1)break;}
 printf("PROBE %u %u %u %u %u\n",word(ram,addresses[0]),word(ram,addresses[1]),word(ram,addresses[2]),word(ram,addresses[3]),word(ram,addresses[4]));
 retro_unload_game();retro_deinit();free(data);dlclose(core);return 0;
}
'''.replace('ADDRESSES',addresses).replace('LIMIT',str(limit))
    if capture_frames:
        assert all(isinstance(f,int) and 0<=f<limit for f in capture_frames)
        callback=r'''static void video(const void*d,unsigned w,unsigned h,size_t pitch){
 if(!d || !(CAPTURE_CONDITION))return;
 char path[2048];snprintf(path,sizeof(path),CAPTURE_DIR "/frame-%u.ppm",frames);
 FILE *f=fopen(path,"wb");if(!f)exit(4);fprintf(f,"P6\n%u %u\n255\n",w,h);
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
  unsigned v=pixel_format==1?((const uint32_t*)((const uint8_t*)d+y*pitch))[x]:((const uint16_t*)((const uint8_t*)d+y*pitch))[x];
  unsigned char rgb[3];
  if(pixel_format==1){rgb[0]=v>>16;rgb[1]=v>>8;rgb[2]=v;}
  else if(pixel_format==2){rgb[0]=((v>>11)&31)*255/31;rgb[1]=((v>>5)&63)*255/63;rgb[2]=(v&31)*255/31;}
  else{rgb[0]=((v>>10)&31)*255/31;rgb[1]=((v>>5)&31)*255/31;rgb[2]=(v&31)*255/31;}
  fwrite(rgb,1,3,f);
 }fclose(f);
}'''.replace('CAPTURE_CONDITION','||'.join('frames=='+str(f) for f in capture_frames)).replace('CAPTURE_DIR',json.dumps(str(out)))
        source=source.replace('static void video(const void*d,unsigned w,unsigned h,size_t p){(void)d;(void)w;(void)h;(void)p;}',callback)
        for frame in capture_frames:(out/f'frame-{frame}.ppm').unlink(missing_ok=True)
    (out/'probe.c').write_text(source)
    subprocess.run(['cc','-O2',str(out/'probe.c'),'-o',str(out/'probe')],check=True)
    result=subprocess.run([str(out/'probe'),str(core),str(rom)],check=True,capture_output=True,text=True,timeout=90)
    (out/'core.log').write_text(result.stdout+result.stderr)
    line=next(l for l in result.stdout.splitlines() if l.startswith('PROBE '))
    return dict(zip(['done','error','batches','words','marker'],map(int,line.split()[1:])))
