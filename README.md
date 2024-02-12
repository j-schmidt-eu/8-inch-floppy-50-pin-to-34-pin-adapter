# 8-inch floppy drive - 50-pin to 34-pin adapter Rev. 2

> The 50-pin to 34-pin adapter can be used to adapt 8-inch floppy disk drives (using a 50-pin connector) to be controlled by 3.5-inch and 5.25-inch floppy controllers (using a 34-pin connector).

![PCB top render](README/pcb_top_render.jpg "PCB top render")

![PCB bottom render](README/pcb_bottom_render.jpg "PCB bottom render")

## Table of Contents
- [Introduction](#Introduction)
- [Specifications](#Specifications)
- [Setup](#Setup)
- [Operation](#Operation)
- [Compatibility](#Compatibility)
- [Manufacturing](#Manufacturing)
- [Firmware](#Firmware)
- [FAQ](#FAQ)
- [Licensing](#Licensing)


## Introduction
Many 8-inch floppy drives use a 50-pin card-edge connector to link them to a controller. This is a problem, since modern floppy controllers use a 34-pin IDC connector.
To solve this problem an adapter is necessary that connects compatible signals and generates signals that are unique to the 8-inch floppy drives.
This project aims to provide the schematic, PCB design and firmware source code to build such an adapter.
All compatible signals are directly linked from the 50-pin connector to the 34-pin connector.
Other signals are generated with an onboard microcontroller.

![PCB top picture](README/pcb_top_picture.jpg "PCB top picture")

![PCB bottom picture](README/pcb_bottom_picture.jpg "PCB bottom picture")

## Specifications
- Power
  - Input voltage: 5V DC
  - Maximum input current: 750mA
  - Connector: 4-pin Berg/Amphenol
- Maximum number of drives per adapter: 2
- Maximum number of adapters per controller: 1
- Pass-through signals:

  | Direction | Name |
  | --------- | ---- |
  | IN        | Index |
  | OUT       | Drive select A |
  | OUT       | Drive select B |
  | OUT       | Direction |
  | OUT       | Step |
  | OUT       | Write data |
  | OUT       | Write gate |
  | IN        | Track 0 |
  | IN        | Write protect |
  | IN        | Read data |
  | OUT       | Side select |
- Generated signals:

  | Direction | Name | Signal source |
  | --------- | ---- | ------------- |
  | OUT       | TG43 (Track greater than 43) /<br>Reduce write current /<br>Filter switch | Determined by tracking the head position.<br>Multiple options available (e.g. control via external signal) |
  | OUT       | Head load | Derived from Motor enable signal.<br>Multiple options available (e.g. control via external signal) |
  | IN        | Ready | Derived from ready signal.<br>Can be disabled by removing a solder jumper bridge JP1 |


## Setup
The adapter can drive up to two drives.

### Floppy drive
A single drive should always be jumpered to be drive A.
The drive should have the termination resistors enabled/present.

A two drive setup should have one drive jumpered to be drive A and the other to be drive to be jumpered drive B.
The last drive (the drive connected at the end of the 50-pin ribbon cable) should have the termination resistors enabled/present, the other drive must have the termination resistors disabled/removed.

__**Important: Only one set of termination resistors must be enabled/present! Having more than one termination resistors enabled/present can damage the adapter and the floppy controller!**__

### Connections

#### Data
To connect the controller to the adapter, use a 34-pin ribbon cable (IDC socket connector to IDC socket connector).
It is recommended to connect the adapter after the twist in the ribbon cable. 

_Note: **When using straight cable without twist, drive A and drive B will be swapped on the controller side.**_

The drive(s) should be connected to the adapter with a 50-pin ribbon cable (IDC card edge connector to IDC socket connector).

#### Power
The adapter should be connected to a power supply via the 4 pin Berg/Amphenol connector (only the 5V line will be used). The pinout is ATX compatible.
Power for the floppy drive(s) should be supplied as described in the operations manual for that specific drive.


## Operation
To operate the adapter a single push button (command button) is present on the bottom right corner of the PCB.

_Note: **The button will be disabled when any drive is in use.**_

### Display
For status display and settings, a two-digit seven-segment display is present on the top right corner of the PCB.
A short press on the command button can be used to cycled between the following display modes:

| Mode | Dot status | Description | Range |
| ---- | ---- | ----------- | ----- |
| Track | Left: off<br>Right: on | Current head position | 0 ... 80 |
| Speed | Left: on (positive value) / blink (negative value)<br>Right: off | Percent deviation from nominal rotational speed | -9.9% ... 9.9% |
| Revision | Left: blink<br>Right: blink<br>Alternates between left and right | Software revision number | 1 ... 99 |
| Dark | Left: off<br>Right: off | Disables display | - |

### Settings
To edit the settings of the adapter the setup menu can be entered by pressing the command button for longer than two seconds.
When navigating the setup menu both dots will be lit.
A short press on the command button can be used to cycled between the menu items.
Item value editing can be enabled and disabled by pressing the command button for longer than two seconds.
During editing both dots will blink simultaneously.
A short press on the command button can be used to cycled between value.

To exit the setup menu, navigate to the Exit item and press the command button for longer than two seconds.

The following menu items are available (in listed order):

| Item | Display | Description | Values | Default |
| ---- | ------- | ----------- | ------ | ------- |
| Default display mode | Left: 1<br>Right: value | Default display mode after adapter power up | 0: Track<br>1: Speed<br>2: Revision<br>3: Dark | 0: Track |
| Drive A<br>Maximum track number | Left/Right: value | Maximum track number that the drive can step to | 32 ... 80 | 76 |
| Drive A<br>TGxx signal | Left: 3<br>Right: value | Source for TG43 / reduce write current / filter signal | 0: Use thresholds<br>1: Always active<br>2: Always inactive<br>3: From external signal | 0: Use thresholds |
| Drive A<br>Head load signal | Left: 4<br>Right: value | Source for head load signal | 0: From [motor A enable]<br>1: Always active<br>2: Always inactive<br>3: From external signal | 0: From [motor A enable] |
| Drive A<br>TGxx write threshold | Left/Right: value | Track number threshold (greater or equal) for TGxx signal active when writing | 0 ... 99 | 44 |
| Drive A<br>TGxx read threshold | Left/Right: value | Track number threshold (greater or equal) for TGxx signal active when reading | 0 ... 99 | 99 |
| Drive B<br>Maximum track number | Left/Right: value | Maximum track number that the drive can step to | 32 ... 80 | 76 |
| Drive B<br>TGxx signal | Left: 8<br>Right: value | Source for TG43 / reduce write current / filter signal | 0: Use thresholds<br>1: Always active<br>2: Always inactive<br>3: From external signal | 0: Use thresholds |
| Drive B<br>Head load signal | Left: 9<br>Right: value | Source for head load signal | 0: From [motor B enable]<br>1: Always active<br>2: Always inactive<br>3: From external signal | 0: From [motor B enable] |
| Drive B<br>TGxx write threshold | Left/Right: value | Track number threshold (greater or equal) for TGxx signal active when writing | 0 ... 99 | 44 |
| Drive B<br>TGxx read threshold | Left/Right: value | Track number threshold (greater or equal) for TGxx signal active when reading | 0 ... 99 | 99 |
| Exit | Left/Right: dark | - | - | - |

_Note: **The external signal cannot distinguish between multiple signals, therefore it should only be assigned to a single signal.**_

The settings are permanently stored when exiting the setup menu.


## Compatibility
The following list shows some 8-inch floppy drive models and the respective settings that should be correct for them.

| Model | TGxx signal | TGxx write threshold | TGxx read threshold | Head load signal | Maximum track number<sup>1</sup> | Tested<sup>2</sup> |
| ----- | ----------- | -------------------- | ------------------- | ---------------- | -------------------- | ------ |
| Mitsubishi M2896-63 | 0: Use thresholds <sup>4</sup> | 44 | 99 | 0: From [motor enable]  | 76(?) | ❓ |
| NEC FD1165 | 0: Use thresholds | 44 | 44 | 0: From [motor enable] | 76(?) | ❓ |
| Shugart SA800<br>Shugart SA801 | 0: Use thresholds <sup>3</sup> | 44 | 99 | 0: From [motor enable] <sup>4</sup> | 76(?) | ❓ |
| Shugart SA810<br>Shugart SA860 | 0: Use thresholds <sup>4</sup> | 44 | 99 | 0: From [motor enable] <sup>4</sup> | 76(?) | ❓ |
| Shugart SA850<br>Shugart SA851 | 0: Use thresholds <sup>4</sup> | 44 | 99 | 0: From [motor enable] <sup>4</sup> | 76(?) | ❓ |
| Tandon TM848-1E<br>Tandon TM848-2E | 0: Use thresholds <sup>4</sup> | 44 | 99 | 0: From [motor enable] <sup>4</sup> | 76(?) | ❓ |
| Y-E DATA YD-180 | 0: Use thresholds | 44 | 60 | 0: From [motor enable] <sup>4</sup> | 79 | ✅ |

<sup>1</sup> Actual seekable track number; track numbering starts at zero.<br>
<sup>2</sup> ✅: Settings have been successfully tested. ❓: Settings should be correct according to the manual, but haven't been tested yet.<br>
<sup>3</sup> Switching is done internally.<br>
<sup>4</sup> Switching is done internally by default; external switching is available optionally (see manual).

## Manufacturing
The schematics and PCB were designed using [KiCad](https://kicad-pcb.org/). The KiCad project is located in the [Schematics & PCB](Schematics%20&%20PCB) folder. 

### Schematic
[PDF](Schematics%20&%20PCB/50%20pin%20to%2034%20pin%20Floppy%20Adapter%20v2/50%20pin%20to%2034%20pin%20Floppy%20Adapter.pdf)

[PNG](Schematics%20&%20PCB/50%20pin%20to%2034%20pin%20Floppy%20Adapter%20v2/50%20pin%20to%2034%20pin%20Floppy%20Adapter.png)

### PCB Gerber files
[ZIP](Schematics%20&%20PCB/50%20pin%20to%2034%20pin%20Floppy%20Adapter%20v2/pcb.zip)

### PCB manufacturing
The board has a size of 100mm x 28.5mm, a minimum trace distance of 0.2mm and the minimal drill diameter is 0.4mm.
There are two layers and a total of 166 holes to drill. All holes, except for the two mounting holes, are plated.

If you are within the EU, you may want to consider choosing AISLER as your PCB manufacturer.

[PCB on AISLER](https://aisler.net/p/LBENFXQV)

### Bill of materials
| Reference | Component | Housing /<br>Footprint | Type | Quantity |
| --------- | --------- | ---------------------- | ---- | -------- |
| C1 | Capacitor 100nF 10V | 0402 | Murata GRM155R61A104JA01D<sup>5</sup> | 1 |
| C2 | Tantalum Capacitor 10µF 6.3V  | B / EIA-3528-21 | Kemet T491B106K006AT<sup>5</sup>  | 1 |
| J1 | 34 pin box header | 2 x 17 x 2.54mm | Connfly DS1013-34SSIB1-B-0<sup>5</sup> | 1 |
| J2 | 4 pin Berg connector | 1 x 4 x 2.54mm | econ connect FK03PN<sup>5</sup> | 1 |
| J3 | 50 pin box header | 2 x 25 x 2.54mm | Connfly DS1013-50SSIB1-B-0<sup>5</sup> | 1 |
| ~~J4~~<sup>6</sup> | ~~3 pin header~~ | ~~1 x 3 x 2.54mm~~ | ~~Connfly DS1021-1*3SF1-1~~<sup>5</sup> | ~~1~~ |
| J5 | 5 pin socket | 1 x 5 x 2.54mm | Connfly DS1023-1*5S21<sup>5</sup> | 1 |
| R2 | Resistor 22kOhm 1/8W 5% | 0805 | Vishay CRCW080522K0FKEA<sup>5</sup> | 1 |
| R1, R3-R4 | Resistor 1kOhm 1/8W 5% | 0805 | Vishay CRCW08051K00FKEA<sup>5</sup> | 3 |
| RN1-RN2, RN5-RN6 | Resistor array 150Ohm 1/16W 5% | 4 x 0603 (1206) convex | Panasonic EXB38V151JV<sup>5</sup> | 4 |
| SW1 | SMD Push button 6mm x 6mm | 9.5mm x 5.8mm | C&K PTS645SK50SMTR92LFS<sup>5</sup> | 1 |
| U1 | PIC18 MCU | QFN-28, 6mm x 6mm, pitch 0.65mm | Microchip PIC18F26K22-I/ML | 1 |
| U2-U3 | BCD to 7-segment decoder | SOP-16, 4.55mm x 10.3mm, pitch 1.27mm | NXP HEF4543B | 2 |
| U4 | Dual buffer | SOT-363 / SC-70-6 | ON Semi NC7WZ16P6X | 1 |
| U5-U6 | Seven segment display | 7mm x 15mm | Kingbright KCSA03-101 | 2 |
| U8 | Dual three-state buffer | VSSOP-8, 2.3mm x 2mm, pitch 0.5mm | ON Semi NC7WZ125K8X | 1 |

<sup>5</sup> Suggestion; Other types may be used. <br>
<sup>6</sup> Not populated; Pin header may be placed, when needed.

### PCB component placement
When soldering the components to the PCB, it is recommended to place them in the following order:
1. Bottom side, hot air: RN1-RN2, RN5-RN6, R2, U2, U3, U8
2. Top side, hot air: C1, C2, R1, R3, R4, U4, U1
3. Top side, soldering iron: U5, U6, SW1, J1, J3, J5, J2
4. Do not populate: J4 (place pin header when needed)


## Firmware
The software for the microcontroller was designed in [MPLAB X IDE](https://www.microchip.com/mplab/mplab-x-ide).
It is written in C and compiled with the [MPLAB XC8 compiler](https://www.microchip.com/mplab/compilers).
A programming adapter is required to transfer the firmware into the microcontroller. The programming socket J5 is designed to match the pinout of the [MPLAB PICkit 4](https://www.microchip.com/developmenttools/ProductDetails/PG164140) (and older) programmers.
The MPLAB X IDE project is located in the [Firmware](Firmware) folder. 

## FAQ
##### My drive is not listed in the compatibility list, will it work?
>**Check the manual of your drive to see if it needs any additional signals like TG43 and Head load.
If so, check if one of the available options matches the behavior required by the drive. When that is the case, the drive will very likely work.**

## Licensing
This project is licensed under the [CC BY-NC-ND 4.0 license](LICENSE).

[![Creative Commons License](https://i.creativecommons.org/l/by-nc-nd/4.0/88x31.png "Creative Commons License")](https://creativecommons.org/licenses/by-nc-nd/4.0/)
