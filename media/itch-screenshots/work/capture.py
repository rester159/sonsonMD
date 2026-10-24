import pathlib,re,ast,subprocess,concurrent.futures
root=pathlib.Path.cwd()/'sonson_native';dest=root.parent/'itch-screenshots';work=dest/'work'
syms={n:int(v,16)&0xffffff for v,n in re.findall(r'^([0-9a-fA-F]+)\s+\w\s+(\w+)\s*$',(root/'out/symbol.txt').read_text(),re.M)}
def literal(file,name):
 tree=ast.parse(file.read_text())
 return next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==name for t in n.targets))
s=literal(root/'tools/perf_run.py','script')
s=s.replace('if samples==1200 or (samples>=20584 and samples<=20590) then','if samples%500==0 then')
s=s.replace('mem:write_u8(gg+(k-1)*42+32,99)','mem:write_u8(gg+(k-1)*42+32,3)')
values={'CPULOAD':syms['cpuFrameLoad'],'PC':syms['g_presentCounter'],'FC':syms['g_frameCounter'],'LW':syms['g_levelW'],'GG':syms['g_game'],'NP':syms['g_numPlayers'],'ATTR':syms['g_attract'],'ENT':syms['g_entities'],'PLAYERS':2,'FRAMES':19500,'OUTPUT':'"'+str(work/'frames.csv')+'"'}
for k,v in values.items():s=re.sub(r'\b'+k+r'\b',str(v),s)
(work/'game.lua').write_text(s)
s=literal(root/'tools/verify_home_ui.py','s')
s=s.replace('if fr==1390 then','if fr==1390 then').replace('fr=fr+1','fr=fr+1\n if fr==1000 or fr==1150 then manager.machine.video:snapshot() end')
for k,v in {'CONFIG':syms['g_config'],'MODE':syms['g_mode'],'LW':syms['g_levelW'],'GG':syms['g_game'],'HC':syms['g_homeCredits'],'PAUSE':syms['g_paused'],'FC':syms['g_frameCounter'],'RESULT':'"'+str(work/'menu-results.txt')+'"'}.items():s=re.sub(r'\b'+k+r'\b',str(v),s)
(work/'menus.lua').write_text(s)
def run(name):
 subprocess.run(['mame','genesis','-cart',str(root/'out/rom.bin'),'-autoboot_script',str(work/(name+'.lua')),'-skip_gameinfo','-window','-keyboardprovider','none','-mouseprovider','none','-joystickprovider','none','-nothrottle','-video','none','-sound','none','-noreadconfig','-snapshot_directory',str(work/name)],cwd=root,stdout=(work/(name+'.log')).open('w'),stderr=subprocess.STDOUT,check=True)
with concurrent.futures.ThreadPoolExecutor() as p:list(p.map(run,['game','menus']))
