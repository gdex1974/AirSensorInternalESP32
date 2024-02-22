$fn=180;

module fireBeetleD()
{
    height = 7;
    width = 29;
    length = 58;
    radius = 2;
    delta = 5;
    x = length - radius;
    y = width - radius;
    z = height;
    hole_diameter = 2.7;
    linear_extrude(height = z) hull() {
        translate([(- x / 2) + (radius / 2), (- y / 2) + (radius / 2), 0])
            circle(r = radius);

        translate([(x / 2) - (radius / 2), (- y / 2) + (radius / 2), 0])
            circle(r = radius);

        translate([(- x / 2) + (radius / 2), (y / 2) - (radius / 2), 0])
            circle(r = radius);

        translate([(x / 2) - (radius / 2), (y / 2) - (radius / 2), 0])
            circle(r = radius);
    }
    translate([-(length - delta) / 2, (width - delta) / 2, height]) cylinder(d = hole_diameter, h = 7, center = false);
    translate([(length - delta) / 2, -(width - delta) / 2, height]) cylinder(d = hole_diameter, h = 7, center = false);
    translate([-(length - delta) / 2, -(width - delta) / 2, height]) cylinder(d = hole_diameter, h = 7, center = false);
    translate([(length - delta) / 2, (width - delta) / 2, height]) cylinder(d = hole_diameter, h = 7, center = false);
}

module bme280i2c()
{
    hole_diameter = 2.7;
    cube([10.5, 13.5, 4]);
    translate([8, 11, - 5]) cylinder(d = hole_diameter, h = 12, center = false);
    translate([3.5, 11, - 10]) cylinder(d = 2.5, h = 8, center = false);
    translate([(10.5 - 8) / 2, 1, 4])cube([8, 2, 6]);
}

module PMS9003M()
{
    x = 40;
    y = 48;
    z = 12;
    radius = 3.5;
    hole_diameter = 2.7;
    union() {
        difference() {
            linear_extrude(height = z) hull() {
                // place 4 circles in the corners, with the given radius
                translate([(- x / 2) + (radius), (- y / 2) + (radius), 0])
                    circle(r = radius);

                translate([(x / 2) - (radius), (- y / 2) + (radius), 0])
                    circle(r = radius);

                translate([(- x / 2) + (radius), (y / 2) - (radius), 0])
                    circle(r = radius);

                translate([(x / 2) - (radius), (y / 2) - (radius), 0])
                    circle(r = radius);
            }
            for (i = [0:1]) rotate([0, 0, i * 180]) translate([(x / 2) - (radius), (y / 2) - (radius), 0]) {
                cylinder(h = 13, r = 3.5, center = false);
                translate([- 3.5, 0, 0])cube([7, 3.5, 13]);
                translate([0, - 3.5, 0]) cube([3.5, 7, 13]);
            }
        }
        translate([0, 0, (z - 1) / 2])linear_extrude(height = 1) hull() {
            // place 4 circles in the corners, with the given radius
            translate([(- x / 2) + (radius), (- y / 2) + (radius), 0])
                circle(r = radius);

            translate([(x / 2) - (radius), (- y / 2) + (radius), 0])
                circle(r = radius);

            translate([(- x / 2) + (radius), (y / 2) - (radius), 0])
                circle(r = radius);

            translate([(x / 2) - (radius), (y / 2) - (radius), 0])
                circle(r = radius);
        }
        for (i = [0:1]) rotate([0, 0, i * 180]) translate([(x / 2) - (radius), (y / 2) - (radius), 0]) {
            cylinder(h = 12, d = hole_diameter, center = false);
        }
        translate([x / 2 - 28 - 4, - y / 2 + 7.5, 12]) cube([8, 9, 5]);
        translate([x / 2 - 28, 36 - y / 2, 12]) difference() {
            cylinder(d = 20, h = 5);
            rotate([0, 0, 135])translate([- 2.5, 0, - 1])cube([5, 10, 10]);
            translate([0, 0, - 1])cylinder(d = 12, h = 7);
        }
    }
}

sps30Width = 40.9;
sps30Depth = 40.8;
sps30Height = 12.5;
module sps30()
{
    cube([sps30Depth, sps30Width, sps30Height]);
    bigInletWidth = 6.6;
    bigInletHeight = 3.8;
    translate([sps30Depth, sps30Width - bigInletWidth - 2.2, 8.2])cube([4, bigInletWidth, bigInletHeight]); // big inlet
    smallInletWidth = 10;
    smallInletHeight = 2;
    translate([sps30Depth, sps30Width - smallInletWidth - 5.2, - 0.2])cube([4, smallInletWidth, smallInletHeight]);
    // small inlet
    outletWidth = 14.8;
    outletHeight = 5;
    translate([sps30Depth, 5.5, 5.8])cube([4, outletWidth, outletHeight]); //outlet
}


