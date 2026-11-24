import S3f_post as q, s3_post as p, sys
for net in ('nuevo','viejo'):
  for rd in ('Rd','sinRd'):
    try: rows=p.a_rows(f'S3f_ilim_{net}_{rd}.log')
    except Exception as e: print(net,rd,'no listo'); continue
    print('==',net,rd,len(rows))
    for r in rows:
        s=r['s']; ok=p.verdict_a(r)
        print(f"{s['vn']:g}V/{s['cl']*1e6:g}u ILIM{s['ilim']:g}: ipk {q.f(r['ipk'])} ilim {q.f(r['ilim'])} drop {q.f(r['drop'])} pg{r['n_pgf']} re{r['n_reop']}/{r['n_recl']} u {q.f(r['u'])} vfin {q.f(r['vfin'])} {'OK' if all(ok) else 'FALLA'}")
