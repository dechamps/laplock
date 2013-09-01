laplock
=======

Author: Etienne Dechamps (a.k.a e-t172) <etienne@edechamps.fr>

https://github.com/dechamps/laplock

This extremely small (102 lines, 21KB binary) C++ program allows you to automatically lock your Windows computer on two events:
 - The computer is a laptop and its lid is closed;
 - The computer screen is turned off. Note that this probably won't work with the power button of your monitor since the system is not notified when this happens; however, it works when the screen is turned off automatically after the delay specified in the Windows power management options.

laplock runs on Windows Vista, 7, and probably later versions; it won't work on Windows XP, since it doesn't implement the necessary power management interface.

Compilation
-----------

There is only one C++ file and it should compile using any Windows compiler. Project files are provided for Microsoft Visual C++ 2010. Note that you'll need the Windows SDK version 7.0 or greater.

Usage
-----

Just run it. At first, nothing will happen; this is normal as laplock runs silently in the background (you can check this using the Windows task manager). laplock will keep running until you log off or kill it. It is recommended to add laplock to your Startup folder; then you can happily forget about it.

Note that laplock listens intelligently for events; meaning, it doesn't consume any CPU *at all* while waiting.

