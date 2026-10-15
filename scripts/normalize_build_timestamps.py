"""Clamp future source timestamps during extraction, never during compilation."""
from pathlib import Path
import os
import time

def normalize(root):
    root=Path(root).resolve()
    now=time.time_ns()
    count=0
    for directory, dirs, files in os.walk(root, followlinks=False):
        dirs[:]=[d for d in dirs if d != '.git' and not (Path(directory)/d).is_symlink()]
        for name in files:
            path=Path(directory)/name
            if path.is_symlink():continue
            info=path.stat()
            if info.st_mtime_ns>now:
                os.utime(path,ns=(info.st_atime_ns,now));count+=1
    return count

if __name__=='__main__':
    print(f'Staging timestamps: {normalize(Path.cwd())} future timestamps corrected')
