include <core.scad>
use <croom.scad>

// these are fastened with M4 screws, in order:
//  croom's mount
//  load cell
//  lower coupling

// these are joined by the shaft, and placed into the coupling:
//  bearing
//  gear
//  platform

// the lower coupling screws into the load cell on
// the floating end. it provides a space to hold the
// bearing above the center of the load cell. the
// bearing will freely rotate within the coupling.
multicolor("purple"){
translate([spoold / 4 + 30, 0, 0]){
    rotate([0, 0, 90]){
        mirror([1, 0, 0]){
            lowercoupling();
        }
    }
}
}

// the worm gear, which is placed onto the rotor.
// it must be tangent to the main gear.
multicolor("white"){
translate([45, -45, wormlen / 2]){
    rotate([90, 0, 0]){
        wormy();
    }
}
}

// the main gear. plugs into the bearing, and is
// placed into the coupling. the gear needs be
// at the same height as the worm gear.
multicolor("lightblue"){
translate([-20, -spoold / 4 - teeth / 2, gearh / 2]){
    gear();
}
}

// the platter upon which the spool rests. plugs
// into the main gear through the center shaft.
multicolor("green"){
translate([10, 0, shafth / 2]){
    mirror([0, 0, 1]){
        shaft();
    }
}
}

/*
multicolor("pink"){
    assembly();
}*/