// bottom chamber for a high-temperature (140C) filament dryer
// intended to be printed in ASA
// holds the MCU, motor, AC adapter, etc.
include <core.scad>

// we need to hold a spool up to 205mm in diameter and 75mm wide
spoold = 205;
spoolh = 75;
spoolholed = 55;

// we'll want some room around the spool, but the larger our
// chamber, the more heat we lose.
wallz = 2; // bottom thickness; don't want much
gapxy = 2; // gap between spool and walls; spool might expand with heat!
wallxy = 2;
topz = 5; // height of top piece
gapz = 2; // spool distance from bottom of top
elevation = 10; // spool distance from bottom
chordxy = 33;

totalxy = spoold + wallxy * 2 + gapxy * 2;
totalz = spoolh + wallz + topz + gapz + elevation;
totald = sqrt(totalxy * totalxy + totalxy * totalxy);

// motor is 37x33mm diameter gearbox and 6x14mm shaft
motorboxh = 33;
motorboxd = 37;
motorshafth = 14;
motorshaftd = 6;

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


jointsx = 10;
jointsy = 60;
cbottomz = 8; // for M4x8 screw, given 2mm of adapter rim
ctopz = wallz;
croomz = wallz + cbottomz + ctopz + 90; // 80mm fan; ought just need sin(theta)80
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

mh = 8; // mount height

adj = croomz - cbottomz;
hyp = sqrt(14 * 14 + adj * adj);
module fanmount(){
    rotate([asin(-14/hyp), 0, 0]){
        difference(){
            cube([5, 2, 5], true);
            rotate([90, 0, 0]){
                cylinder(2, 2, 2, true);
            }
        }
    }
}

// support underneath the upper left fan mount, so it's not hanging
module fansupportleft(){
    rotate([asin(-14/hyp) + 270, 0, 0]){
        linear_extrude(2){
            polygon([
                [-2.5, 2.5],
                [2.5, 2.5],
                [2.5, 7.5]
            ]);
        }
    }
}

// inverted frustrum
module controlroom(){
    difference(){
        union(){
            difference(){
                croomcore();
                union(){
                    scale(0.95){
                        croomcore();
                    }
                    // holes for AC adapter mounting screws
                    translate([0, 60, -croomz + 2]){
                        acadapterscrews(cbottomz - 2);
                    }
                    // hole for AC wire
                    translate([-totalxy / 2 - 10, 60, -croomz + 5 + cbottomz]){
                        rotate([0, 90, 0]){
                            cylinder(10, 5, 5);
                        }
                    }
                    // fan hole
                    translate([0, -totalxy / 2, -croomz + cbottomz + 40 + 10]){
                        rotate([asin(-14/hyp), 0, 0]){
                            cube([80, 25, 80], true);
                        }
                    }
                }
           }
           // fan mounts
           translate([-40 + 5 / 2, -(0.95 * totalxy + 14) / 2 + 1, -croomz + cbottomz + 10 + 5/2]){
            fanmount();
            translate([0, 5, 75]){
                fanmount();
                mirror([1, 0, 0]){
                    fansupportleft();
                }
            }
           }
           translate([40 - 5 / 2, -(0.95 * totalxy + 14) / 2 + 1, -croomz + cbottomz + 10 + 5/2]){
            fanmount();
            translate([0, 5, 75]){
                fanmount();
                fansupportleft();
            }
           }
           // load cell mounting base
           translate([-76 / 2 + 21.05 / 2, 0, -croomz + cbottomz + mh / 2]){
               cube([21.05, 13.5, mh], true);
           }
           // mount for perfboard
            translate([-mh, -0.95 * totalxy / 2 + mh, -croomz + cbottomz + mh / 2]){
                perfmount();
                translate([-80 - 3.3, 0, 0]){
                    perfmount();
                }
                translate([0, 61.75 + 3.3, 0]){
                    perfmount();
                    translate([-80 - 3.3, 0, 0]){
                        perfmount();
                    }
                }
            }
       }
       // holes for loadcell screws
       translate([-76 / 2 + 5.425, 0, -croomz + (cbottomz - 2 + 8) / 2 + 2]){
           cylinder(cbottomz - 2 + 8, 2, 2, true);
       }
       translate([-76 / 2 + 15.625, 0, -croomz + (cbottomz - 2 + 8) / 2 + 2]){
           cylinder(cbottomz - 2 + 8, 2, 2, true);
       }
       // perfboard holes
        translate([-mh, -0.95 * totalxy / 2 + mh, -croomz + 2 + (mh + cbottomz - 2) / 2]){
            perfmounthole();
            translate([-80 - 3.3, 0, 0]){
                perfmounthole();
            }
            translate([0, 61.75 + 3.3, 0]){
                perfmounthole();
                translate([-80 - 3.3, 0, 0]){
                    perfmounthole();
                }
            }
        }
    }
    // shield around load cell
    translate([0, 0, -croomz + cbottomz + 17.5 / 2]){
        difference(){
            cube([82, 19.5, 17.5], true);
            cube([78, 14.5, 17.5], true);
        }
    }
    // mount for motor
    translate([50, -50, -croomz + cbottomz + 37 / 2]){
        rotate([0, 0, 135]){
            cube([65, 37, 37], true);
            translate([0, 0, 37]){
                difference(){
                    translate([0, 0, -37 / 2]){
                        cube([65, 37, 37 / 2], true);
                    }
                    rotate([0, 90, 0]){
                        cylinder(65, 37 / 2, 37 / 2, true);
                    }
                }
            }
        }
    }
}

module perfmount(){
    cube([mh, mh, mh], true);
}

module perfmounthole(){
    cylinder(mh + cbottomz - 2, 2, 2, true);
}

module motor(){
    cylinder(motorboxh, motorboxd / 2, motorboxd / 2);
    translate([0, 0, motorboxh]){
        cylinder(motorshafth, motorshaftd / 2, motorshaftd / 2);
    }
}

multicolor("black"){
    controlroom();
        /*multicolor("silver"){
            translate([0, 60, -croomz + cbottomz + 30 / 2]){
                acadapter();
            }
            translate([0, 0, -croomz + cbottomz + 13.5 / 2]){
                loadcell();                
            }
        }*/
}

module acadapter(){
    difference(){
        cube([165.1, 60, 30], true);
        translate([0, 0, -15]){
            acadapterscrews(30);
        }
    }
}

// 60mm wide total
// screw holes are 6mm in from sides, so they start at
// 6mm (through 10mm) and 50mm (through 54mm)
module acadapterscrews(l){
    translate([165.1 / 2 - 3, -22, 0]){
        cylinder(l, 2, 2);
    }
    mirror([0, 1, 0]){
    mirror([1, 0, 0]){
        translate([165.1 / 2 - 3, -22, 0]){
            cylinder(l, 2, 2);
        }
    }
}
}

/*
multicolor("pink"){
    translate([0, 0, -motorboxh]){
        motor();
    }
}*/

module loadcell(){
    // https://amazon.com/gp/product/B07BGXXHSW
    difference(){
        cube([76, 13.5, 13,5], true);
    }
}