module stepup5V()
{
    union() {
        cube([18, 26, 1.5]);//3.8
        translate([2, - 1, - 1.5 - 1.4 + 0.6]) cube([14, 21.7, 2.3 + 1.4]);
    }
}

module epd37()
{
    plateHeight = 1.8;
    shift = 0.6;
    union() {
        cube([58, 96.5, plateHeight]);
        translate([5, 5, - 3]) cube([48, 82, 3]);
        translate([52, 49.4, plateHeight]) cube([5.7, 20, 6]);
        translate([shift, 33.6, plateHeight]) cube([5, 51.2, 20.2]);
        translate([shift, 33.6 - (60 - 51.2) / 2, 0.3 + 18.6]) cube([7, 60, 1.5]);
        translate([shift + 4.2, 33.6 - (60 - 51.2) / 2 + 3, 0.3 + 18.6 - 5.5])cylinder(d = 1.8, h = 9);
        translate([shift + 4.2, 33.6 - (60 - 51.2) / 2 + 3 + 54, 0.3 + 18.6 - 5.5])cylinder(d = 1.8, h = 9);
        translate([shift + 4.8, 33.6, 0.3 + 18.6 + 1.5]) cube([3, 51.2, 1.4]);
    }
}

module LiIonCase()
{
    base = 55;
    width = 76;
    height = 20;
    depth = 21;
    hole_diameter = 2.7;
    cube([depth, width, height]);
    translate([depth / 2, width / 2, - 6]) cylinder(d = hole_diameter, h = 6);
}

basePlateThickness = 2.4;
internalHeight = 122;
internalBatteryHeight = 25;
internalBatteryWidth = 76;
internalMainWidth = 58;
internalSpaceHeignt = 21.6;
basePlateShiftZ = 18;

module mainFrame()
difference() {
    union() {
        color("magenta") translate([0, - internalBatteryHeight, basePlateShiftZ])cube([internalMainWidth,
            internalHeight, basePlateThickness]);
        color("magenta") translate([- 9, - internalBatteryHeight, basePlateShiftZ])cube([internalBatteryWidth,
            internalBatteryHeight, basePlateThickness]);
        color("green") translate([2, 5, 14.5]) cube([26, 20, 4]);
        color("magenta") translate([2.5, 3, 13]) {
            cylinder(d = 4, h = 6);
            translate([0, 24, 0]) cylinder(d = 4, h = 6);
            translate([53, 0, 0]) cylinder(d = 4, h = 6);
            translate([53, 24, 0]) cylinder(d = 4, h = 6);
        }

        translate([12.7 + 1.3, 36.7 + 51.3, 12.5]) cylinder(d = 4, h = 6);
        translate([16.5, 38.5 - 1, 4.9]) cube([41.5, 3.6, 13.1]);
        translate([17.5, 35 + 44.8, 4.9]) cube([40.5, 3.6, 13.1]);

        color("magenta") translate([- 1 + 27.5, - 5 - 0.5, - 1.5 + 0.2 - 1.1]) union() {
            cube([5, 6, internalSpaceHeignt]);
            translate([0, 1, 6]) rotate([- 75, 0, 0]) cube([5, 6, 5]);
        }

        color("magenta") {
            translate([0, 92, - 0.6]) cube([4, 5, 20]);
            translate([54, 92, - 0.6]) cube([4, 5, 20]);
            translate([55, 30, - 1]) cube([3, 4, 20]);
        }
    }
    union() {
        color("green")translate([17.2, 40, 5.5]) sps30();
        translate([67, - 5, - basePlateThickness])rotate([0, - 90, 90])LiIonCase();

        translate([1, 0, - basePlateThickness])epd37();

        color("red") translate([29, 15, 6])rotate([0, 0, 0])fireBeetleD();
        color("blue")translate([25, 96, 14])rotate([0, 180, 90]) bme280i2c();

        color("cyan") translate([27, 24, 15.8]) rotate([0, 180, 90]) difference() {
            stepup5V();
            translate([(18 - 1) - 1.2, 24.3 - 1 - 1.5, - 2]) cylinder(d = 1.9, h = 4, center = false);
            translate([(0 + 1) + 1.2, 24.3 - 1 - 1.5, - 2]) cylinder(d = 1.9, h = 4, center = false);
        }
    }
    translate([3, - 2, 11])rotate([0, 0, 0])cylinder(d = 2.7, h = 10);
    translate([55, - 2, 11])rotate([0, 0, 0])cylinder(d = 2.7, h = 10);
    translate([- 1, 94.5, 16])rotate([0, 90, 0])cylinder(d = 2.7, h = 10);
    translate([54, 94.5, 16])rotate([0, 90, 0])cylinder(d = 2.7, h = 10);
}

