$fn=20;

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

module shieldscrew(){
    translate([(-82 + 6) / 2, (19.5 + 6) / 2, mh / 2]){
        cylinder(mh, 4 / 2, 4 / 2, true);
    }
}

module shieldbinder(){
    difference(){
        translate([(-82 + 6) / 2, (19.5 + 6) / 2, mh / 2]){
            cube([6, 6, mh], true);
        }
        shieldscrew();
    }
}