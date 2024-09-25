include <core.scad>
use <tween.scad>

module top(){
    difference(){
        core2(totalxy / 2, 0);
        translate([0, 0, 7]){
            cylinder(3, totalxy / 2 - 5, totalxy / 2 - 5, true);
        }
    }
}

multicolor("white"){
    top();
}