frontThickness=2;
wallThickness=2;

wallHeight=internalSpaceHeignt+frontThickness+basePlateThickness;
widthDiff2=(internalBatteryWidth-internalMainWidth)/2;
internalGap=0.2;

module cover()
difference() {
    union() {
        color("red") translate([0, - 25, - basePlateThickness - frontThickness]) union() {
            translate([- internalGap, internalBatteryHeight, 0])cube([internalMainWidth + 2 * internalGap,
                        internalHeight - internalBatteryHeight + internalGap, frontThickness]);
            translate([- widthDiff2 - internalGap, - internalGap, 0]) cube([internalBatteryWidth + 2 * internalGap,
                    internalBatteryHeight + internalGap, frontThickness]);
            translate([- internalGap, internalHeight + internalGap, 0]) cube([internalMainWidth + 2 * internalGap,
                wallThickness, wallHeight]);
            translate([- wallThickness - internalGap, internalBatteryHeight + wallThickness, 0]) cube([wallThickness,
                        internalHeight - internalBatteryHeight + internalGap, wallHeight]);
            translate([internalMainWidth + internalGap, internalBatteryHeight + wallThickness, 0]) cube([wallThickness,
                        internalHeight - internalBatteryHeight + internalGap, wallHeight]);
            translate([- widthDiff2 - internalGap, internalBatteryHeight, 0]) cube([widthDiff2, 2, wallHeight]);
            translate([internalMainWidth + internalGap, internalBatteryHeight, 0]) cube([widthDiff2, wallThickness,
                wallHeight]);
            translate([- widthDiff2 - wallThickness - internalGap, - internalGap, 0]) cube([wallThickness,
                        internalBatteryHeight + wallThickness + internalGap, wallHeight]);
            translate([internalMainWidth + widthDiff2 + internalGap, - internalGap, 0]) cube([wallThickness,
                        internalBatteryHeight + wallThickness + internalGap, wallHeight]);
            translate([- internalGap, - internalGap, 0])union() {
                translate([- widthDiff2 - wallThickness, - 3, 0]) cube([internalBatteryWidth + 2 * (wallThickness +
                    internalGap), 3, wallHeight]);
                rotate([- 4.4, 0, 0])translate([- 11, - 5, - 0.5]) cube([internalBatteryWidth + 2 * (wallThickness +
                    internalGap), 3, wallHeight]);
                rotate([- 4.4, 0, 0])translate([- 11, - 5, - 0.5]) cube([internalBatteryWidth + 2 * (wallThickness +
                    internalGap), 2, 37]);
            }
        }

        color("magenta") {
            translate([3, - 2, - basePlateThickness]) difference() {
                union() {
                    cylinder(d = 4, h = internalSpaceHeignt - 1);
                }
                translate([0, 0, internalSpaceHeignt - 8.5]) cylinder(d = 1.9, h = 9);
            }
            translate([55, - 2, - basePlateThickness]) difference() {
                union() {
                    cylinder(d = 4, h = internalSpaceHeignt - 1);
                }
                translate([0, 0, internalSpaceHeignt - 8.5]) cylinder(d = 1.9, h = 9);
            }
        }
    }
    translate([0, 0, - basePlateThickness])epd37();
    translate([- 5, 94.5, internalSpaceHeignt - 4.4])rotate([0, 90, 0])cylinder(d = 3, h = 10);
    translate([54, 94.5, internalSpaceHeignt - 4.4])rotate([0, 90, 0])cylinder(d = 3, h = 10);
    translate([- 11 - internalGap - 1, - 35 - internalGap, - 6.8])cube([internalBatteryWidth + 2 * (wallThickness +
        internalGap + 1), 40, basePlateThickness]); // proper cut for the stand
    translate([- 5 + 60 + 2, 7, - 4.4 + 13]) cube([10, 9, 3.4]); // usb charge
    translate([17.2, 39.8, 5.5]) sps30();
}

mainFrame();

rotate([0,180,0]) translate([22,0,-14.8])
    translate([0,0,basePlateShiftZ + basePlateThickness - internalSpaceHeignt])
        cover();
