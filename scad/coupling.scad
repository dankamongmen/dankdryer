include <core.scad>
use <croom.scad>
//include <croom.scad>
// these are fastened with M4 screws, in order:
//  croom's mount
//  loadcell mount / shield
//  load cell
//  loadcell mount
//  lower coupling

// these are joined by the shaft, and placed into the coupling:
//  bearing
//  gear
//  platform

translate([-25, 0, 0]){
    loadcellmount(loadcellsupph);
}

multicolor("blue"){
    mirror([1, 0, 0]){
        lowercoupling();
    }
}

translate([5, 0, wormlen / 2]){
    translate([20, 0, 0]){
        rotate([90, 0, 0]){
            wormy();
        }
    }
}

translate([0, 0, 4]){
    gear();
}

translate([10, 0, shafth / 2]){
    mirror([0, 0, 1]){
        shaft();
    }
}

//dropmotormount();