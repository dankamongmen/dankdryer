include <core.scad>

module top(){
    inh = 3;
    // fills in the top of the hotbox, having radius
    // innerr. give us a bit of smidge so it's not
    // a tight fit.
    translate([0, 0, 3 * inh / 2]){
        cylinder(inh, innerr - 1, innerr - 1, true);
    }
    linear_extrude(inh){
        polygon(ipoints);
    }
}

multicolor("white"){
    top();
}