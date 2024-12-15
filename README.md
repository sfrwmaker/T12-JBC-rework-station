This is a binary file (.hex) and source code of the soldering and rework station based on STM32F446RET6 MCU.
See detailed description on hackster.io https://www.hackster.io/sfrwmaker/soldering-and-rework-station-for-t12-and-jbc-tips-ec2c44

Revesion history
- Jun 30 2023, version 1.01.
  - Bug fixes in working mode procedures.
- Sep 05 2023, version 1.02.
  - Black screen issue at first startup fixed
  - Tip activation issue fixed
  - Added ability to backup and restore the configuragion data: parameters, tip calibration, pid parameters
- Sep 10 2023, version 1.03.
  - Fixed limited temperature display in iron tune mode: now the temperature higher than 400 degrees displayed correctly.
  - Mirror bugs fixed.
- July 03 2024, version 1.04.
  - Adjusted main view when several devices are in use. Reviewed the switching algorythm between active devices: JBC or T12 iron and Hot Iron Gun
  - Fixed bug in the iron tip activation menu
  - Updated the managemant of FLASH memory
  - In case the FLASH memory failed to read, the controller switches into flash debug mode
  - Added error message when the FLASH write error occurs.
- Aug 15 2024, version 1.05
  - Added new JBC tips to the tip list
- Sep 08 2024, version 1.06
  - Added green progress bar to the automatic calibration screen indicating how fast the iron reaches the reference temperature
- Dec 15 2024, version 1.08
  - Added "max temperature" preference menu item
  - Added support for IPS type of the display, ILI9341v
  - Implemented pre-heat phase in the calibration mode
  - Fixed incorrent ambient temperature readings issue
  - Modified rotary encoder behavior to change Fan speed by 1%
  - Fixed incorrect tip calibration flag issue
  - Fisex incorrect translate temperatures greater than maximum possible (400 or 500 depending on device type) issue
  - Fixed calibration issue in automatic calibration mode
  - Updated JBC tip list
  - Fixed issue in shutdown process of the Gun when it is not connected
  - Fixed issue the Hot Gun safety relay was tuned on before the Fan is working properly
