* soa_S3.inc - SOA del SiRS4401DP (datasheet p.4 'Safe Operating Area', TC=25 C, pulso unico),
* digitalizada por analisis de color del PDF ..\modelos\SiRS4401DP_datasheet.pdf (incertidumbre ~ +-10 %).
* Lineas: limite Rds(on) I=V/4.1m (extraida de la grafica), IDM 350 A, BVDSS 41 V (la tabla satura en V>41.7).
* u_k = |I(Vid)| / I_SOA_k(V_DS): UTILIZACION de cada curva SOA (>1 = fuera de SOA). Solo cuenta con V_DS>0.5 V (region no ohmica).
* Sin derating por temperatura de carcasa (TC=25 C): optimista si la placa calienta. Pulso equivalente: t_eq = E/Ppk (ver resumen.md).
* curva 100 us: (8.466V,347.499A) (9.124V,334.775A) (28.88V,105.318A) (38.666V,60.685A) (41.67V,56.79A)
Bu100u u100u 0 V=(V(vint)-V(vdr)>0.5)*abs(I(Vid))/min(350,min(max(V(vint)-V(vdr),1m)/4.1m,10**table(log10(max(V(vint)-V(vdr),0.5)),0.9277,2.5410,0.9602,2.5248,1.4606,2.0225,1.5873,1.7831,1.6198,1.7543)))
* curva 1 ms: (2.596V,347.499A) (2.756V,340.372A) (4.831V,187.382A) (9.687V,62.731A) (19.137V,9.876A) (31.358V,1.718A) (38.666V,0.892A) (41.67V,0.801A)
Bu1m u1m 0 V=(V(vint)-V(vdr)>0.5)*abs(I(Vid))/min(350,min(max(V(vint)-V(vdr),1m)/4.1m,10**table(log10(max(V(vint)-V(vdr),0.5)),0.4143,2.5410,0.4403,2.5320,0.6840,2.2727,0.9862,1.7975,1.2819,0.9946,1.4963,0.2350,1.5873,-0.0496,1.6198,-0.0964)))
* curva 10 ms: (1.058V,270.984A) (1.192V,248.393A) (4.795V,51.413A) (5.167V,45.401A) (31.124V,0.835A) (38.666V,0.534A) (41.67V,0.497A)
Bu10m u10m 0 V=(V(vint)-V(vdr)>0.5)*abs(I(Vid))/min(350,min(max(V(vint)-V(vdr),1m)/4.1m,10**table(log10(max(V(vint)-V(vdr),0.5)),0.0245,2.4329,0.0763,2.3951,0.6808,1.7111,0.7132,1.6571,1.4931,-0.0783,1.5873,-0.2725,1.6198,-0.3036)))
* curva 40 ms: (0.52V,139.609A) (0.556V,135.056A) (0.904V,73.432A) (10.208V,3.306A) (19.572V,1.334A) (38.666V,0.404A) (41.67V,0.381A)
Bu40m u40m 0 V=(V(vint)-V(vdr)>0.5)*abs(I(Vid))/min(350,min(max(V(vint)-V(vdr),1m)/4.1m,10**table(log10(max(V(vint)-V(vdr),0.5)),-0.2840,2.1449,-0.2549,2.1305,-0.0438,1.8659,1.0089,0.5193,1.2916,0.1252,1.5873,-0.3936,1.6198,-0.4191)))
* curva 100 ms / DC: (0.482V,119.759A) (0.539V,109.321A) (4.94V,6.915A) (18.297V,1.093A) (38.378V,0.322A) (41.67V,0.3A)
Budc udc 0 V=(V(vint)-V(vdr)>0.5)*abs(I(Vid))/min(350,min(max(V(vint)-V(vdr),1m)/4.1m,10**table(log10(max(V(vint)-V(vdr),0.5)),-0.3170,2.0783,-0.2684,2.0387,0.6937,0.8398,1.2624,0.0386,1.5841,-0.4921,1.6198,-0.5229)))
