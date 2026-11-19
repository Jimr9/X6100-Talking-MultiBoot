# X6100 Talking MultiBoot

One microSD card for the Xiegu X6100 with four firmware builds on it and a **talking boot menu**, so a blind operator
can choose a build by ear.

Download: [Releases](https://github.com/Jimr9/X6100-Talking-MultiBoot/releases). Instructions:
[blindsea.com/x6100/multiboot](https://www.blindsea.com/x6100/multiboot/).

## What is on the card

| Key | Build | Speech |
|-----|-------|--------|
| F1 | [X6100 Accessibility](https://github.com/Jimr9/X6100-Accessibility-Testing) 1.2.0.1 (OpenX6100 v1.2.0 with speech, updates itself) | yes |
| F2 | [OpenX6X00 Speech Access](https://github.com/Jimr9/OpenX6X00-Speech-Access) 0.3.12.1 (OpenX6X00 v0.3.12 with speech, updates itself) | yes |
| F3 | [OpenX6100](https://github.com/OpenX6100/x6100_gui) v1.2.0, the original | no |
| F4 | [OpenX6X00](https://github.com/Guru-RF/OpenX6X00) v0.3.12, the original | no |
| F5 | The talking menu | it is the speech |

Each build keeps its own settings.

## How it starts

1. Power on. The screen menu (TomekR1's multi-boot menu) shows for 3 seconds, silently. A sighted helper can press F1 to F4.
2. Nobody pressed a key: the talking menu starts, about 10 to 15 seconds after power-on, and says the choices:
   "Boot menu. Press F1 for ... F2 for ... F3 for ... F4 for ...".
3. Press F1 to F4 while it talks. It says "Starting ..." and the radio restarts straight into that build
   (about 40 seconds until the build speaks).
4. No key: after the choices have been said once (30 seconds), F1 starts.
5. F5 says the choices again; the power key says "Power off" and switches the radio off.
6. Holding F1 to F4 while switching on still starts that build directly.

The OpenX6X00 Speech Access build can change all of this from the radio: Settings > Voice > START-UP (which build
starts, how long the menus wait, and "Restart into, once").

## How it works

- `talkmenu.c`: the talking menu, a static program that runs as `/sbin/init` of a tiny system of its own (F5). It
  plays the prompts (`BOOT/TALK/*.WAV`, 48 kHz stereo) with `aplay` from the F2 build's system (mounted read-only),
  turns on the radio board's play mode over I2C, reads the F-keys, writes the choice to the card's sector 2 ("BM" and
  the key's digit) and restarts.
- `talkcard.py`: builds the card with [TomekR1's Xiegu6x00-Multi-Boot](https://github.com/TomekR1/Xiegu6x00-Multi-Boot)
  and adds to its boot script a check of sector 2: a mark there starts that build once and is cleared.
- `BOOT/talkmenu.txt` on the card: `default=` (the key started when no key is pressed), `wait=` (seconds; 0 waits for
  a key), `volume=` (0 to 55).
- `prompts/`: the spoken prompts, made with RHVoice (voice Lyubov) on the radio itself (`tools/rhv_make.sh`,
  `tools/rhv_fetch.py`).

Build: `build.sh` (the talking menu's root filesystem; needs the ARM toolchain of an OpenX6X00 buildroot tree), then
`card.sh NAME F1.zip F2.zip`.

## Licence

GPL-3.0-or-later (it builds on Xiegu6x00-Multi-Boot, GPL-3.0). Each build on the card keeps its own licence; see the
links above for their source code.
