# Archive browser timing (HC_TIMING=1 writes hc_timing.log)

| | before | after |
|---|---|---|
| UNI2 list (index of d\, 20188 entries) | ~0.5 s | ~0.5 s (index only, no per-entry parse) |
| UNI2 one thumbnail (cold) | 6.9-8.9 s (whole stack unpacked to disk, full load incl. .pat) | 0.16-0.32 s (light stack, sprite bank read from archive in memory) |
| UNI2 28 visible thumbnails, first to last | serial, tens of seconds | 3 workers, ~2.5 s |
| UNI2 second open (disk cache) | same as cold | 28 tiles in 0.15-0.17 s |
| MBTL list (22173 entries) | 0.36 s | 0.14 s |
| MBTL 28 thumbnails cold | n/a | 0.28-1.7 s |
| MBTL second open (cache) | n/a | 0.21-0.23 s |

Cache: %LOCALAPPDATA%\Hantei-chan\thumbcache, keyed by archive path + entry + size + mtime. Raw logs: not committed.
