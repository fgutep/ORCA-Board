import S3f_post as q, s3_post as p, sys
def show(log):
    st,m=p.parse_log(log)
    for i,s in enumerate(st):
        g=lambda k:p._g(m,k,i)
        us=[g(k) for k in ('mxu100u','mxu1m','mxu10m','mxu40m','mxudc')]
        e,pk=g('eq6'),g('ppk'); teq=None if not pk or e is None else e/pk
        u=p.u_at(teq,us) if None not in us else None
        tf=g('tfull'); 
        print(' '.join(f'{k}={v:g}' for k,v in s.items()), '| ipk',q.f(g('ipk')),'drop',q.f(g('drop')),'pgf',g('pgf1'),'vdsc',q.f(g('vdsc')),'vextc',q.f(g('vextc')),'ppre',q.f(g('ppre')),'pstby',q.f(g('pstby')),'ton_ms',q.f(None if tf is None else (tf-s.get('tclose',1e-3))*1e3),'t15_ms',q.f(None if g('t15') is None else g('t15')*1e3),'E_mJ',q.f(None if e is None else e*1e3),'ppk',q.f(pk),'teq_ms',q.f(None if teq is None else teq*1e3),'u',q.f(u),'vfin',q.f(g('vfin')))
show(sys.argv[1])
