"""The multi-boot card with the talking menu (F5) and a one-time start from sector 2.
Run from ~/Xiegu6x00-Multi-Boot: python3 talkcard.py OUT.img TALKDIR ROOTFS image1 image2 image3 image4"""
import glob
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Xiegu6x00-Multi-Boot"))
from x6100mb import compose as C               # noqa: E402
from x6100mb.__main__ import _load             # noqa: E402
from x6100mb.image import ExtraSystem          # noqa: E402
from x6100mb.writer import Progress, write_image_file   # noqa: E402

out, talkdir, rootfs = sys.argv[1:4]
images = sys.argv[4:8]
NAMES = ["X6100 Access 1.2.0", "OpenX6X00 speech", "OpenX6100 1.2.0 original", "OpenX6X00 0.3.12 original"]
TALK_KEY = 5

ONCE = [
    "# A one-time start (the talking menu, or Settings > Start-up > Restart into): sector 2 holds BM and the key",
    "setenv bm_once",
    "mmc dev 0",
    "if mmc read 0x45200000 2 1; then",
    "    if itest.b *0x45200000 == 0x42; then if itest.b *0x45200001 == 0x4d; then",
] + ["        if itest.b *0x45200002 == 0x%x; then setenv bm_once %d; fi" % (0x30 + k, k) for k in range(1, 6)] + [
    "        mw.b 0x45200000 0 200; mmc write 0x45200000 2 1",
    "    fi; fi",
    "fi",
    'if test -n "${bm_once}"; then setenv bm_choice ${bm_once}; fi',
]

_menu_script = C.menu_script


def menu_script(*a, **k):
    s = _menu_script(*a, **k)
    marker = 'while test -z "${bm_choice}"; do'
    assert s.count(marker) == 1
    return s.replace(marker, "\n".join(ONCE) + "\n" + marker)


C.menu_script = menu_script

_Fat = C.Fat32Builder


class Fat(_Fat):
    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        if k.get("label") == "BOOT":
            for p in sorted(glob.glob(os.path.join(talkdir, "*.WAV"))):
                self.add_file("TALK/" + os.path.basename(p), open(p, "rb").read())
            self.add_file("talkmenu.txt", (
                "# The talking menu (F5): the system started when no key is pressed (1-4),\r\n"
                "# seconds before it starts (0: waits for a key), the radio's volume while it talks (0-55)\r\n"
                "default=1\r\nwait=30\r\nvolume=40\r\nheadphone=47\r\nself=%d\r\n" % TALK_KEY).encode())


C.Fat32Builder = Fat

systems = _load(images)
entries = [{"kind": "sd", "name": n, "sys": s} for n, s in zip(NAMES, systems)]
talk = ExtraSystem("Talking menu", rootfs, systems[1], extra="talk")
entries.append({"kind": "sd", "name": "Talking menu", "sys": talk, "extra": "talk"})
plan = C.compose(entries, default=TALK_KEY - 1, timeout_s=3, data_mode="separate")
open(out + ".boot.cmd", "w").write(plan.script)
last = [-1]


def show(phase, done, total, msg=""):
    pct = done * 100 // max(total, 1)
    if pct // 10 != last[0] or phase == "done":
        last[0] = pct // 10
        print(f"{phase} {pct}%", flush=True)


write_image_file(plan, out, Progress(show))
print(out, plan.size // 2**20, "MB", plan.info)
