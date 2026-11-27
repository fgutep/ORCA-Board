import re,json
R="C:/Users/Lenovo/Documents/Universidad/2026-20/ORCA-Board/ORCA-Potencia/ORCA-Board/"
out={}
for k,d in [("ctrl","Hardware"),("pot","Hardware-Potencia")]:
    t=open(R+d+"/ORCA-BOARD.kicad_pcb",encoding='utf8').read()
    blocks=re.split(r'\n\t\(footprint "',t)[1:]
    out[k]={}
    for b in blocks:
        lib=b.split('"')[0]
        at=re.search(r'\(at ([-\d.]+) ([-\d.]+)( [-\d.]+)?\)',b)
        mm=re.search(r'\(property "Reference" "([^"]+)"',b)
        if not mm: continue
        ref=mm.group(1)
        val=re.search(r'\(property "Value" "([^"]+)"',b).group(1)
        side='B' if '(layer "B.Cu")' in b[:200] else 'F'
        if re.match(r'(J|U|SW|RV|L|Q|TP|H)',ref):
            out[k][ref]=[float(at.group(1)),float(at.group(2)),val,lib.split(':')[-1][:30],side]
            print(k,ref,at.group(1),at.group(2),side,val,lib.split(':')[-1][:30])
json.dump(out,open('fp.json','w'))
