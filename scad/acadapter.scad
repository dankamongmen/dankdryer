include <core.scad>
include <BOSL2/std.scad>

// hole for four-prong rocker switch + receptacle.
// https://www.amazon.com/dp/B0CW2XJ339
// 45.7mm wide, 20mm high. if we can't print
// the bridge, we'll have to reorient it.
// model with fuse is 30mm
//rockerh = 20;
rockerh = 30;
rockerw = 46; // a bit of skoosh viz 45.7
module rockerhole(iech){
	translate([-botinalt - 2, 0, iech / 2 + 25]){
		rotate([90, 0, 0]){
			rotate([0, 180 - theta, 0]){
				difference(){
					cube([croomwall + 2, rockerw, rockerh], true);
					// two thin clips to hold the plug in
					// depth: 1mm height: 1.5mm
					translate([-1, 0, 0]){
						union(){
							translate([0, 0, rockerh / 2 - 0.75]){
								cube([1, rockerw, 1.5], true);
							}
							translate([0, 0, -rockerh / 2 + 0.75]){
								cube([1, rockerw, 1.5], true);
							}
						}
					}
				}
			}
		}
	}
}

//acadapterh = 30; // old model
acadapterh = 22;
acadapterw = 50;
acadapterl = 135;
acmounth = 6;

// the stand without the plug, since our AC
// adapters only have two places to screw
module acadapterstand(){
	translate([acadapterl / 2 - 0.5, -acadapterw / 2 + 7, acmounth / 2]){
		diff(){
			cuboid([8, 6, acmounth], 
					rounding=-1, edges=BOT)
				edge_profile(except=[TOP,BOT])
					mask2d_roundover(r=1);
		}
	}
}

// 60mm wide total
// screw holes are 6mm in from sides, so they start at
// 6mm (through 10mm) and 50mm (through 54mm)
// they have 2mm radius and 3mm height
module acadapterstands(){
	translate([0, acadapterw, 0]){
     	acadapterstand();
        mirror([0, 1, 0]){
			acadapterstand();
			mirror([1, 0, 0]){
				acadapterstand();
            }
        }
		mirror([1, 0, 0]){
			acadapterstand();
		}
    }
}

// hole in the AC adapter stand for screw
module acadapterhole(){
	translate([acadapterl / 2 - 0.5, -acadapterw / 2 + 7, 0]){
		holed = 3.6;
		ScrewThread(holed, acmounth);
	}
}

// screw holes for mounting ac adapter in two corners
module acadapterholes(){
	translate([0, acadapterw, 0]){
		acadapterhole();
		mirror([0, 1, 0]){
			mirror([1, 0, 0]){
				acadapterhole();
			}
		}
	}
}

module acadapter(){
	translate([0, 0, acadapterh / 2]){
		cube([acadapterl, acadapterw, acadapterh], true);
	}
}

/*difference(){
	acadapterstands();
	acadapterholes();
}*/
//acadapter();
