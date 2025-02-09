include <core.scad>

module top(){
    inh = 8;
    // fills in the top of the hotbox, having radius
    // innerr. give us a bit of smidge so it's not
    // a tight fit.
    translate([0, 0, inh]){
        difference(){
			threaded_rod(innerr * 2 + 4, inh, 2);
            //cylinder(inh, innerr - 0.5, innerr - 0.5, true);
            cylinder(inh, innerr - 2, innerr - 2, true);
        }
    }
    linear_extrude(inh / 2){
        circle(totalxy / 2);
    }
}

multicolor("lightyellow"){
    top();
}
