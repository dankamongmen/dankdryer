include <core.scad>

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

translate([10, 20, bearingh / 2]){
    rotate([90, 0, 45]){
        shaft();
    }
}