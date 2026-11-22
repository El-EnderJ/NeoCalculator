#!/usr/bin/env python3
"""Join verified mathematical and product originals into one offline review ZIP."""
import argparse
import hashlib
import html
from html.parser import HTMLParser
import json
from pathlib import Path
import zipfile
from PIL import Image

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--product', type=Path, required=True)
p.add_argument('--math', type=Path, help='Mathematical gallery directory (defaults to math-corpus)')
p.add_argument('--native-sha256', required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
gallery = root/'out/math-tex-parity-02/gallery'
product = a.product.resolve()
productRel = product.relative_to(gallery).as_posix()
math = a.math.resolve() if a.math else gallery/'math-corpus'
mathRel = math.relative_to(gallery).as_posix()
assert (product/'index.html').is_file() and (math/'index.html').is_file()
recipeCount = len(json.loads((math/'master-corpus.json').read_text(encoding='utf-8'))['cases'])
allowed = {'.png', '.html', '.json'}
files = [f for folder in (math, product) for f in folder.rglob('*')
         if f.is_file() and f.suffix.lower() in allowed]
manifest = []
for path in sorted(files):
    if path.suffix.lower() == '.png':
        with Image.open(path) as im:
            assert im.size == (320, 240), (path, im.size)
    manifest.append(dict(path=path.relative_to(gallery).as_posix(),
                         bytes=path.stat().st_size,
                         sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
page = '''<!doctype html><html lang="es"><meta charset="utf-8">
<meta name="viewport" content="width=device-width"><title>NumOS · revisión matemática</title>
<style>body{max-width:880px;margin:40px auto;padding:0 20px;font:17px/1.5 system-ui;color:#172b3a;background:#f2f4f6}a{color:#0756a0}section{padding:18px;background:white;margin:18px 0;border:1px solid #cad4dc}code{overflow-wrap:anywhere;font-size:13px}</style>
<h1>NumOS · MATH-TEX-PARITY-02</h1>
<p>Paquete local de revisión. Las capturas conservan los 320×240 píxeles originales. No incluye fuentes ni requiere conexión.</p>
<section><h2>Geometría y referencia</h2><p><a href="'''+html.escape(mathRel)+'''/index.html">'''+str(recipeCount)+''' expresiones en ocho estilos</a>: baseline, candidato, referencia LuaHBTeX con el mismo STIX y cajas geométricas. El selector recorre los ocho estilos; baseline y TeX indican explícitamente los estilos que no capturan.</p></section>
<section><h2>Uso real y edición</h2><p><a href="'''+html.escape(productRel)+'''/index.html">Galería de producto</a>: estados del cursor, plantillas incompletas, unidades, temperaturas, incertidumbre, cuatro locales, Tutor y desplazamiento. Se comparan baseline y candidato.</p></section>
<p>Binario nativo asociado: <code>'''+html.escape(a.native_sha256)+'''</code>.</p>
<p>Las referencias de contorno y las fuentes bitmap tienen redondeos distintos. Radicales, llaves/barras vectoriales y el tamaño mínimo de 8px son decisiones de producto documentadas; este paquete no afirma igualdad píxel a píxel con TeX.</p>
<p><a href="combined-manifest.json">Manifiesto de identidad y SHA-256</a>. La aprobación crítica y los resultados de hardware se documentan por separado.</p></html>'''
(gallery/'index.html').write_text(page, encoding='utf-8')
(gallery/'combined-manifest.json').write_text(json.dumps(dict(
    nativeSha256=a.native_sha256, originalSize=[320,240], files=manifest), indent=2), encoding='utf-8')
files += [gallery/'index.html', gallery/'combined-manifest.json']
class LocalLinks(HTMLParser):
    def handle_starttag(self, tag, attributes):
        for key, value in attributes:
            if key in ('src', 'href') and value and not value.startswith('#'):
                assert '://' not in value and not value.startswith('data:'), value
                linked = (self.path.parent/value.split('#',1)[0]).resolve()
                linked.relative_to(gallery.resolve())
                assert linked.is_file(), (self.path, value)
for path in files:
    if path.suffix.lower() == '.html':
        links = LocalLinks()
        links.path = path
        links.feed(path.read_text(encoding='utf-8'))
archive = gallery/'MATH-TEX-PARITY-02-review.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
    for path in files:
        z.write(path, path.relative_to(gallery).as_posix())
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    assert all(Path(name).suffix.lower() in allowed for name in z.namelist())
print(json.dumps(dict(archive=str(archive), files=len(files), fontFree=True,
                      sha256=hashlib.sha256(archive.read_bytes()).hexdigest())))
