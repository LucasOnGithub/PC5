# Notice

PC5's own code (`title/`, `tools/`, `tests/`, `art/`, `patches/`) is under the GNU LGPL 2.1
or later. Parts from elsewhere, each under its own licence:

| Part | From | Licence | Where |
| --- | --- | --- | --- |
| prospero-win v0.2.0 (644e32e) | <https://github.com/mpereiraesaa/prospero-win> | LGPL 2.1 or later | fetched by `tools/build.sh`, patched by `patches/prospero-win/` |
| Wine, RADV/Mesa, Mesa Zink, DXVK, the PS5 SDK and boilerplate, Lapy | as prospero-win's `THIRD_PARTY.md` and the app's `SOURCES.txt` list them | their own | fetched by `tools/build.sh` at pinned commits |
| FFmpeg 8.1.3 (1041abd) | <https://github.com/FFmpeg/FFmpeg> | LGPL 2.1 or later (only its LGPL parts are built) | fetched by `tools/build.sh`, built into `winedmo.prx` and `winegstreamer.prx` for games' videos and sound |
| ps5-framegen (6436210) | <https://github.com/Swordpdf/ps5-framegen> | GPL 3.0 or later; its shaders (from AMD's FSR3) MIT | fetched by `tools/build.sh`, built into `libvulkan.prx` for Frame generation |
| VKD3D-Proton 2.14.1 | <https://github.com/HansKristian-Work/vkd3d-proton> | LGPL 2.1 or later | its release's DLLs, unchanged, pinned by SHA-256 |
| Mbed TLS 3.6.7 | <https://github.com/Mbed-TLS/mbedtls> | Apache 2.0 | its release archive, unchanged, pinned by SHA-256 and built into the eboot with `title/pc5_mbedtls_config.h` |
| Mozilla's root certificates | Debian's `ca-certificates` package (`/usr/share/ca-certificates/mozilla`) on the build machine | MPL 2.0 | the app's `pc5/ca-certificates.crt`, unchanged |
| stb_image 2.30 (a7d4c21) | <https://github.com/nothings/stb> | public domain or MIT | `title/third_party/stb_image.h`, unchanged |
| Inter 4.1 | <https://github.com/rsms/inter> | SIL Open Font License 1.1 | `fonts/` (three of its files, unchanged, and `OFL.txt`) |

Blur5 (<https://github.com/bornaradusin/Blur5>), sahin (<https://github.com/7tg/sahin>) and
PS5_MihawkOS (<https://github.com/mihawk-99/PS5_MihawkOS>) were looked at for ideas only.
None of their code is in PC5.

Game covers are not part of PC5. On the console it downloads them, for the games the player
installs, from Steam's store, GOG's catalogue and Wikipedia, and keeps them with each game.
The sample covers in the tests and screenshots are drawn by `tests/make-sample-covers.py`
for made-up games.
