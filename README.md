![alt text](https://github.com/MrBuggySan/UofC_SolarCar_MPPT/blob/master/SolarInAction.jpg)



# [MPPT](https://en.wikipedia.org/wiki/Maximum_power_point_tracking) used by [University of Calgary Solar Car Team](http://www.calgarysolarcar.ca/) in [FSGP 2015](http://americansolarchallenge.org/)

This MPPT design is a newer version of the one used in [WSC 2013](https://www.worldsolarchallenge.org/). Off the shelf MPPTs usually cost around $1000. To save money, the team assigned me to make a custom MPPT for them. In the end the parts cost summed to $150 and it performed as expected! 

Features:
- Easy to use [LPC11C24](http://www.nxp.com/products/microcontrollers-and-processors/arm-processors/lpc-cortex-m-mcus/lpc1100-cortex-m0-plus-m0/scalable-entry-level-32-bit-microcontroller-mcu-based-on-arm-cortex-m0-plus-m0-cores:LPC11C24FBD48)
- A new algorithm that takes care of device inefficiencies due to clouds covering the solar cells 
- 98% maximum efficiency in static conditions 
- New testing modes (Static Voltage, Dynamic Trace, IV Trace, Open)
- 5 LEDs showing different status
- Fuses at the input and output of the device
- Working [CAN](https://en.wikipedia.org/wiki/CAN_bus) messages 

The altium files used to create the PCBs are also [here](https://github.com/MrBuggySan/UofC_SolarCar_MPPT/tree/master/Altium)

The C code is [here](https://github.com/MrBuggySan/UofC_SolarCar_MPPT/tree/master/src)

[Top view](https://github.com/MrBuggySan/UofC_SolarCar_MPPT/blob/master/TopView.jpg)

[Bottom view](https://github.com/MrBuggySan/UofC_SolarCar_MPPT/blob/master/BottomView.jpg)

[Schematics](https://github.com/MrBuggySan/UofC_SolarCar_MPPT/blob/master/MPPTSchematic.PNG)

[3D View from Altium](https://github.com/MrBuggySan/UofC_SolarCar_MPPT/blob/master/MPPTBoard.PNG)


