// Echo Mini case — snap-together two-part (back tray + front cover)
// For the SNOWSKY (FiiO) Echo Mini DAP.
//
// Every number has a source tag:
//   [REF]      measured from the reference case in reference/ (two STL halves
//              that are known to fit the player)
//   [SPEC]     from FiiO's spec sheet (fiio.com/echomini_parameters)
//   [DESIGN]   a case-design choice, not a device measurement
//   [ESTIMATE] a guess
//
// Coordinates (player face-up, screen toward you, buttons on the top edge):
//   X = length (0 = left end,    cav_w = right end)
//   Y = height (0 = bottom edge, cav_l = top edge, where the buttons are)
//   Z = depth  (0 = back face,   cav_t = front/screen face)
// The origin is the inside corner of the cavity, so all cut-out positions are
// measured from the player itself.
//
// See README.md for how to edit, render and export.

/* ================= WHAT TO RENDER ================= */

// "both"          both halves of this model, laid flat for printing
// "back" / "front" one half of this model
// "assembled"     this model put together (front lifted by `explode`)
// "fit_test"      a 10 mm slice of the back tray
// "reference"     the reference case (reference/*.stl), assembled
// "compare"       this model, with the reference case as a see-through overlay
// "ref_back_lanyard" the reference back half with the lanyard loop added, laid flat
// "ref_front"     the reference front half, laid flat
part = "both";
explode = 10;

/* ================= CAVITY (the space for the player) ================= */
// The reference case's cavity. It fits the player, so these replace the spec size.
// For comparison, the spec sheet says 80 x 54.5 x 14.5 mm [SPEC].

cav_w = 80.35;          // [REF] inside length
cav_l = 55.60;          // [REF] inside height
cav_t = 13.90;          // [REF] inside depth (less than the 14.5 mm spec; the front
                        //   frame overlaps the player's rounded front edge)
cav_r = 1.1;            // [REF] inside corner radius
fit_extra = 0.0;        // [DESIGN] extra gap per side. Raise to 0.1-0.2 if your print is tight.

/* ================= CASE SETTINGS ================= */

wall    = 1.5;          // [DESIGN] side wall. The reference uses 0.95-1.2 on the ends, 2.8-3.1 on the long sides.
floor_t = 1.0;          // [DESIGN] back and front face. The reference uses 0.8 back, 0.5 front.
split_z = 7.2;          // [REF] where the halves meet, measured from the cavity's back face

// Lip that the front cover slides over (part of the back tray)
lip_h   = 3.0;          // [DESIGN]
lip_t   = 0.7;          // [DESIGN] taken from the inner side of the wall
lip_gap = 0.15;         // [DESIGN] clearance between lip and cover

// Snap bumps on the lip's long sides, grooves in the cover
snap_on    = true;
snap_x     = [4, 76.3]; // [DESIGN] bump centres along X, on both long sides.
                        //   Kept clear of the button slots (X 8.1-72.3).
snap_len   = 6;         // [DESIGN]
snap_depth = 0.4;       // [DESIGN] more = tighter click

/* ================= FRONT WINDOWS ================= */
// [ name, centre X, centre Y, width, height, corner radius ]

windows = [
  ["screen", 40.05, 34.40, 58.00, 27.00, 2.0],   // [REF] position and size; radius [DESIGN]
  ["lower",  40.18,  8.49, 64.15, 13.76, 2.0],   // [REF] the lower front panel; radius [DESIGN]
];
window_chamfer = 0.8;   // [DESIGN] bevel on the outside edge of each window

/* ================= CUT-OUTS ================= */
// One row per opening: [ name, face, u, v, w, h, kind ]
//   face: "left" "right" "top" "bottom" (edges) or "front" "back"
//   On an edge:      u = centre along the edge (Y for left/right, X for top/bottom)
//                    v = centre height above the cavity's back face (Z)
//   On front/back:   u = centre X, v = centre Y
//   w, h: opening size along u and v
//   kind: "hole" = plain opening, "button" = opening or flex tab (see button_style)
//
// All rows are [REF]. Buttons are numbered left to right, seen from the front.
// Slot 2 is wider than the others in the reference.

