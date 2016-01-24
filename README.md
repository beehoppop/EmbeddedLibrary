Release Notes
=============
1/24/2016 - 0.2.0 - General Stabilization and API improvements
  - The EmbeddedLibrary has now been deployed on two complicated systems that are used daily. (One is at https://github.com/beehoppop/FrontHouseLEDProject)
  - Command system has been generalized to handle input from any source (currently a serial port and the CAN bus)
  - Removed dependency on external Sparkfun library
  - Brought CAN bus library back to life, commands can now be issued over the CAN bus (more testing is still needed)
  - The module system now does most of the heavy lifting for saving and restoring settings from EEPROM
  - Improved time change handling by providing a mechanism to register when the time has been changed
  - Config vars can now be registered to leverage the existing mechanisms to set and get persistent global config settings
  - Improvements to the debug output system so debug messages generating during command execution will go to the output of that command source
  - Bug fixes

12/28/15/ - 0.1.0 - First pre-release

Embedded Library
========================================
A framework for developing moderate to very complex ARM based Arduino sketches in C++. At the core of the framework is the 
concept of modules. All functionality is provided through modules allowing sketches to include what it
needs. Each module can have the following:

 - Its own EEPROM space that is managed by the framework
 - Periodic updates at a specified interval
 - Initialization done according to priorities to allow correct module initialization ordering
 
Another core concept is being event based. The sketch registers event handlers to implement its unique
functionality. An event handler is just a method on a C++ object. For example when a digital
IO pin becomes active the user's registered event handler is called for that pin. The framework handles the details
of configuring the IO pin, managing the state, and handling debouncing.
 
In addition to the module and event systems this library provides the following functionality:
 - A unified real world time system that supports time providers such as the DS3234, understands time zones, and supports
	real world time based alarms and periodic time based events.
 - A module for computing sunrise and sunset times as well as calling event handlers for sunset and sunrise events
 - Handling of commands coming over any input source in a similar fashion to linux command line tools with
	arguments being broken up and passed into an event handler as an array of c strings.
 - A Digital IO module that handles debouncing and calling a specified event on both active and deactive events. Also
	supports setting an output to an active state for a specified period of time before returning to the 
	deactive state.
 - A touch module for the Teensyduino analog touch input pins.
 - A simple config value module for setting and getting persistent global config data.
 - A debugging facility for asserts and reporting errors to the user over the serial port
 - A CANBus event handling module
 - A module for the Sparkfun TSL2561 luminosity sensor

In Development:
 - A generalized internet device system
 - ESP8266 support
 - More thorough examples

Repository Contents
-------------------

A zip file of the library that can easily be installed as described below

Installation Instructions
-------------------------

Extract the contents of the zip file and place into your Documents/Arduino/libraries/ folder. Rename the enclosed
folder from "EmbeddedLibrary-master" to just "EmbeddedLibrary".

Documentation
--------------

An example sketch is provided and the headers files have lots of comments describing the API
calls.

A Few Notes
-----------
My development environment is as follows:
 - Windows 7 64-bit
 - Visual Studio 2015 Community Edition (https://www.visualstudio.com/en-us/products/visual-studio-community-vs.aspx), it's free.
 - Visual Micro, also free (http://www.visualmicro.com/)
 - Visual Assist X (http://www.wholetomato.com/) which is not free but well worth the $99.
 - Teensyduino 3.x
 
I mention this because this library is relatively new and has not been tested outside of this setup. 

Please send feedback to embeddedlibraryfeedback@gmail.com. Thanks!

License Information
-------------------

This product is _**open source**_ based on the MIT license.

Please use, reuse, and modify these files as you see fit. Please maintain attribution to Brent Pease.

Distributed as-is; no warranty is given.
