# Archive

Files from the project's earlier stages (ReChord, before the GILB fork) that nothing builds, runs or
imports any more. They are kept, with their history, for reference. Moved here on 2026-11-25 with
`git mv`, under the same paths they had (`archive/tools/x.py` was `tools/x.py`). Older docs still name
them by their old paths.

| Folder | What it was |
|---|---|
| `firmware/qemu/` | A QEMU harness (Aug 2026): small firmware pieces run on QEMU's generic Cortex-M3 boards (`lm3s6965evb`, `mps2-an385`), since no emulator knows the Echo Mini's RKnanoC: the crash handler's logic, the first firmware entry, an early menu renderer. Replaced by the desktop simulator (`sim/`) and device tests. `docs/HANDOVER.md` §8b |
| `tools/build_qemu_test.sh`, `qemu_echo_read.py`, `render_qemu_fb.py` | The QEMU harness's build, its telemetry reader and its framebuffer-to-PNG renderer |
| `firmware/mb1/` | An old boot hook (`mb1_hook.S`, `mb1_stub.c`) with its built binaries; `tools/build_mb1_img.py` packed it |
| `firmware/drivers/gfx.c`, `gfx.h` | An early 8x8-font drawing layer; the display driver and `ui/` replaced it |
| `firmware/services/test_fs_sdrw.c`, `test_fs_target.c`, `test_fw_update.c` | Device and host test programs for the file system and the updater, with `tools/test_fs_target.py` and `tools/test_fw_update.py`; `make host-tests` runs today's (`tools/test_fs_write.py`, `test_fw_powercut.c`) |
| `tools/ghidra_scripts/` and most of `tools/` | The reverse-engineering kit used to study the stock firmware: Ghidra import, decompiling, matching functions to the Rockchip SDK and across stock versions (`match_*`, `propagate_*`, `chain*_propagate_names`, `cross_version_*`, `changelog_*`), schema and debug checks (`check_*`, `debug_*`), and the old patch builders (`make_v0*_*.py`) |

Second pass, the same day: the SDK-era pipeline and the first module system, which the clean-room
firmware (`firmware/ap.mk`, `firmware/coreb/`) replaced. Today's image builds byte-for-byte the same
without them (the overlays' ABI hash and its CRC aside).

| Folder | What it was |
|---|---|
| `firmware/rockchip/` | The Rockchip RKNanoD SDK sources (from the vendor's Keil project) and their manifests: the "SDK core B with the stock UI" image (`make release`) of the hybrid era. Its two headers today's build still needs moved to `firmware/compat/` (`armcc_compat.h`, `typedef.h`) |
| `Makefile.legacy` | The Makefile's cut parts: `release`, `bb`, `ap`, `pack-full`, `apbb-experimental` (it bricked a device on 2026-08-25), `bb-stub`, `hook-img`, `trial-img`, `entry-img`, `hook-early-img`, `test-dsp`, the codec `.lib` lists. `make release` already failed before the cut (`build/bb/section3_custom.bin` had no rule) |
| `firmware/hook/`, `firmware/bb_stub/` | The boot experiments: our code inside the stock image (hook, trial, entry) and a mailbox-only core B |
| `firmware/rechord_app.c`, `stubs.c`, `fault.c`, `entry_stubs.S`, `firmware.ld`, `bb_trace.*`, `rechord_win.c`, `ipc.h`, `rechord_dsp_core.*`, `test_dsp_core.c`, `INDEX.md` | The hybrid era's glue and its DSP core (core B's EQ now: `firmware/coreb/eq*.c`) |
| `firmware/app/`, `firmware/modules/`, `firmware/dsp/` | The first module system: a main and module registry, a module loader and builder, a DSP chain, each with its tests |
| `firmware/player/` (most of it) | The first player: `player.c`, codec I/O, the minimp3 / dr_flac / WAV adapters, the ring buffer, ReplayGain math, the IPC mock and target, its tests. Today's player is `firmware/player/player_audio.c` (with `player.h`, `codec.h`, `codecs/dr_flac/dr_flac.h` and its license) |
| `firmware/*/*.mk` (but `ap.mk`) | The module fragments the SDK link used; `firmware/ap.mk` lists core A's sources itself |
| `firmware/ui/Makefile`, `settings_schema.c`, `firmware/resource/` | An early standalone UI build, its settings table, a resource API header |
| `tools/make_hook_img.py`, `compile_check.py`, `check_sdk_manifests.py` | The legacy targets' tools |

The firmware tools still in `tools/` are the ones the build uses (`ap_size.py`, `ovl_pack.py`,
`make_fw1.py`, `pack_*.py`, ...) and the ones run by hand (`gilb_pixfont.py`, `gilb_cjkfont.py`,
`make_covers.py`, the firmware unpack and diff tools, `patch-rkdeveloptool-echo-mini.sh`).

Third pass, 2026-11-25: `docs/` (467 files) down to what the README, `handoff.md` or today's code point
at (32 files). The rest moved to `archive/docs/`, under the same paths; the docs still in `docs/` now
name them there.

| Folder | What it was |
|---|---|
| `docs/re/decomp/` (410 files) | The decompiled stock firmware (3.7.0), annotated: the spec the early rewrite was checked against |
| `docs/re/` (the other 13) | Reverse-engineering plans and reports: SDK matching and porting, the dispatch and mailbox guides, the multi-version plan, the RE backlog. Still in `docs/re/`: `refcfw-analysis.md` and `route-b-minimum.md` (the code cites them) |
| `docs/rewrite/` `services`, `player`, `modules`, `integration`, `customization` | Design notes of the first rewrite's parts that were later replaced; the parts the code still cites stay in `docs/rewrite/` |
| `docs/STATUS.md`, `RE-HISTORY.md` | The SDK-splice era's build status and the decompilation phase's story |
| `docs/bootloader-analysis.md`, `dispatch-map.md`, `sdk-reconciliation.md` | Early boot and SDK-header analyses (Aug 2026) |
| `docs/PROPRIETARY-INVENTORY.md`, `MERGE_NOTES_bt.md` | The SDK era's list of binary parts, and the notes from merging the Bluetooth branches |