cutouts = [
  // name       face     u      v     w      h     kind
  ["usb_sd",   "left",  24.40, 5.95, 15.00, 8.50, "hole"],    // [REF] left end opening
  ["jacks",    "right", 31.90, 5.70, 22.00, 9.00, "hole"],    // [REF] right end opening (3.5 + 4.4 mm)
  ["button_1", "top",   12.33, 6.70,  8.45, 6.00, "button"],  // [REF]
  ["button_2", "top",   28.75, 6.70, 17.40, 6.00, "button"],  // [REF]
  ["button_3", "top",   44.16, 6.70,  8.38, 6.00, "button"],  // [REF]
  ["button_4", "top",   56.08, 6.70,  8.45, 6.00, "button"],  // [REF]
  ["button_5", "top",   68.03, 6.70,  8.45, 6.00, "button"],  // [REF]
];

cut_r = 1.0;            // [DESIGN] corner radius of every opening

/* ================= BUTTONS ================= */

button_style = "open";  // "open" = slots like the reference
                        // "flex" = hinged tabs with a nub. The button slots cross the
                        //   split line, so for "flex" raise split_z above ~10.5 first.
flex_slot = 0.8;        // [DESIGN] slot width around a flex tab
flex_nub  = 0.6;        // [DESIGN] how far the nub reaches in

/* ================= LANYARD LOOP ================= */

lanyard_on   = true;
lanyard_edge = "right"; // [DESIGN] "left" or "right"
lanyard_y    = 50;      // [DESIGN] centre along the end (Y), clear of the end openings
lanyard_len  = 8;       // [DESIGN] how far it sticks out
lanyard_wid  = 9;       // [DESIGN] width along the end
lanyard_th   = 3.0;     // [DESIGN] thickness, flush with the back
lanyard_hole = 3.2;     // [DESIGN] cord hole diameter

/* ================= REFERENCE FILES ================= */

ref_front = "reference/snowsky_echomini_a_cut.stl";
ref_back  = "reference/snowsky_echomini_b_cut.stl";
// The reference files use their own axes. This moves them into this model's frame:
// X = x + 0.05, Y = 0.9 - z, Z = y + 0.2
ref_matrix = [[1, 0,  0, 0.05],
              [0, 0, -1, 0.90],
              [0, 1,  0, 0.20],
              [0, 0,  0, 1   ]];
ref_back_z  = -0.8;     // [REF] outer back face, in this model's frame
ref_front_z = 14.4;     // [REF] outer front face
ref_right_x = 81.55;    // [REF] outer right end
ref_left_x  = -0.95;    // [REF] outer left end

/* ================= QUALITY ================= */

$fn = 48;
eps = 0.01;

/* ================================================================ */
/* ======================  Model below  =========================== */
/* ================================================================ */

fe   = fit_extra;
in_w = cav_w + 2*fe;
in_l = cav_l + 2*fe;
in_r = cav_r + fe;
out_w = in_w + 2*wall;
out_l = in_l + 2*wall;
out_r = in_r + wall;
z0 = -fe - floor_t;                // outer back surface
z1 = cav_t + fe + floor_t;         // outer front surface

module rrect(x, y, w, l, r) {      // rounded rectangle, lower-left at [x, y]
  r2 = max(eps, min(r, w/2 - eps, l/2 - eps));
  translate([x + r2, y + r2]) offset(r = r2) square([w - 2*r2, l - 2*r2]);
}

module crect(w, h, r) {            // rounded rectangle, centred
  r2 = max(eps, min(r, w/2 - eps, h/2 - eps));
  offset(r = r2) square([w - 2*r2, h - 2*r2], center = true);
}

module outer_body() {
  translate([0, 0, z0]) linear_extrude(z1 - z0)
    rrect(-fe - wall, -fe - wall, out_w, out_l, out_r);
}

module cavity() {
  translate([0, 0, -fe]) linear_extrude(cav_t + 2*fe)
    rrect(-fe, -fe, in_w, in_l, in_r);
}

