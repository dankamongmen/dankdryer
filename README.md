<p align="center">
ESP32, OpenSCAD, and KiCad sources for the Dankdryer, the world's best filament dryer.
 <img alt="Side view" src="images/render.png"/>
 a nick black joint
</p>

# Features

* Temperatures up to at least 150℃  when printed with proper materials.
  * Such temperatures are appropriate (necessary) for certain engineering filaments.
  * A high-temperature spool is required at these temperatures.
* Accurate weight sensing throughout to determine how much water has been exorcised.
  * Post contextless deltas to reddit in the heat of dumbass arguments! DATA, bitch!
* Slow rotation like delicious savory meat.
* Equal heating of all the spool's filament.
* Control and reporting over MQTT via WiFi.
* OTA firmware upgrades.
* Isolated hot and cool chambers, with most active equipment in the cool chamber.
* Humidity sensing and temperature sensing in both chambers.
* Entirely open source.

More info at [dankwiki](https://nick-black.com/dankwiki/index.php/Dankdryer).

# Dependencies

The project is built with GNU Make.
Running `make` in the toplevel will attempt to build firmware and STLs.
Building the firmware requires the `esp-idf` library and a configured
ESP-IDF environment (i.e. the various `IDF_*` environment variables
must be set), along with CMake. `idf.py` ought be in your `$PATH`,
and work when invoked.

A network configuration file must be created and populated at
`esp32-s3/dankdryer/dryer-network.h`.

## Firmware
* [esp-idf](https://github.com/espressif/esp-idf) 5.3+
* [CMake](https://gitlab.kitware.com/cmake/cmake) 3.16+

## 3D models
* OpenSCAD 2024.10+

I use
[BOSL2](https://github.com/BelfrySCAD/BOSL2), which
is included as a submodule.

## PCB
* KiCad 8+

# Construction / BOM

The device as designed requires approximately 600g of high-temperature filament
(PC, PAHT-CF, etc.) to print, as well as about 20g of polycarbonate. The exact
amount of filament will depend on material and print settings. Six pieces must
be printed:

 * Hot chamber (pa6-cf)
 * Cool chamber (pa6-cf)
 * Top (pa6-cf)
 * Motor sheath (polycarbonate)
 * Sheath coupling (polycarbonate)
 * Spool platform (polycarbonate)

The hot and cool chamber dominate filament consumption, though the top is
non-negligible.

### Hot chamber

* [230C 110V ceramic heating element](https://www.amazon.com/dp/B0BXNPXXYW)
* [KSD301 175C NC thermal control](https://www.aliexpress.us/item/2251832675942217.html)
* [Hall sensor](https://www.digikey.com/en/products/detail/diodes-incorporated/AH3712Q-P-B/19920700)
* Custom PCB (fabrication cost is negligible; see components below)

We need a temperature sensor and hall effect sensor (and obviously the
heating circuit) in the hot box; nothing else ought be there.

### Cool chamber

* [Greartisan 5RPM 12V DC motor](https://www.amazon.com/dp/B072N867G3/)
* [NEMA plug + rocker switch](https://www.amazon.com/ASHATA-Rocker-Switch-Adapter-Printer/dp/B085VSS1F2)
* [160W 12V AC adapter](https://www.amazon.com/gp/product/B0D7GMVK2F)
* [8dBi antenna](https://www.aliexpress.us/item/3256807262687553.html)
* 5kg bar strain load cell
* 2-to-4 cable splitter
* Custom PCB (fabrication cost is negligible; see components below)

Of these, we don't need nearly so powerful an AC adapter, and we ought
be able to use a cheapter motor.

#### PCB components

<img alt="Custom PCB" src="images/pcb.png"/>

Generated via Kicad:

|"Designator"|"Footprint"|"Quantity"|"Designation"|
|------------|-----------|----------|-------------|
|U1|TPS62132|1|TPS62132|
|R4|R_1206_3216Metric_Pad1.30x1.75mm_HandSolder|1|360|
|R2|R_1206_3216Metric_Pad1.30x1.75mm_HandSolder|1|620|
|R6,R8|R_1206_3216Metric_Pad1.30x1.75mm_HandSolder|2|4.7k|
|C15,C16|C_1206_3216Metric_Pad1.33x1.80mm_HandSolder|2|1n|
|SW2|SW_B3F-1000_OMR|1|bootmode|
|R13,R20,R15|R_1206_3216Metric_Pad1.30x1.75mm_HandSolder|3|10k|
|R5,R1,R22|R_1206_3216Metric_Pad1.30x1.75mm_HandSolder|3|680|
|C7,C3,C9,C8,C20|C_1206_3216Metric_Pad1.33x1.80mm_HandSolder|5|0.1u|
|U2|TRANS_AH3712-P-B_DIO|1|AH3712Q-P-B|
|C6,C13|C_1206_3216Metric_Pad1.33x1.80mm_HandSolder|2|22u|
|C10,C17|C_1206_3216Metric_Pad1.33x1.80mm_HandSolder|2|1u|
|IC1|DIP792W51P254L1905H53Q16N|1|NAU7802KGI|
|J10|FanPinHeader_1x04_P2.54mm_Vertical|1|lower fan|
|C19,C2,C1,C14|C_1206_3216Metric_Pad1.33x1.80mm_HandSolder|4|10u|
|J2|JST_VH_B5P-VH-B_1x05_P3.96mm_Vertical|1|to microboard|
|J4|TerminalBlock_bornier-2_P5.08mm|1|heater|
|J8|TerminalBlock_bornier-2_P5.08mm|1|motor|
|J7|FanPinHeader_1x04_P2.54mm_Vertical|1|upper fan|
|R9,R19|R_1206_3216Metric_Pad1.30x1.75mm|2|15k|
|U6|XCVR_ESP32-C6-MINI-1U-H4|1|ESP32-C6-MINI-1U|
|J11|PinHeader_1x01_P1.00mm_Vertical|1|testpin|
|R16|R_1206_3216Metric_Pad1.30x1.75mm_HandSolder|1|412|
|J13|BarrelJack_Wuerth_6941xx301002|1|barreljack|
|R7|R_1206_3216Metric_Pad1.30x1.75mm_HandSolder|1|1.62k|
|C4,C18|C_1206_3216Metric_Pad1.33x1.80mm_HandSolder|2|.01u|
|J5|TerminalBlock_bornier-4_P5.08mm|1|5kg load cell|
|R10,R11|R_0805_2012Metric_Pad1.15x1.40mm_HandSolder|2|47|
|U5|TO-92_Inline|1|LM35-LP|
|R3|R_1206_3216Metric_Pad1.30x1.75mm_HandSolder|1|100|
|L1|IND_IFSC_1515AH_VIS|1|2.2u|
|R14|R_1206_3216Metric_Pad1.30x1.75mm_HandSolder|1|3.3k|
|OC1|MOC306XS_LTO|1|MOC3063S|
|C12|C_1206_3216Metric_Pad1.33x1.80mm_HandSolder|1|3.3n|
|C11|C_0805_2012Metric_Pad1.18x1.45mm_HandSolder|1|470p|
|R21|R_1206_3216Metric_Pad1.30x1.75mm_HandSolder|1|68|
|J9|PinHeader_1x04_P2.54mm_Vertical|1|UART|
|J6|JST_PHD_B8B-PHDSS_2x04_P2.00mm_Vertical|1|LCD|
|J3|JST_VH_B5P-VH-B_1x05_P3.96mm_Vertical|1|to baseboard|
|Q3|TO-263-2|1|T1210-800G|
|Q2|SOT-323_SC-70|1|SSM3K127TU|
|R12|R_1206_3216Metric_Pad1.30x1.75mm_HandSolder|1|330|
|U3|SOT-89-3|1|HT7550|

# MQTT

MQTT is used to report status and to accept commands.

## Controls

* `NAME/control/tare`: tare using the last weight read
* `NAME/control/dry`: takes as argument a string "DRYS/TEMP", where DRYS and TEMP are unsigned integers
    specifying the number of seconds to dry, and the temperature to dry at. Any ongoing drying operation
    will be replaced with the newly specified one. Specifying zero for DRYS will cancel any ongoing
    drying operation.
* `NAME/control/lpwm`: takes as argument a hexadecimal number between 0 and 255, left-padded with zeroes
    so as to be exactly two digits, i.e. "00".."ff". Sets the lower fan's PWM.
* `NAME/control/upwm`: takes as argument a hexadecimal number between 0 and 255, left-padded with zeroes
    so as to be exactly two digits, i.e. "00".."ff". Sets the upper fan's PWM.
* `NAME/control/factoryreset`: takes no arguments. Blanks the persistent storage, disables the motor
    and heater, and reboots.

# Renderings

View from the top of the lower chamber by itself, with the AC
adapter and motor assembly present.

<p align="center">
<img alt="Top view, lower chamber" src="images/topview-croom.png"/>
</p>

Combined OpenSCAD render for mating testing.

<p align="center">
 <img alt="Combined render" src="images/stl.png"/>
</p>

# Questions

* How does air flow? Let's get some visible air and test it.
* What all could we accomplish by reading RFID?
