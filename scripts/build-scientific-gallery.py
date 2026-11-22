#!/usr/bin/env python3
"""Build an offline self-contained screenshot gallery with exact original hashes."""
import argparse,base64,hashlib,html,json,struct,zipfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--evidence',type=Path,default=Path('out/scientific-quantities-01/E-final'))
a=p.parse_args();out=a.evidence/'gallery';out.mkdir(parents=True,exist_ok=True)
native=a.evidence/'validation-final-v3/product'
visual=a.evidence/'visual-review-final-v3'
web=a.evidence/'web-final-v3'
roots=[('native',native),('visual-review',visual),('web',web)]
for group,folder in roots:
 assert folder.is_dir(),('Missing final candidate evidence',group,folder)
records=[];sections=[]
for group,folder in roots:
 cards=[]
 for path in sorted(folder.rglob('*.png')):
  data=path.read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n'
  size=struct.unpack('>II',data[16:24]);assert size==(320,240),(path,size)
  label=group+'/'+path.relative_to(folder).as_posix();digest=hashlib.sha256(data).hexdigest()
  records.append({'id':label,'source':path.as_posix(),'width':320,'height':240,'bytes':len(data),'sha256':digest})
  cards.append('<figure><img width="320" height="240" loading="lazy" alt="'+html.escape(label)+'" src="data:image/png;base64,'+base64.b64encode(data).decode()+'"><figcaption>'+html.escape(label)+'</figcaption><small>'+digest[:16]+'</small></figure>')
 sections.append('<section><h2>'+group+'</h2><div class="grid">'+''.join(cards)+'</div></section>')
page='''<!doctype html><html lang="es"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>NumOS · Medidas e incertidumbre</title><style>body{margin:24px;background:#eef1f5;color:#172335;font:16px system-ui,sans-serif}main{max-width:1500px;margin:auto}h1{margin-bottom:8px}p{max-width:900px;line-height:1.5}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:16px}figure{margin:0;padding:12px;background:white;border:1px solid #d2dae5;border-radius:8px}img{display:block;max-width:100%;height:auto;background:white}figcaption{margin-top:10px;overflow-wrap:anywhere;font-size:14px}small{color:#637389}a{color:#145d95}</style><main><h1>NumOS · Medidas e incertidumbre</h1><p>Originales 320 × 240 del candidato validado. Cada imagen está incorporada en este HTML: funciona sin red, sin fuentes distribuidas ni dependencias externas. El manifiesto identifica los originales por SHA-256.</p><p>± expresa incertidumbre estándar (k=1). La vista expandida indica k=2, sin atribuir un nivel de confianza. Los resultados largos se desplazan con ←/→: las capturas correlated-start/end muestran ambos extremos y comprueban el retorno sin alterar el valor canónico.</p>'''+''.join(sections)+'</main></html>'
(out/'index.html').write_text(page,encoding='utf8')
manifest={'schema':1,'originalSize':[320,240],'screenshots':records,'count':len(records),'gallerySha256':hashlib.sha256(page.encode()).hexdigest(),'noFontFiles':True}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n',encoding='utf8')
with zipfile.ZipFile(out/'scientific-quantities-gallery.zip','w',zipfile.ZIP_DEFLATED) as z:
 z.write(out/'index.html','index.html');z.write(out/'manifest.json','manifest.json')
 for r in records:z.write(r['source'],'originals/'+r['id'])
print(json.dumps({'count':len(records),'html':str(out/'index.html'),'zip':str(out/'scientific-quantities-gallery.zip')}))