module ring(t, za, zb) {           // band of thickness t around the cavity
  translate([0, 0, za]) linear_extrude(zb - za)
    difference() {
      rrect(-fe - t, -fe - t, in_w + 2*t, in_l + 2*t, in_r + t);
      rrect(-fe, -fe, in_w, in_l, in_r);
    }
}

// Places a 2D shape (drawn in u/v) on a face and pushes it through the wall
module on_face(face, u, v) {
  d = max(wall, floor_t) + 2;
  if (face == "left")
    translate([-fe - wall - 1, u, v]) rotate([90, 0, 90]) linear_extrude(d) children();
  else if (face == "right")
    translate([cav_w + fe + wall + 1, u, v]) rotate([90, 0, -90]) mirror([1,0,0]) linear_extrude(d) children();
  else if (face == "bottom")
    translate([u, -fe - wall - 1, v]) rotate([-90, 0, 0]) mirror([0,1,0]) linear_extrude(d) children();
  else if (face == "top")
    translate([u, cav_l + fe + wall + 1, v]) rotate([90, 0, 0]) linear_extrude(d) children();
  else if (face == "front")
    translate([u, v, z1 + 1]) mirror([0,0,1]) linear_extrude(d) children();
  else if (face == "back")
    translate([u, v, z0 - 1]) linear_extrude(d) children();
}

module flex_slot_2d(w, h) {        // U-shaped slot, hinge on the -u end
  difference() {
    crect(w + 2*flex_slot, h + 2*flex_slot, cut_r + flex_slot);
    crect(w, h, cut_r);
    translate([-w/2 - flex_slot - 1, -h/2]) square([flex_slot + 1 + eps, h]);
  }
}

function is_edge(f) = f == "left" || f == "right" || f == "top" || f == "bottom";

for (c = cutouts)
  if (c[6] == "button" && button_style == "flex" && is_edge(c[1])
      && c[3] + c[5]/2 + flex_slot > split_z - 0.5)
    echo(str("WARNING: flex button '", c[0], "' crosses split_z; raise split_z or use button_style=\"open\""));

module cutout_cuts() {
  for (c = cutouts) {
    face = c[1]; u = c[2]; v = c[3]; w = c[4]; h = c[5]; kind = c[6];
    if (kind == "button" && button_style == "flex")
      on_face(face, u, v) flex_slot_2d(w, h);
    else
      on_face(face, u, v) crect(w, h, cut_r);
  }
}

module flex_nubs() {
  if (button_style == "flex")
    for (c = cutouts) if (c[6] == "button") {
      face = c[1]; u = c[2]; v = c[3];
      nd = min(c[4], c[5]) * 0.45;
      if (face == "left")   translate([-fe - eps, u, v]) rotate([0, 90, 0]) cylinder(d = nd, h = flex_nub);
      if (face == "right")  translate([cav_w + fe + eps, u, v]) rotate([0, -90, 0]) cylinder(d = nd, h = flex_nub);
      if (face == "bottom") translate([u, -fe - eps, v]) rotate([-90, 0, 0]) cylinder(d = nd, h = flex_nub);
      if (face == "top")    translate([u, cav_l + fe + eps, v]) rotate([90, 0, 0]) cylinder(d = nd, h = flex_nub);
      if (face == "front")  translate([u, v, cav_t + fe + eps]) mirror([0,0,1]) cylinder(d = nd, h = flex_nub);
      if (face == "back")   translate([u, v, -fe - eps]) cylinder(d = nd, h = flex_nub);
    }
}

module front_windows() {
  for (wn = windows) {
    cx = wn[1]; cy = wn[2]; w = wn[3]; h = wn[4]; r = wn[5];
    translate([cx, cy, cav_t]) linear_extrude(fe + floor_t + 1) crect(w, h, r);
    translate([cx, cy, z1 - window_chamfer])
      linear_extrude(window_chamfer + eps, scale = [(w + 2*window_chamfer)/w, (h + 2*window_chamfer)/h])
        crect(w, h, r);
  }
}

