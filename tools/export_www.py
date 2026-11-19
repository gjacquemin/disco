"""Export the SD interface from the standalone recovery page."""
from pathlib import Path
import re, shutil
root = Path(__file__).resolve().parents[1]
source = root / 'Disco_ESP/data'
out = root / 'www'
out.mkdir(exist_ok=True)
html = (source / 'index.html').read_text()
css = re.search(r'<style>(.*?)</style>', html, re.S).group(1)
js = re.search(r'<script>(.*?)</script>', html, re.S).group(1)
html = re.sub(r'<style>.*?</style>', '<link rel="stylesheet" href="style.css">', html, flags=re.S)
html = re.sub(r'<script>.*?</script>', '<script src="app.js"></script>', html, flags=re.S)
html = html.replace('src="/icon-192.png"', 'src="icon-192.png"')
(out / 'index.html').write_text(html)
(out / 'style.css').write_text(css)
(out / 'app.js').write_text(js)
shutil.copyfile(source / 'icon-192.png', out / 'icon-192.png')
print('www exported: index.html, style.css, app.js, icon-192.png')
