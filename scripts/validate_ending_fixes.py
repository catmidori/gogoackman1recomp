"""Replay the ending fixes in both pixel aspects; keep saves/config untouched.
Uses dummy SDL drivers and compares actual PPU frames, not a physical display.
"""
import argparse,csv,json,os,re,shutil,subprocess
from pathlib import Path

root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser()
ap.add_argument('--exe',type=Path,required=True)
ap.add_argument('--rom',type=Path,required=True)
ap.add_argument('--out',type=Path,default=root/'private/ending-fixes-validation')
a=ap.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
work=out/'runner';work.mkdir(exist_ok=True);(work/'saves').mkdir(exist_ok=True)
shutil.copy2(a.exe.resolve(),work/'AckmanRecomp.exe')
config=(root/'config.ini').read_text().replace('DisableFrameDelay = 0','DisableFrameDelay = 1').replace('VSync = 1','VSync = 0')
replay='wait 1\nloadstate 0\nwait 10000\n'
advance='wait 1\nloadstate 0\nwait 30\n'+('press a+b 1\nwait 30\n'*350)

def run(prefix,frames=1000,snapshot=10,**extra):
 env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SNESRECOMP_RUN_FRAMES=str(frames),ACKMAN_FADE_TRACE=str(prefix)+'.fade.csv',ACKMAN_VIDEO_TRACE=str(prefix)+'.video.csv',ACKMAN_DUMP_FRAME=str(snapshot),ACKMAN_DUMP_PREFIX=str(prefix),SNESRECOMP_SCREENSHOT=str(prefix)+'.ppm',SNESRECOMP_SCREENSHOT_FRAME=str(snapshot))
 env.update(extra)
 with Path(str(prefix)+'.log').open('w') as log:
  p=subprocess.run([str(work/'AckmanRecomp.exe'),'--script',str(work/'replay.txt'),'--no-launcher',str(a.rom.resolve())],cwd=work,env=env,stdout=log,stderr=log,timeout=120)
 text=Path(str(prefix)+'.log').read_text(errors='replace')
 assert p.returncode==0 and f'RUN_FRAMES reached after {frames} frames' in text,prefix
 assert not re.search(r'SEH CRASH|slice watchdog|CPU-port guest-clock sync timed out|load refused',text),prefix

def ppm(prefix):
 _,dim,_,data=Path(str(prefix)+'.ppm').read_bytes().split(b'\n',3)
 w,h=map(int,dim.split());assert len(data)==w*h*3
 return w,h,data

def row(data,w,y,x,n):return data[(y*w+x)*3:(y*w+x+n)*3]

results=[]
scenes=[('final créditos 02',7),('final',4),('final 02',8),('final tela de the end',6)]
for aspect in ['4:3','8:7']:
 (work/'config.ini').write_text(re.sub(r'DisplayAspect\s*=.*',f'DisplayAspect = {aspect}',config))
 for idx,(name,policy) in enumerate(scenes):
  shutil.copy2(root/'saves'/f'ackman-beam-v20 - {name}.sav',work/'saves/GGA1_save_01.sav')
  (work/'replay.txt').write_text(replay)
  prefix=out/f'{aspect.replace(":", "-")}-{idx}'
  run(prefix)
  native=Path(str(prefix)+'-native');run(native,12,ACKMAN_WIDE_EXPERIMENTAL='0')
  w,h,pix=ppm(prefix);nw,nh,npix=ppm(native);margin=(w-256)//2
  assert (nw,nh,h)==(256,224,224)
  assert all(row(pix,w,y,margin,256)==row(npix,nw,y,0,256) for y in range(h)),('native center',name,aspect)
  for ext in ['wram','vram','oam','spc-ram']:
   assert Path(str(prefix)+'.'+ext).read_bytes()==Path(str(native)+'.'+ext).read_bytes(),('guest state',name,aspect,ext)
  trace=list(csv.DictReader(Path(str(prefix)+'.fade.csv').open()))
  assert trace[9]['effective_policy']==str(policy)
  assert int(trace[9]['left'])==margin==int(trace[9]['right'])
  if idx==0:
   layers=Path(str(prefix)+'-landscape');run(layers,12,SNESRECOMP_LAYER_MASK='3')
   lw,lh,lp=ppm(layers);assert (lw,lh)==(w,h)
   # OBJ and BG3 cannot leak outside; margins are exactly the two landscapes.
   assert all(row(pix,w,y,0,margin)==row(lp,w,y,0,margin) and row(pix,w,y,w-margin,margin)==row(lp,w,y,w-margin,margin) for y in range(h))
   # The previously black upper landscape now fills both physical sides.
   assert all(any(row(pix,w,y,0,margin)) and any(row(pix,w,y,w-margin,margin)) for y in range(0,60))
  if idx==1:
   bg=Path(str(prefix)+'-sky');run(bg,12,SNESRECOMP_LAYER_MASK='2')
   bw,bh,bp=ppm(bg);assert (bw,bh)==(w,h)
   # Actual castle OBJ pixels survive to the right of native x=256.
   count=sum(row(pix,w,y,x,1)!=row(bp,w,y,x,1) for y in range(h) for x in range(w-margin,w))
   assert count>100,('castle margin',aspect,count)
  if idx==2:
   assert all(any(row(pix,w,y,0,margin)) and any(row(pix,w,y,w-margin,margin)) for y in range(h))
  results.append(dict(scene=name,aspect=aspect,frames=1000,policy=policy,native_center_identical=True,guest_state_identical=True))
  print('PASS',name,aspect,flush=True)
 # Exercise actual immediate dialogue progression, including a black endpoint.
 shutil.copy2(root/'saves'/'ackman-beam-v20 - final 02.sav',work/'saves/GGA1_save_01.sav')
 (work/'replay.txt').write_text(advance)
 prefix=out/f'{aspect.replace(":", "-")}-dialogue-advance';run(prefix,5500,1100)
 trace=list(csv.DictReader(Path(str(prefix)+'.fade.csv').open()))
 for script,policy in [('9D0D','8'),('9F08','5'),('AC82','5')]:
  visible=[r for r in trace if r['selector']=='2D00' and r['script']==script and r['black']=='0']
  assert visible and all(r['effective_policy']==policy and int(r['left'])>0 and int(r['right'])>0 for r in visible),(script,aspect)
 results.append(dict(scene='post-final dialogue progression',aspect=aspect,frames=5500,scripts=['9D0D','9F08','AC82']))
# B523 must return to THE END clipping after its landscape phase goes black.
shutil.copy2(root/'saves'/'ackman-beam-v20 - final créditos 02.sav',work/'saves/GGA1_save_01.sav')
(work/'replay.txt').write_text(advance)
prefix=out/'credits-advance';run(prefix,10500,10000)
trace=list(csv.DictReader(Path(str(prefix)+'.fade.csv').open()))
seen={r['effective_policy'] for r in trace if r['selector']=='2D00' and r['script']=='B523' and r['black']=='0'}
assert {'6','7'}<=seen,('shared ending script phases',seen)
for previous,current in zip(trace,trace[1:]):
 if previous['effective_policy']!=current['effective_policy']:
  assert previous['black']=='1',('fade latch',previous,current)
results.append(dict(scene='credits landscape to THE END',aspect='8:7',frames=10500,policies=[7,6]))
(out/'results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
print('PASS: both aspects, native pixels/state, ending layers, castle OBJ, dialogue and shared-script fade transitions.')
