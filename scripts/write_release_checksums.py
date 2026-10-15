"""Write hashes for final bundles only, keeping logs and staging out of releases."""
import hashlib
from pathlib import Path

directory = Path(__file__).resolve().parents[1] / 'releases'
directory.mkdir(exist_ok=True)
bundles = sorted(p for p in directory.iterdir() if p.suffix in ('.zip', '.flatpak'))
text = ''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in bundles)
(directory / 'SHA256SUMS.txt').write_text(text, encoding='utf-8', newline='\n')
print(text, end='')
