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

translate([30, -60, 0]){
    loadcellmount(loadcellsupph);
}

translate([spoold / 4 + 30, 0, 0]){
    rotate([0, 0, 90]){
        mirror([1, 0, 0]){
            lowercoupling();
        }
    }
}

translate([45, -45, wormlen / 2]){
    rotate([90, 0, 0]){
        wormy();
    }
}

translate([-20, -spoold / 4 - 20, gearh / 2]){
    gear();
}

translate([10, 0, shafth / 2]){
    mirror([0, 0, 1]){
        shaft();
    }
}