// chamber for a high-temperature (140C) filament dryer
// intended to be printed in polycarbonate + PAHT-CF
// the top is lifted up and out to insert a spool
// the spool sits on corner walls
include <core.scad>
include <BOSL2/std.scad>
include <BOSL2/screws.scad>

// we need to hold a spool up to 205mm in diameter and 75mm wide
spoold = 205;
spoolh = 75;
spoolholed = 55;

// for each component, measure its total geometry, and the geometry
// of all standoff holes. by convention, use w/l for width and
// length of the part, and holegapw/holegapl for spacing of the
// holes (of radius r). r ought be (outergap - innergap) / 4.
module stub(stype, height, bh){
	translate([0, 0, bh / 2])
		screw(stype, length=height, head="flat", anchor=TOP, orient=DOWN);
}

// for the common pattern of four stubs forming a quadrilateral
module fourstubs(holegapw, holegapl, r, height, bh){
	translate([-holegapw / 2, -holegapl / 2, 0]){
		stub(r, height, bh);
		translate([holegapw, 0, 0]){
			stub(r, height, bh);
			translate([0, holegapl, 0]){
				stub(r, height, bh);
			}
		}
		translate([0, holegapl, 0]){
			stub(r, height, bh);
		}
	}
}

// corners, for mating to the croom
module corner(){
    side = 20;
    t = totalxy / 2;
    difference(){
        translate([t - side - 6, t - 6, 0]){
            linear_extrude(side){
                polygon([
                    [0, 0],
                    [side, 0],
                    [side, -side]
                ]);
            }
        }
        translate([totalxy / 2 - 12, totalxy / 2 - 12, side / 2]){
            cylinder(side, 5 / 2, 5 / 2, true);
        }
    }
}

// ceramic heater 230C
//  77x62
// holes: 28.3/48.6 3.5
ceramheat230w = 77;
ceramheat230l = 62;
module ceramheat230(height, bh){
	r = 3.5 / 2;
	holegapw = 32;
	holegapl = 44;
	fourstubs(holegapw, holegapl, "M3", height, bh);
}

// 40x80mm worth of air passage through one quadrant of floor
module floorcuts(){
    d = wallz;
    for(i=[0:1:10]){
        translate([-80, 25 + i * 6, d / 2]){
            rotate([0, 0, 45]){
                cube([40, 4, d], true);
            }
        }
        translate([-42, 40 + i * 6, d / 2]){
            cube([40, 4, d], true);
        }
    }
}

module fanmounts(){
    translate([-40 + 11.5 / 2, -(0.95 * totalxy + 16) / 2 + wallxy * 2, 10.5]){
        fanmount();
        translate([0, 0, 70]){
            mirror([0, 0, 1]){
                fanmount();
            }
            translate([0, -1, 0]){
                mirror([1, 0, 0]){
                    rotate([270, 0,0]){
                        fansupportleft();
                    }
                }
            }
        }
    }
}

module hotbox(){
    difference(){
        union(){
            topbottom(wallz);
            translate([0, 0, wallz]){
                linear_extrude(totalz - wallz - topz){
                    polygon(
                        iipoints,
                    paths=[
                        [0, 1, 2, 3, 4, 5, 6, 7],
                        [8, 9, 10, 11, 12, 13, 14, 15]
                    ]);
                }
            }
            // four corners for mating to croom
            corner();
            mirror([0, 1, 0]){
                corner();
            }
            mirror([1, 0, 0]){
                corner();
            }
            mirror([1, 1, 0]){
                corner();
            }
        }
        // now remove all other interacting pieces
        union(){
            // 80x80mm worth of air passage cut into the floor
            floorcuts();
            mirror([1, 0, 0]){
                floorcuts();
            }
            translate([0, 0, totalz - wallz]){
                mirror([0, 0, 1]){
                    top();
                }
            }
            // hole for heating element wires
            translate([ceramheat230w / 2 + 10, totalxy / 4 + 10, 0]){
                cylinder(wallz, 4, 4);
            }
            // exhaust fan hole
            difference(){
                translate([0, -totalxy / 2, 80 / 2 + 5]){
                    cube([80, 25, 80], true);
                }
                fanmounts();
                mirror([1, 0, 0]){
                    fanmounts();
                }
            }
            // central column
            cylinder(10, 30 / 2, 30 / 2, true);
        }
    }
    // pegs for the ceramic heating element. we want it entirely
    // underneath the spool, but further towards the perimeter
    // than the center
    translate([0, totalxy / 4 + 10, wallz / 2]){
        ceramheat230(10, wallz);
    }
}

// walls to support the spool

ga37rgh = 53.3;
ga37rgr = 37 / 2;
//ga37rgshaftr

module spool(){
    translate([0, 0, wallz + elevation]){
        linear_extrude(spoolh){
            difference(){
                circle(spoold / 2);
                circle(spoolholed / 2);
            }
        }
    }
}

/*
multicolor("green"){
    spool();
}*/

multicolor("purple"){
    hotbox();
}