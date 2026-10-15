| id | residual error | roster | roster_v2 | roster_v3 |
|---|---|---|---|---|
| M1 | parts-frame boxes wrong | 1159 / 1159 | 0 / 1159 | 0 / 1159 |
| -- | CG-frame boxes wrong (control) | 63 / 92450 | 63 / 92450 | 63 / 92450 |
| M2 | parts-frame EF positions wrong | 90 / 90 | 0 / 90 | 0 / 90 |
| M2/M9 | CG-frame EF positions wrong | 192 / 5857 | 0 / 5857 | 0 / 5857 |
| M3 | ATVV[2] non-zero (stun leaked) | 1362 / 4556 | 0 / 4556 | 0 / 4556 |
| M7 | missing 900-908/351/354 | 264 / 24 | 0 / 24 | 0 / 24 |
| M8 | IF51 p0 == -1 | 69 / 222 | 0 / 222 | 0 / 222 |
| M8 | IF51 p2 not a pattern | 3 / 222 | 0 / 222 | 0 / 222 |
| M12 | EF1/8/11 p2 bits >= 0x100 | 12 / 4230 | 0 / 4230 | 0 / 4230 |
| G1 | Guard != 1.03A CT f1..f4 | 24 / 24 | 24 / 24 | 0 / 24 |
| G2 | giant reaction slots missing | 15 / 19 | 15 / 19 | 0 / 19 |
| L1 | EF1 leaks into MBAACC-only slots | 17 / 2193 | 17 / 2193 | 0 / 2193 |
| S1 | EX window != 6/6/8 without sidecar | 22 / 23 | 22 / 23 | 0 / 23 |
# roster: chars=26 missing_char=2 pattern_shape_mismatch=0
# roster_v2: chars=26 missing_char=2 pattern_shape_mismatch=0
# roster_v3: chars=26 missing_char=2 pattern_shape_mismatch=0
