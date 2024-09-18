$fn = 64;

current_color = "ALL";

module multicolor(color) {
	if (current_color != "ALL" && current_color != color) { 
		// ignore our children.
	} else {
		color(color)
		children();
	}        
}

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

jointsx = 10;
jointsy = 60;
//cbottomz = 8; // for M4x8 screw, given 2mm of adapter rim
ctopz = wallz;
croomz = wallz + 8 + ctopz + 90; // 80mm fan; ought just need sin(theta)80
outerxy = (totalxy + 14) * sqrt(2);

mh = 8; // mount height

shieldw = 88;
module shieldscrew(){
    translate([(-shieldw + 6) / 2, (19.5 + 6) / 2, mh / 2]){
        cylinder(mh, 4 / 2, 4 / 2, true);
    }
}

module shieldbinder(){
    difference(){
        translate([(-shieldw + 6) / 2, (19.5 + 6) / 2, mh / 2]){
            cube([6, 6, mh], true);
        }
        shieldscrew();
    }
}

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

module fanmount(h){
    difference(){
        cube([11.5, h, 11.5], true);
        translate([-2, 0, -2]){
            rotate([90, 0, 0]){
                cylinder(h, 2, 2, true);
            }
        }
    }
}

// support underneath the upper left fan mount, so it's not hanging
module fansupportleft(h){
    translate([0.25, 0.25, 0]){
        linear_extrude(h){
            polygon([
                [-6, 5.5],
                [5.5, 5.5],
                [5.5, 17]
            ]);
        }
    }
}

// screw hole for the top hold
module holdhole(h){
    translate([-10, -10, h / 2]){
        cylinder(h, 4 / 2, 4 / 2, true);
    }
}

module holdholes(h){
    holdhole(h);
    mirror([0, 1, 0]){
        holdhole(h);
    }
    mirror([1, 0, 0]){
        holdhole(h);
    }
    mirror([1, 1, 0]){
        holdhole(h);
    }
}

module top(){
    difference(){
        hull(){
            topbottom(1);
            translate([0, 0, topz]){
                linear_extrude(1){
                    polygon(ipoints);
                }
            }
        }
        holdholes(6);
    }
}