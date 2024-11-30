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

// the lower coupling screws into the load cell on
// the floating end. it provides a space to hold the
// bearing above the center of the load cell. the
// bearing will freely rotate within the coupling.
multicolor("purple"){
translate([spoold / 4 + 30, 0, -mlength / 2]){
    rotate([0, 90, ]){
        mirror([1, 0, 0]){
            lowercoupling();
        }
    }
}
}

translate([100, -100, rotorh / 2]){
	cupola();
}

// sheathes the rotor. the platform rests atop it.
translate([-100, -100, rotorh / 2]){
	rotor();
}

/*
multicolor("pink"){
    assembly();
}*/
