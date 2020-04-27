# hangman_puzzle
Arduino code to control an escape room hangman puzzle. The puzzle has
two parts to it. First, the team collects six cubes and places them
in receptacles to spell a name. The cubes contain RFID tags which are
read by six RFID readers. When the cubes have been placed correctly
a solenoid is activated which unlocks the actual hangman puzzle.
This consists of a custom "keyboard" which has the correct buttons
hardwired to generate specific voltages on an output line. All
incorrect buttons yield the same voltage. When the game is "won"
it is simply reset, when it is "lost" a hatch opens to hang the
man and the game is over. It is then reset by removing the letter cubes.

External hardware like lights and solenoids are controlled using two
serial relay modules, each controlling eight relays.
The RFID readers share all connections except MISO (using an idea
from https://arduino.stackexchange.com/a/38156).

The Arduino pin usage is as follows:

A0: analog button input

A1-A5: all connections except MISO on all the RFID readers

2-4, 5-7: connections to the relay modules

8-13: connected to the individual RFID readers' MISO

The correct RFID tags are programmed into the system by using a hardcoded
master tag in the position to be programmed, then the correct tags (up to
two), and finally the master tag again. The correct tag ids are stored
in EEPROM in order to be persistant.

The rfid code is taken from sunfounder.com's tutorials.
