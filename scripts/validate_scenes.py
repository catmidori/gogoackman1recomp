"""Private savestate regression: candidate only; originals are never overwritten.
Usage: python scripts/validate_scenes.py --exe build-validation/AckmanRecomp.exe
       --rom path.sfc --saves build-ninja/saves --baseline private/scene-audit
Requires the 51 recorded fixtures and their frame-10 dumps in the baseline.
"""
from pathlib import Path
import argparse, concurrent.futures, csv, ctypes, json, os, re, shutil, subprocess, time
ap=argparse.ArgumentParser()
ap.add_argument('--exe',type=Path,required=True);ap.add_argument('--rom',type=Path,required=True)
ap.add_argument('--saves',type=Path,required=True);ap.add_argument('--baseline',type=Path,required=True)
ap.add_argument('--out',type=Path,default=Path('private/scene-validation'))
a=ap.parse_args();root=Path(__file__).resolve().parents[1];out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
records=json.loads((a.baseline/'manifest.json').read_text(encoding='utf8'))
config=(root/'config-debug.ini').read_text().replace('DisableFrameDelay = 0','DisableFrameDelay = 1')
class Memory(ctypes.Structure):
 _fields_=[('cb',ctypes.c_ulong),('faults',ctypes.c_ulong)]+[(n,ctypes.c_size_t) for n in ['peak_work','work','peak_pool','pool','peak_nonpool','nonpool','page','peak_page','private']]
getmem=ctypes.windll.psapi.GetProcessMemoryInfo
getmem.argtypes=[ctypes.c_void_p,ctypes.POINTER(Memory),ctypes.c_ulong]
def image(p):
 _,dim,_,data=p.read_bytes().split(b'\n',3);w,h=map(int,dim.split());assert len(data)==w*h*3;return w,h,data
def batch(worker):
 d=out/f'worker{worker}';d.mkdir(exist_ok=True);(d/'saves').mkdir(exist_ok=True)
 shutil.copy2(a.exe,d/'AckmanRecomp.exe');(d/'config.ini').write_text(config)
 shutil.copytree(root/'assets/fonts',d/'assets/fonts',dirs_exist_ok=True)
 (d/'replay.txt').write_text('wait 1\nloadstate 0\nwait 1000\n')
 results=[]
 for r in records[worker::4]:
  idx=r['id'];prefix=out/f'{idx:02d}';base=a.baseline/f'{idx:02d}'
  shutil.copyfile(a.saves/r['save'],d/'saves/GGA1_save_01.sav')
  env=os.environ.copy();env.update(SDL_VIDEODRIVER='dummy',SDL_AUDIO_DRIVER='dummy',SDL_RENDER_DRIVER='software',SNESRECOMP_RUN_FRAMES='1000',ACKMAN_DUMP_FRAME='10',ACKMAN_DUMP_PREFIX=str(prefix),ACKMAN_TIMING_TRACE=str(prefix)+'.timing.csv',SNESRECOMP_SCREENSHOT=str(prefix)+'.ppm',SNESRECOMP_SCREENSHOT_FRAME='10')
  samples=[];start=time.monotonic()
  with Path(str(prefix)+'.log').open('w') as f:
   p=subprocess.Popen([str(d/'AckmanRecomp.exe'),'--script',str(d/'replay.txt'),'--no-launcher',str(a.rom.resolve())],cwd=d,env=env,stdout=f,stderr=f)
   while p.poll() is None:
    if time.monotonic()-start>60:p.kill();raise AssertionError(f'timeout {idx}')
    m=Memory();m.cb=ctypes.sizeof(m)
    if getmem(int(p._handle),ctypes.byref(m),m.cb):samples.append(m.private)
    time.sleep(.05)
  log=Path(str(prefix)+'.log').read_text(errors='replace')
  assert p.returncode==0 and 'RUN_FRAMES reached after 1000 frames' in log,(idx,p.returncode)
  assert not re.search(r'SEH CRASH|slice watchdog|CPU-port guest-clock sync timed out|load refused',log),idx
  for ext in ['wram','vram','oam','spc-ram']:
   assert Path(str(base)+'.'+ext).read_bytes()==Path(str(prefix)+'.'+ext).read_bytes(),(idx,ext)
  w,h,pix=image(Path(str(prefix)+'.ppm'));bw,bh,old=image(Path(str(base)+'.ppm'))
  assert (w,h)==(384,224)==(bw,bh)
  if idx!=37:assert all(pix[y*w*3+192:y*w*3+960]==old[y*w*3+192:y*w*3+960] for y in range(h)),('center',idx)
  if idx==37:
   # The previous wide road corrupted central pixels; use native 4:3 as oracle.
   native=out/'road-native.ppm'
   native_env=env.copy();native_env.update(ACKMAN_WIDE_EXPERIMENTAL='0',SNESRECOMP_RUN_FRAMES='12',SNESRECOMP_SCREENSHOT=str(native))
   for name in ['ACKMAN_DUMP_FRAME','ACKMAN_DUMP_PREFIX','ACKMAN_TIMING_TRACE']:native_env.pop(name,None)
   with (out/'road-native.log').open('w') as f:
    q=subprocess.run([str(d/'AckmanRecomp.exe'),'--script',str(d/'replay.txt'),'--no-launcher',str(a.rom.resolve())],cwd=d,env=native_env,stdout=f,stderr=f,timeout=30)
   assert q.returncode==0
   nw,nh,np=image(native);assert (nw,nh)==(256,224)
   assert all(pix[y*w*3+192:y*w*3+960]==np[y*768:(y+1)*768] for y in range(h)),('native road',idx)
  timing=list(csv.DictReader(Path(str(prefix)+'.timing.csv').open()))
  assert len(timing)==1000
  # Skip the state-load discontinuity. CPU instruction overrun is preserved.
  assert all(int(y['vblank_edge'])-int(x['vblank_edge'])==357368 for x,y in zip(timing[1:],timing[2:])),('cadence',idx)
  # Exact outside-map black margins for left-edge saves and fixed-aspect scenes.
  left=64 if idx in [1,3,11,15,17,23,27,39,44] else 0
  right=64 if idx in [1,17] else (63 if idx in [27,36] else 0)
  assert all(not any(pix[y*w*3:y*w*3+left*3]) and not any(pix[(y+1)*w*3-right*3:(y+1)*w*3]) for y in range(h)),('mask',idx)
  tail=samples[len(samples)*3//5:];growth=max(0,max(tail)-tail[0]) if tail else 0
  Path(str(prefix)+".memory.json").write_text(json.dumps(samples))
  assert samples and growth<16*1024*1024,('memory growth',idx,growth)
  results.append(dict(id=idx,save=r['save'],frames=1000,peak_private=max(samples),tail_growth=growth,seconds=round(time.monotonic()-start,2)))
 return results
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:result=sum(pool.map(batch,range(4)),[])
result.sort(key=lambda r:r['id']);(out/'results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
print(f'PASS: {len(result)} saves, {len(result)*1000} frames; RAM/VRAM/OAM/SPC and captured centers/masks checked.')
print('Memory check bounds late-run private-byte variation; it is not a leak sanitizer.')
