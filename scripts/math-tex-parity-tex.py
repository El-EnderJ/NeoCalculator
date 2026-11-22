#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""LuaHBTeX/unicode-math independent STIX geometry and PNG references.

TeX/PDF intermediates stay in an isolated ASCII cache. Only JSON/PNG/logs
are delivered; neither font files nor PDFs containing embedded fonts are copied.
"""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
TEX={
'A-negative':r'-3','A-relation-negative':r'x=-3','A-sum':r'a+b-c','A-product':r'a\times b','A-implicit':r'2x','A-factors':r'(2x+5)(x-1)','A-scientific-fraction':r'\frac{66.63}{2\times10^{-3}}',
'B-square':r'x^2','B-negative':r'x^{-2}','B-sum':r'x^{a+b}','B-nested':r'x^{y^2}','B-subscript':r'x_{i+1}','B-subsup':r'{x_i}^2','B-supsub':r'{x^2}_i','B-exp':r'e^{x^2+1}','B-small':r'10^{-12}','B-unit':r'\mathrm{s}^{-2}',
'C-half':r'\frac12','C-sums':r'\frac{a+b}{c+d}','C-nested':r'\frac{\frac1x}{\frac1y}','C-continued':r'\frac1{1+\frac1x}','C-exponent':r'x^{\frac12}','C-powered':r'{\frac{a+b}{c+d}}^2','C-root':r'\sqrt{\frac{a+b}{c+d}}',
'D-simple':r'\sqrt{x}','D-square':r'\sqrt{x^2+1}','D-fraction':r'\sqrt{\frac{a+b}{c+d}}','D-indexed':r'\sqrt[3]{x}','D-nested':r'\sqrt{2+\sqrt{2+\sqrt2}}','D-index-expression':r'\sqrt[n+1]{x}',
'E-nested':r'\left(\left[\left(\frac{a+b}{c+d}\right)\right]\right)',
'F-log-base':r'\log_2\left(x^2+1\right)','F-hyperbolic':r'\sinh(x)',
'G-quadratic':r'\frac{-b\pm\sqrt{b^2-4ac}}{2a}','G-discriminant':r'b^2-4ac','G-matrix':r'\begin{bmatrix}1&x\\\frac12&\sqrt2\end{bmatrix}','G-piecewise':r'\begin{cases}x^2&x\ge0\\-x&\text{otherwise}\end{cases}',
'G-list':r'\left[\frac12,\sqrt2,x^2\right]','G-set':r'\{1,2,x\}','G-interval':r'[0,+\infty)','G-equation':r'x=\frac12','G-unevaluated':r'\operatorname{unevaluated}(x^2+1)',
'H-periodic':r'x=\frac\pi6+2\pi k','H-domain':r'k\in\mathbb Z','H-decimal':r'0.1\overline6','H-degrees':r'\sin(30\mathrm{^{\circ}})',
'I-integral':r'\int_0^1 x^2\,\mathrm{d}x','I-sum':r'\sum_{k=1}^n k^2',
'J-acceleration':r'9.81\,\frac{\mathrm m}{\mathrm s^2}','J-force':r'\frac{\mathrm{kg}\cdot\mathrm m}{\mathrm s^2}','J-kohm':r'\mathrm{k\Omega}','J-microfarad':r'\mathrm{\mu F}','J-square-cm':r'\mathrm{cm}^2','J-cube-m':r'\mathrm m^3','J-celsius':r'{}^{\circ}\mathrm C','J-fahrenheit-difference':r'\Delta{}^{\circ}\mathrm F',
'K-measurement':r'(10\pm1)\,\mathrm m','K-temperature':r'(20.0\,{}^{\circ}\mathrm C\pm0.5\,\Delta{}^{\circ}\mathrm C)','K-long':r'(6.67430\pm0.00015)\times10^{-11}','K-relative':r'u_r','K-expanded':r'U\mathord{,}k=2','K-codata':r'G','K-correlated':r'a_0\times m_e',
'L-empty':r'\square','L-fraction':r'\frac\square\square','L-power':r'\square^{\square}','L-subscript':r'\square_{\square}','L-root':r'\sqrt\square','L-indexed-root':r'\sqrt[\square]\square','L-function':r'\sin(\square)','L-logbase':r'\log_\square(\square)','L-paren':r'(\square)','L-integral':r'\int_\square^\square\square\,\mathrm d\square','L-sum':r'\sum_\square^\square\square','L-bigop':r'\prod_\square^\square\square','L-measurement':r'(\square\pm\square)',
'N-alternating':r'\frac{x^2}{\sqrt{1+\frac1{y^{-2}}}}','N-units-fraction':r'\frac{\frac{\mathrm m}{\mathrm s^2}}{\frac{\mathrm{kg}\cdot\mathrm m}{\mathrm s^2}}','N-fraction-base-script':r'{\frac{\frac1x}{\frac1y}}^2','N-large-base-subscript':r'{\frac{\frac1x}{\frac1y}}_i','N-native-variable':r'x^2',
}
for k,lr in dict(paren=('(',')'),bracket=('[',']'),brace=(r'\{',r'\}'),bar=('|','|')).items():
 TEX['E-'+k]=r'\left'+lr[0]+'x'+r'\right'+lr[1];TEX['E-'+k+'-tall']=r'\left'+lr[0]+r'\frac{\frac{a+b}{c+d}}{\frac1x}\right'+lr[1]
for k in ['sin','cos','tan','arcsin','arccos','arctan','ln','log']:TEX['F-'+k]='\\'+k+r'(x^2)'
TEX['F-factorial']=r'(x^2)!'
for k,cmd in dict(product='prod',coproduct='coprod',intersection='bigcap',union='bigcup',logicaland='bigwedge',logicalor='bigvee').items():TEX['I-'+k]='\\'+cmd+r'_{i=1}^n x_i'
for depth in range(2,7):
 t='x'
 for _ in range(depth):t='x^{'+t+'}'
 TEX['N-powers-'+str(depth)]=t
for depth in range(2,6):
 t='x'
 for _ in range(depth):t=r'\frac1{1+'+t+'}'
 TEX['N-fractions-'+str(depth)]=t
TEX['M-wide']='+'.join(['x']*70);TEX['M-wide-tall']=r'\frac{'+ '+'.join(['x']*35)+'}{'+ '+'.join(['x']*35)+'}'
for base,script in [('f','x'),('V','A'),('A','V'),('f','f'),('x','j'),('T','i')]:
 TEX['B-kern-'+base+'-'+script]=base+'^{'+script+'}';TEX['B-kern-sub-'+base+'-'+script]=base+'_{'+script+'}'
TEX['B-subsup']=r'x_i^2';TEX['K-reference-power']=r'm_{\symup e}^2';TEX['B-combined-tall']=r'x_{\frac12}^{\frac34}'
TEX['A-unscripted-italic']='fx'
TEX['F-factorial-adjacency']='2x!y'
TEX.update({
'K-measurement-ratio':r'\frac{\left(10\pm1\right)}{\left(2\pm0.1\right)}',
'K-measurement-root':r'\sqrt{\left(100\pm4\right)}\,\mathrm m',
'K-measurement-numerator':r'\frac{\left(6.67430\pm0.00015\right)}{r^2}',
'K-measurement-denominator':r'\frac{m_{\symup e}}{\left(2.0\pm0.1\right)}',
'K-measurement-reference':r'\frac{\left(m_{\symup e}\pm u\right)}{\left(a_0\pm v\right)}',
'K-measurement-root-ratio':r'\sqrt{\frac{\left(10\pm1\right)}{\left(2\pm0.1\right)}}',
'K-measurement-frac-root':r'\frac1{\sqrt{\left(100\pm4\right)}}',
'K-measurement-tall':r'\frac{\left(\frac1x\pm\sqrt2\right)}{\left(3\pm\frac1y\right)}',
'K-measurement-script':r'x^{\frac{\left(10\pm1\right)}{\left(2\pm0.1\right)}}',
'L-measurement-root-empty':r'\sqrt{\left(\square\pm1\right)}',
'L-measurement-ratio-empty':r'\frac{\left(10\pm\square\right)}{\left(\square\pm1\right)}',
'K-measurement-script-root':r'2^{\sqrt{\left(100\pm4\right)}}',
'L-measurement-script-root-empty':r'2^{\sqrt{\left(\square\pm4\right)}}',
'K-measurement-reciprocal-ratio':r'\frac1{\frac1{\left(2\pm0.1\right)}}',
})
LUA=r'''
local out=io.open("tex-geometry.jsonl","w")
local function esc(s) return '"'..s:gsub('\\','\\\\'):gsub('"','\\"')..'"' end
local function sp(x) return (x or 0)/65536 end
local function walk(head,x,y,vertical,items,container)
 for n in node.traverse(head) do
  local kind=node.type(n.id)
  if kind=="glyph" then
   local f=font.getfont(n.font); local g=f.characters[n.char] or {}
   items[#items+1]=string.format('{"kind":"glyph","cp":%d,"x":%.8f,"baseline":%.8f,"width":%.8f,"height":%.8f,"depth":%.8f,"fontSize":%.8f}',n.char,sp(x+(n.xoffset or 0)),sp(y-(n.yoffset or 0)),sp(n.width),sp(n.height),sp(n.depth),sp(f.size))
   x=x+n.width
  elseif kind=="hlist" or kind=="vlist" then
   local bx,by=x,y
   if vertical then by=y+n.height;bx=x+(n.shift or 0) else by=y+(n.shift or 0) end
   walk(n.head,bx,kind=="vlist" and by-n.height or by,kind=="vlist",items,n)
   if vertical then y=y+n.height+n.depth else x=x+n.width end
  elseif kind=="rule" then
   local rw=n.width<0 and container.width or n.width
   items[#items+1]=string.format('{"kind":"rule","x":%.8f,"baseline":%.8f,"width":%.8f,"height":%.8f,"depth":%.8f}',sp(x),sp(vertical and y+n.height or y),sp(rw),sp(n.height),sp(n.depth))
   if vertical then y=y+n.height+n.depth else x=x+n.width end
  elseif kind=="kern" then if vertical then y=y+n.kern else x=x+n.kern end
  elseif kind=="glue" then
   local g=n.width
   if container.glue_sign==1 and n.stretch_order==container.glue_order then g=g+n.stretch*container.glue_set end
   if container.glue_sign==2 and n.shrink_order==container.glue_order then g=g-n.shrink*container.glue_set end
   if vertical then y=y+g else x=x+g end
  end
 end
end
function record(id,style)
 local b=tex.box[0];local items={};walk(b.head,0,0,false,items,b)
 out:write(string.format('{"id":%s,"style":%d,"width":%.8f,"ascent":%.8f,"descent":%.8f,"nodes":[%s]}\n',esc(id),style,sp(b.width),sp(b.height),sp(b.depth),table.concat(items,',')))
 out:flush()
end
'''
def main():
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--work',type=Path,default=Path('C:/.codex-cache/math-tex-parity-02/tex-reference'));a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True);a.work.mkdir(parents=True,exist_ok=True)
 import ctypes
 b=ctypes.create_unicode_buffer(32768);ctypes.windll.kernel32.GetShortPathNameW(str(ROOT/'assets/fonts'),b,len(b));fontdir=os.path.relpath(b.value,a.work).replace('\\','/')+'/'
 # A local junction avoids TeX interpreting the '~' in Windows short paths.
 # It references the licensed input in place; no font bytes enter evidence.
 if os.name=='nt' and not (a.work/'font-source').exists():
  quoted=lambda path:"'"+str(path.resolve()).replace("'","''")+"'"
  cmd='New-Item -ItemType Junction -Path '+quoted(a.work/'font-source')+' -Target '+quoted(ROOT/'assets/fonts')+' | Out-Null'
  subprocess.run(['powershell','-NoProfile','-NonInteractive','-Command',cmd],check=True,capture_output=True)
 if (a.work/'font-source').is_dir():fontdir='font-source/'
 # Explicit math font sizes are a product comparison: the TeX engine still
 # executes its own mlist-to-hlist OpenType rules. Font has no external download.
 pre=r'''\documentclass{article}
\usepackage{amsmath,unicode-math}
\setmathfont[Path={FONTDIR}]{STIXTwoMath-Regular.ttf}
\setmainfont[Path={FONTDIR}]{STIXTwoMath-Regular.ttf}
\providecommand{\square}{\mathord{\char"25A1}}
\DeclareMathSizes{18}{18}{12}{8}
\pagestyle{empty}
\pagewidth=320pt
\pageheight=240pt
\hoffset=-1in
\voffset=-1in
\directlua{dofile("oracle.lua")}
\begin{document}
\fontsize{18}{18}\selectfont
'''.replace('FONTDIR',fontdir)
 pages=[];body=[]
 for id,tex in TEX.items():
  for style,cmd in [(0,r'\displaystyle'),(1,r'\textstyle')]:
   body.append(r'\setbox0=\hbox{$'+cmd+' '+tex+r'$}'+f'\n\\directlua{{record("{id}",{style})}}\n'+r'\dimen0=120pt\advance\dimen0 by-\ht0\shipout\vbox to240pt{\vskip\dimen0\hbox to320pt{\kern8pt\box0\hfil}\vfil}'+'\n')
   pages.append(dict(id=id,style=style,tex=tex))
 (a.work/'oracle.lua').write_text(LUA,encoding='utf-8');(a.work/'reference.tex').write_text(pre+''.join(body).replace(r'\mathrm',r'\symup').replace(r'\square',r'{\char"25A1}')+r'\end{document}',encoding='utf-8')
 cmd=['C:/Program Files/MiKTeX/miktex/bin/x64/lualatex.exe','--disable-installer','-interaction=nonstopmode','-halt-on-error','reference.tex'];r=subprocess.run(cmd,cwd=a.work,capture_output=True);(a.out/'tex-build.log').write_bytes(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError((r.stdout+r.stderr).decode(errors='replace')[-4000:])
 import fitz
 doc=fitz.open(a.work/'reference.pdf');assert len(doc)==len(pages),(len(doc),len(pages))
 for page,meta in zip(doc,pages):
  pix=page.get_pixmap(matrix=fitz.Matrix(72.27/72,72.27/72),alpha=False);pix.save(str(a.out/(meta['id']+'-s'+str(meta['style'])+'.png')))
 (a.out/'tex-geometry.jsonl').write_bytes((a.work/'tex-geometry.jsonl').read_bytes())
 version=subprocess.run([cmd[0],'--version'],capture_output=True).stdout.decode(errors='replace')
 (a.out/'tex-reference-version.json').write_text(json.dumps(dict(engine=version,command=cmd,font='STIX Two Math 2.12 b168a',fontSizesPt=[18,12,8],rasterDpi=72.27,pages=pages,caveats=['TeX outline rasterizer differs from LVGL bitmap rasterizer.','TeX uses design glyph metrics and NumOS uses compiled bitmap advances. Compare normalized geometry; no exact pixel claim.','NumOS vector radicals and editor placeholders are deliberate presentation differences.','Semantic identity remains NumOS-owned; reference renders notation only.']),ensure_ascii=False,indent=2),encoding='utf-8')
 print('PASS LuaHBTeX references',len(pages))
if __name__=='__main__':main()
