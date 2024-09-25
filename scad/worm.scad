include <core.scad>

translate([5, 0, wormlen / 2]){
    rotate([90, 0, 0]){
        wormy();
    }
}

translate([0, 0, 4]){
    gear();
}