module snap_bumps(extra = 0) {
  if (snap_on) {
    zc = split_z + lip_h * 0.55;
    for (x = snap_x) {
      for (side = [0, 1]) {
        y = side == 0 ? -fe - lip_t : cav_l + fe + lip_t;
        translate([x, y, zc]) rotate([0, 90, 0])
          scale([1, (snap_depth + extra) / (lip_h * 0.3), 1])
            cylinder(r = lip_h * 0.3, h = snap_len + 2*extra, center = true);
      }
    }
  }
}

// Lanyard tab. x_edge = outer face of the end wall, zb = outer back face.
module lanyard_loop(x_edge, zb) {
  if (lanyard_on) {
    dir = lanyard_edge == "left" ? -1 : 1;
    translate([0, 0, zb]) linear_extrude(lanyard_th)
      difference() {
        hull() {
          translate([x_edge - dir*1 - (dir > 0 ? eps : 0), lanyard_y - lanyard_wid/2]) square([1 + eps, lanyard_wid]);
          translate([x_edge + dir*(lanyard_len - lanyard_wid/2), lanyard_y]) circle(d = lanyard_wid);
        }
        translate([x_edge + dir*(lanyard_len - lanyard_wid/2), lanyard_y]) circle(d = lanyard_hole);
      }
  }
}

module shell() { difference() { outer_body(); cavity(); } }

module below(z) { intersection() { children(); translate([-100, -100, -100]) cube([300, 300, 100 + z]); } }
module above(z) { intersection() { children(); translate([-100, -100, z]) cube([300, 300, 100]); } }

module back_half() {
  difference() {
    union() {
      below(split_z) shell();
      ring(lip_t, split_z - eps, split_z + lip_h);
      snap_bumps();
      lanyard_loop(lanyard_edge == "left" ? -fe - wall : cav_w + fe + wall, z0);
    }
    cutout_cuts();
  }
  below(split_z) flex_nubs();
}

module front_half() {
  difference() {
    above(split_z) shell();
    ring(lip_t + lip_gap, split_z - 1, split_z + lip_h + lip_gap);
    snap_bumps(lip_gap);
    cutout_cuts();
    front_windows();
  }
  above(split_z + lip_h + lip_gap) flex_nubs();
}

module player_ghost() {
  %translate([0, 0, 0]) linear_extrude(cav_t) rrect(0, 0, cav_w, cav_l, cav_r);
}

module ref_front_part() { multmatrix(ref_matrix) import(ref_front); }
module ref_back_part()  { multmatrix(ref_matrix) import(ref_back); }

module fit_test() {
  intersection() {
    back_half();
    translate([cav_w/2 - 5, -100, -100]) cube([10, 300, 300]);
  }
}

if (part == "back") {
  translate([0, 0, -z0]) back_half();
} else if (part == "front") {
  translate([0, 0, z1]) mirror([0, 0, 1]) front_half();
} else if (part == "both") {
  translate([0, 0, -z0]) back_half();
  translate([0, out_l + 15, z1]) mirror([0, 0, 1]) front_half();
} else if (part == "assembled") {
  player_ghost();
  color("SteelBlue") back_half();
  color("LightSteelBlue", 0.8) translate([0, 0, explode]) front_half();
} else if (part == "fit_test") {
  translate([0, 0, -z0]) fit_test();
} else if (part == "reference") {
  player_ghost();
  color("Tan") ref_back_part();
  color("Wheat") translate([0, 0, explode]) ref_front_part();
} else if (part == "compare") {
  color("SteelBlue") back_half();
  color("LightSteelBlue") front_half();
  %ref_back_part();
  %ref_front_part();
} else if (part == "ref_back_lanyard") {
  translate([0, 0, -ref_back_z]) union() {
    ref_back_part();
    lanyard_loop(lanyard_edge == "left" ? ref_left_x : ref_right_x, ref_back_z);
  }
} else if (part == "ref_front") {
  translate([0, 0, ref_front_z]) mirror([0, 0, 1]) ref_front_part();
}
