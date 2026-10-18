"""Game data roots for the render-parity corpus (PLAN.md section 2).

Kaggle-style fixed install layout of the development machine. A different
machine regenerates the corpus by editing this table; the corpus files
themselves stay the archive of record.

Every value is a Linux path (this runs under WSL). The generator converts to a
Windows path when it writes a .batch, because the editor takes Windows paths.
"""

# family: how the character is loaded, which decides which scan tool is used
#   ha6      -> ha6_dump.exe (UNI2 / MBTL / UNIB / DFCI / MBAACC txt projects)
#   ha4      -> ha4tool.exe dump (ActCadenza .DAT; pattern list only, no frames)
#   han2     -> RBO / GOF2 (.dt2 / .pac); pattern 0 only, no frame scan
ROOTS = {
    "uni2":   "/mnt/c/dev/hantei-chan/gamedata/uni2",
    "mbtl":   "/mnt/c/dev/hantei-chan/gamedata/mbtl",
    "unib":   "/mnt/c/games/unib",
    "dfci":   "/mnt/c/dev/transmission/dfci+transmission",
    "mbaacc": "/mnt/c/games/mbaacc",
    "mbac":   "/mnt/c/dev/hantei-chan/gamedata/mbac",
    "rbo":    "/mnt/c/games/rbo",
    "gof2":   "/mnt/c/games/gof2_run",
    "gof1":   "/mnt/c/games/gof1/run",
}

# character entry points, per game. .txt projects for the HA6 families; the
# generator skips a case whose file is missing so a partial install still
# produces a corpus.
CHARS = {
    "uni2": [
        "data/chr000/chr000_0.txt", "data/chr016/chr016_0.txt",
        "data/chr019/chr019_0.txt", "data/chr020/chr020_0.txt",
        "data/chr024/chr024_0.txt", "data/chr025/chr025_0.txt",
        "data/chr027/chr027_0.txt",
    ],
    "mbtl": [
        "data/chr000/chr000_0.txt", "data/chr001/chr001.txt",
        "data/chr004/chr004_0.txt", "data/chr011/chr011_0.txt",
    ],
    "unib": [
        "data/Aka_0/Aka_0.txt", "data/Hil_0/Hil_0.txt",
    ],
    "dfci": [
        "data/Ako_0/Ako_0.txt", "data/Kro_s_0/Kro_s_0.txt",
        "data/Asu_0/Asu_0.txt",
    ],
    "mbaacc": [
        "data/akiha.txt", "data/aoko.txt", "data/arc.txt", "data/ciel.txt",
    ],
    "mbac": [
        "02/AKIHA.DAT", "02/AOKO.DAT",
    ],
    "rbo": [],
    "gof2": [],
    "gof1": [],
}

# Section 4.2's stages: every UNI2 and MBTL stage in bg/BgList.txt, 5 DFCI/UNIB
# stages, 3 MBAACC seeds.
BGLISTS = {
    "uni2": "bg/BgList.txt",
    "mbtl": "bg/BgList.txt",
    "dfci": "bg/BgList.txt",
    "unib": "bg/BgList.txt",
}

# 3D stage view (s3d_view) models: one per game that has them.
S3D = {
    "uni2": "bg",
    "mbtl": "bg",
}

# The 2D stage tabs (area C): MBAACC bgmake .dat under bg/, MBAC BGNN.DAT under
# 05/. MBAC .DAT needs the header-based open (the batch `open` sends .dat to the
# stage loader only for bgmake), so it goes through a ui-script.
STAGES_2D = {
    "mbaacc": ("bg", ["bg01", "bg10", "bg18"], ".dat"),
    "mbac":   ("05", ["BG00", "BG01"], ".DAT"),
}
