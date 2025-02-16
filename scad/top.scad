include <core.scad>
use <threads.scad>

module top(){
    inh = topz;
	toph = 2;
    // fills in the top of the hotbox, having radius
    // innerr. give us a bit of smidge so it's not
    // a tight fit.
    translate([0, 0, toph]){
        difference(){
			ScrewThread(innerr * 2 + 4, inh, 2);
			//threaded_rod(innerr * 2 + 4, inh, 2);
            //cylinder(inh, innerr - 0.5, innerr - 0.5, true);
			translate([0, 0, inh / 2]){
				cylinder(inh, innerr - 2, innerr - 2, true);
			}
        }
    }
    linear_extrude(toph){
        circle(totalxy / 2);
    }
}

multicolor("lightyellow"){
    top();
}
