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
