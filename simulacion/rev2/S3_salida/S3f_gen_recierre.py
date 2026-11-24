tmpl = '''S3f recierre: Q6 cerrado con carga (C_carga + 8 A), SW1 abre en TOPEN y recierra GAP despues del COMANDO de apertura (Q6 tarda ~2-8 ms en abrir); C gd 100n + D1N4148 + Rrst={R}, RG 22k
.include S3f_common.inc
.include soa_S3.inc
.options solver=alt method=modtrap
.param CL=470u TCLOSE=1m TOPEN=40m GAP=1m TREC={{TOPEN+GAP}} TEND=140m RLOAD={{VN/8}} TSRCOFF=1e3 CGD=100n RG=22k RQ7=1m CQ7=1p RST={R} RSTG=1G RDON=1 RPRE=1G
.step param VN list 12 25
.step param CL list 100u 470u 1000u
.step param GAP list 1m 5m 20m
Cl vext cl1 {{CL}}
Rcl cl1 0 {{ESRL}}
Rload vext 0 {{RLOAD}}
Vctl ctl 0 PWL(0 0 {{TCLOSE}} 0 {{TCLOSE+1n}} 1 {{TOPEN}} 1 {{TOPEN+1n}} 0 {{TREC}} 0 {{TREC+1n}} 1)
.tran 0 {{TEND}} 0 1u
.meas tran vdsrec find V(vint)-V(vdr) at={{TREC}}
.meas tran vgsrec find V(g6)-V(vint) at={{TREC}}
.meas tran vmrec find V(m)-V(vint) at={{TREC}}
.meas tran vextrec find V(vext) at={{TREC}}
.meas tran ipk max I(Vid) from={{TREC}}
.meas tran ilim max I(Vsns) from={{TREC}}
.meas tran vmin min V(vint) from={{TREC}}
.meas tran drop param (VN-vmin)/VN*100
.meas tran pgf1 when V(pgood)=3.7 fall=1 td={{TREC}}
.meas tran pgf2 when V(pgood)=3.7 fall=2 td={{TREC}}
.meas tran pgf3 when V(pgood)=3.7 fall=3 td={{TREC}}
.meas tran goff1 when V(g6)-V(vint)=-2.5 rise=1 td={{TREC}}
.meas tran goff2 when V(g6)-V(vint)=-2.5 rise=2 td={{TREC}}
.meas tran gon1 when V(g6)-V(vint)=-2.5 fall=1 td={{TREC}}
.meas tran gon2 when V(g6)-V(vint)=-2.5 fall=2 td={{TREC}}
.meas tran tfull when V(vint)-V(vdr)=0.5 fall=1 td={{TREC}}
.meas tran ppk max (V(vint)-V(vdr))*I(Vid) from={{TREC}}
.meas tran eq6 integ (V(vint)-V(vdr))*I(Vid) from={{TREC}}
.meas tran teq param eq6/ppk
.meas tran mxu100u max V(u100u) from={{TREC}}
.meas tran mxu1m max V(u1m) from={{TREC}}
.meas tran mxu10m max V(u10m) from={{TREC}}
.meas tran mxu40m max V(u40m) from={{TREC}}
.meas tran mxudc max V(udc) from={{TREC}}
.meas tran vfin avg V(vint) from={{TEND}}-10m to={{TEND}}
.meas tran vgsfin avg V(g6)-V(vint) from={{TEND}}-10m to={{TEND}}
.end
'''
for tag,R in (('1G','1G'),('10k','10k'),('47k','47k'),('100k','100k'),('330k','330k')):
    open(f'S3f_recierre_R{tag}.cir','w',encoding='utf-8').write(tmpl.format(R=R))
