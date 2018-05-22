Welcome to the IWIDarwin project.

The following installer will install the following software:

1. iwifi.kext - Driver for the Intel Pro Wireless 3945, 4965, 5150, 5350, 6x00, 6x50, 1000 Family of wireless network adaptors.
2. nsGUI.app - Application which should control and allow the user to turn the card on/off - not available yet

INSTALL:
if this is your first install wait for mac os x to detect a new interface
it will ask you to open system preferences - networks open it and press apply

to remove the software:
. boot with -x
. delete iwifi.kext
. reboot

Known Bugs:
the driver is expected to give kernel panics
if the driver don't detect the interface check your extensions and update

Currently Supported:
- 2200bg / 2915abg

Drivers in Development:
- 2100
- 3945abg

Requirement:
- Mac OS X 10.4.7
