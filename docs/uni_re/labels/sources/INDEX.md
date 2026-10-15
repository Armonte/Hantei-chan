# EF/IF label sources (fetched 2026-10-15)

| Source | URL | Saved as | EF/IF content |
|---|---|---|---|
| Mizuumi FPANModding, Hantei-chan Effects | https://mizuumi.wiki/w/User:Pixloen/FPANModding/Hanteichan/Effects | mizuumi/Hanteichan_Effects.wiki | EF 1, 4, 6 (No. 1, 2, 5, 6, 8, 12, 111, 255), 8, 9, 14 with params |
| Mizuumi FPANModding, Hantei-chan usage | https://mizuumi.wiki/w/User:Pixloen/FPANModding/Hanteichan | mizuumi/Hanteichan.wiki | IF 56 (vacuum: speed, range) |
| Mizuumi FPANModding, HA6 | https://mizuumi.wiki/w/User:Pixloen/FPANModding/HA6 | mizuumi/HA6.wiki | EF 1/6/8/9 one-liners, IF 56 |
| Mizuumi FPANModding, HA6Ref | https://mizuumi.wiki/w/User:Pixloen/FPANModding/HA6Ref | mizuumi/HA6Ref.wiki | EF6 255 calls the UNI win screen; pattern presets |
| Mizuumi FPANModding, other 26 subpages | https://mizuumi.wiki/w/User:Pixloen/FPANModding/* | mizuumi/*.wiki | no EF/IF tables (files, scripts, unpacking, audio, stages) |

All pages were pulled as wikitext through the MediaWiki API (`https://mizuumi.wiki/api.php?action=parse&prop=wikitext`);
each file starts with its source URL. The wiki pages give no Japanese text and no AT field tables.
No other community source (GitHub, pastebins, Google docs) documenting UNI/MBTL EF/IF numbers was found in this pass;
wiki.gbl.gg links on these pages point to the same Mizuumi content.

Ground truth used for verification: the decompiled handlers saved in `../ida/uni2/` and `../ida/mbtl/`.
