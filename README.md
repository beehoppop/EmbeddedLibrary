Embedded Library
========================================
A framework for developing moderate to very complex ARM based Arduino sketches. At the core of the framework is
the concept of Modules. All functionality is provided through modules allowing sketches to only include what it
needs. Each module can have the follwing:

 - Its own EEPROM space that is managed by the framework
 - Periodic updates at a specified interval
 - Initialization done according to priorities to allow correct module initialization ordering
 
Another core concept is being event based. The sketch registers event handlers to implement the unique
functionality of the sketch. An event handler is just a method on a C++ object. For example when a digital
IO pin becomes active a specific event handler is called. The framework handles the details of configuring
the IO pin, managing the state, and handling debouncing.
 
In addition to the module system this library provides the following functionality:
 - Handling of commands coming over the serial port in a similar fashion to linux command line tools with
	arguments being broken up and pased into an event handler as an array of c strings.
 - A real time system that supports time providers such as the DS3234, understands time zones, and supports
	real world time based alarms and periodic time based events.
 - A Digital IO module that handles debouncing and calling a specified event on both active and deactive events. Also
	supports setting an output to an active state for a specified period of time before returning to the 
	deactive state.
 - A touch module for the Teensyduino analog touch input pins.
 - A simple config value module for setting and getting persistent global config data.
 - A module for computing sunrise and sunset times as well as calling event handlers for sunset and sunrise events
 - A debugging facility for asserts and reporting errors to the user
 - A CANBus event handling modules (still in active development and not fully tested yet)
 - A module for the Sparkfun TSL2561 luminosity sensor

Repository Contents
-------------------

A zip file of the library that can easily be installed as described below

Installation Instructions
-------------------------

Extract the contents of the zip file and place into your Documents/Arduino/libraries/ folder

Documentation
--------------

An example sketch is provided and the headers files have lots of comments descriping the API
calls.

License Information
-------------------

This product is _**open source**_ based on the MIT license.

Please use, reuse, and modify these files as you see fit. Please maintain attribution to Brent Pease and release anything derivative under the same license.

Distributed as-is; no warranty is given.
