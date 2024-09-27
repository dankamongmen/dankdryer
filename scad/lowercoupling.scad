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
//  lower spacer
//  gear
//  upper spacer
//  platform

// the lower mount has the shield attached to it
translate([-50, 0, 0]){
    lowersupport();
}

// the upper mount is printed separately from the coupling
// to simplify printing; we already need a screw there (to
// fasten to the load cell), so no loss.
translate([-25, 0, 0]){
    loadcellmount(loadcellsupph);
}

multicolor("blue"){
    mirror([1, 0, 0]){
        lowercoupling();
    }
}

translate([50, 0, 4 / 2]){
    shaftsupport(4);
}

//dropmotormount();