pdm
===

This project is written in C, and is the 'brains' of my Raspberry Pi access control 
system - the web user interface part is called Pidoorman, and is written in PHP.

The Wiring Pi library [https://projects.drogon.net/raspberry-pi/wiringpi/] is used to 
read from and to the gpio pins on the Pi. An rfid card can be placed onto a reader 
connected to the gpio pins on the pi. This number is then read by the pdm application, 
and compared with the list of allowed cards at this location against a central mysql 
database. This same database is accessed by the web interface, such that cards can be
assigned to people, and allowed at various locations.

Currently, the location of the application is read in via a config file - just an ID 
so that each location (raspberry pi door) can be allowed or denied for a specific user.

Grouping will have to follow, as will offline functionality (i.e. what will happen if
the connection to the master mysql database is broken?

Currently my idea is to have some method of synchronising local mysql instances on each
pi. Perhaps a separate program will do this.

Build command: 
gcc pdm.c -o pdm -std=c99 -L/usr/local/lib -lwiringPi -lpthread `mysql_config --cflags --libs` && ./pdm
