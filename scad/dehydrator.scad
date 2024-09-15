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

opoints = [
            [-totalxy / 2 + chordxy, -totalxy / 2],
            [totalxy / 2 - chordxy, -totalxy / 2], 
            [totalxy / 2, -totalxy / 2 + chordxy],
            [totalxy / 2, totalxy / 2 - chordxy],
            [totalxy / 2 - chordxy, totalxy / 2],
            [-totalxy / 2 + chordxy, totalxy / 2],
            [-totalxy / 2, totalxy / 2 - chordxy],
            [-totalxy / 2, -totalxy / 2 + chordxy]
        ];

ipoints = [[-totalxy / 2 + (chordxy + wallxy), -totalxy / 2 + wallxy],
                [totalxy / 2 - (chordxy + wallxy), -totalxy / 2 + wallxy], 
                [totalxy / 2 - wallxy, -totalxy / 2 + (chordxy + wallxy)],
                [totalxy / 2 - wallxy, totalxy / 2 - (chordxy + wallxy)],
                [totalxy / 2 - (chordxy + wallxy), totalxy / 2 - wallxy],
                [-totalxy / 2 + (chordxy + wallxy), totalxy / 2 - wallxy],
                [-totalxy / 2 + wallxy, totalxy / 2 - (chordxy + wallxy)],
                [-totalxy / 2 + wallxy, -totalxy / 2 + (chordxy + wallxy)]];
iipoints = concat(opoints, ipoints);

module topbottom(height){
    linear_extrude(wallz){
        polygon(opoints);
    }
}

outerxy = (totalxy + 14) * sqrt(2);

module croomcore(){
    rotate([0, 0, 45]){
        mirror([0, 0, 1]){
            cylinder(croomz,
                     totalxy * sqrt(2) / 2,
                     outerxy / 2, $fn = 4);
        }
    }
}

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
        }
        // now remove all other interacting pieces
        union(){
            translate([0, 0, totalz - wallz]){
                mirror([0, 0, 1]){
                    top();
                }
            }
            // hole for heating element wires
            translate([ceramheat230w / 2 + 10, totalxy / 4 + 10, 0]){
                cylinder(wallz, 4, 4);
            }
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

module top(){
    hull(){
        topbottom(1);
        translate([0, 0, topz]){
            linear_extrude(1){
                polygon(ipoints);
            }
        }
    }
}

multicolor("white"){
    translate([totalxy + 10, 0, 0]){
        top();
    }
}

/*
multicolor("green"){
    spool();
}*/

multicolor("purple"){
    hotbox();
}