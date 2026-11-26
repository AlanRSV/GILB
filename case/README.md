# Echo Mini case

A parametric, snap-together two-part case for the SNOWSKY (FiiO) Echo Mini, written in OpenSCAD.

- **Back tray:** holds the player and has a lip with snap bumps, plus an optional lanyard loop.
- **Front cover:** has the screen window and the lower-panel window. It slides over the lip and clicks onto the bumps.

## Where the numbers come from

`reference/` holds a two-part case that is known to fit the player:

- `snowsky_echomini_a_cut.stl` is the front frame.
- `snowsky_echomini_b_cut.stl` is the back.

Every size and opening in `echo_mini_case.scad` was measured from these two files and is tagged `[REF]`. Case-design choices are tagged `[DESIGN]`.

| What | Value (mm) |
|---|---|
| Cavity (space for the player) | 80.35 × 55.6 × 13.9 |
| Halves meet at | 7.2 above the cavity's back face |
| Top edge | 5 button slots, 6 tall, centred 6.7 above the back face (slot 2 is 17.4 wide, the rest about 8.4) |
| Left end | one opening, 15 × 8.5 (USB-C and microSD) |
| Right end | one opening, 22 × 9 (3.5 mm and 4.4 mm jacks) |
| Front | screen window 58 × 27; lower-panel window 64.15 × 13.76 |

The reference is thinner than this model: 0.95–1.2 mm on the ends, 0.5 mm on the front face. This model defaults to 1.5 mm walls and 1.0 mm faces.

Not copied from the reference:

- its own clips between the halves (this model uses a lip with snap bumps instead);
- the button icons and logo engraved on it;
- a raised block on its left end and a tab on its right end.

## Get OpenSCAD

Install it yourself, either way:

- Download the Windows installer from the official site: <https://openscad.org/downloads.html>
- Or use winget:

```powershell
winget install OpenSCAD.OpenSCAD
```

## Edit, render, export

1. Open `echo_mini_case.scad` in OpenSCAD. Keep it next to the `reference/` folder.
2. Change the numbers at the top. Everything else follows from them.
3. Set `part`:

| `part` | Shows |
|---|---|
| `"both"` | Both halves of this model, laid flat for printing |
| `"back"` / `"front"` | One half |
| `"assembled"` | This model put together, over a grey block the size of the cavity |
| `"fit_test"` | A 10 mm slice of the tray, for a cheap test print |
| `"compare"` | This model with the reference as a see-through overlay. Use it to check that the openings line up. |
| `"reference"` | The reference case on its own |
| `"ref_back_lanyard"` | The reference back half with the lanyard loop added, laid flat |
| `"ref_front"` | The reference front half, laid flat |

4. Press **F5** to preview, then **F6** to render. Check the console for `WARNING` lines.
5. Export with **File → Export → Export as STL**.

From the command line (one STL per half):

```powershell
& "C:\Program Files\OpenSCAD\openscad.exe" -o back.stl -D 'part="back"' echo_mini_case.scad
```

```powershell
& "C:\Program Files\OpenSCAD\openscad.exe" -o front.stl -D 'part="front"' echo_mini_case.scad
```

**Quickest route to a printed case with a lanyard loop:** export `ref_back_lanyard` and `ref_front`. That's the proven reference case with only the loop added.

## Settings worth knowing

| Variable | Default | What it does |
|---|---|---|
| `fit_extra` | 0 | Extra gap around the player, per side. Try 0.1–0.2 if a print comes out tight. |
| `wall` / `floor_t` | 1.5 / 1.0 | Thickness of the walls and of the faces. |
| `lip_gap` | 0.15 | Clearance between the lip and the cover. Raise it if the cover won't close. |
| `snap_depth` | 0.4 | Snap strength. Lower it if the cover is hard to remove. |
| `button_style` | `"open"` | Open slots, like the reference. `"flex"` adds hinged tabs, but the slots cross the split line, so raise `split_z` above about 10.5 first. |
| `lanyard_*` | right end, near the top | Position and size of the loop. Set `lanyard_on = false` to drop it. |

## Printing (service)

- **Best material:** SLS or MJF nylon (PA12). It is strong, the snaps work well, and it needs no supports. Most services offer it.
- **Cheaper:** FDM in PETG. PLA is brittle at thin walls and snaps.
- **Resin:** "tough" resin is fine. Standard resin cracks at the snaps.
- **Orientation:** every part exports lying on its flat outer face. For FDM, ask the service to keep it that way.
- **Test first:** order `fit_test` before the full case, then adjust `fit_extra` and `lip_gap`.
- Export one STL per half. Most services price per part.

## Licence of the reference files

The reference STLs came from outside this repo. Check their licence before publishing them or anything built from them.
