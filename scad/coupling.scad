include <core.scad>
use <croom.scad>

// the load cell is fastened to the load cell mount
// using two M5x8 screws. on the other side, the
// lower coupling is fastened to the load cell with
// two M5x6 screws. the motor is placed in the
// coupling, with its terminals through the holes.
// the rotor sheath is placed atop the rotor, where
// it can freely spin. the cupola is inverted and
// pushed down atop the sheath. the platform is then
// inverted and pushed down atop the cupola. these
// should be placed on the motor through the hole
// in the hot chamber, after it's been fastened to
// the cool chamber (the hot chamber will otherwise
// not fit, getting caught on the platform).
multicolor("purple"){
translate([spoold / 4 + 30, 0, 0]){
    lowercoupling();
}
}

multicolor("blue"){
translate([30, -50, rotorh / 2]){
	mirror([0, 0, 1]){
		cupola();
	}
}
}

// sheathes the rotor. the platform rests atop it.
multicolor("lightblue"){
translate([-30, -50, rotorh / 2]){
	rotor();
}
}

multicolor("lightgreen"){
translate([0, 30, platformh]){
	mirror([0, 0, 1]){
		platform(platforminnerr, platformouterd / 2);
	}
}
}

/*
multicolor("pink"){
    assembly();
}*/
