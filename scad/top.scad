include <core.scad>
use <tween.scad>

module top(){
    difference(){
        // fills in the top of the hotbox, having radius
        // innerr. give us a bit of smidge so it's not
        // a tight fit.
        core2(innerr - 1, 0);
        /*translate([0, 0, 7]){
            cylinder(3, totalxy / 2 - 5, totalxy / 2 - 5, true);
        }*/
    }
}

multicolor("white"){
    top();
}