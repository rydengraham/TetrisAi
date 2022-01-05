# ---------------------------------------------------
#    Made By: Aaron Diep, Calvin Choi, & Ryden Graham
#
#    CMPUT 275, Winter 2020
#
#    Arduino Tetris AI
#    
#    The program runs an implementation of tetiris on the Arduino, with a server-based AI.
#    Control is done using a joystick to move and pushbuttons for rotation.
#    Pressing the joystick while the game and server is active activates the AI.
# ---------------------------------------------------

Accessories:
- Arduino Mega Board
- Tft display
- Multiple wires
- Breadboard
- USB cable
- Joystick
- Pushbuttons

Wiring instructions (identical to assignment 2 part 2 for simplicity):
Connect the Vry and Vrx pins on the joystick to the A8 and A9 ports on the Arduino, respectivey. Bend the ends of the wires attached to the Arduino down so they fit under the tft display.
Connect the tft display to the top of the Arduino, using the ground pins as reference in alignment.
Connect the ground and 5V pins on the joystick to the exposed ground and 5V pins on the Arduino not covered by the tft display.
Connect the SW pin on the joystick to digital pin 53 on the Arduino.
Connect the pushbuttons to ground and digital pins 45 and 47 on the Arduino.

Known Issues: When the gameboard reaches a high height, the AI fails to compensate well, usually resulting in a game over.

Running Instructions :
1. In the directory containing the files serialport.cpp, serialport.h, server.cpp and a server Makefile, use "make" to compile the code into an executable.

2. In the directory containing the files client.cpp and an Arduino Makefile, use "make upload" to compile the code into an executable and load it on the Arduino. Use arduino-port-select to connect the Arduino.

3. In the directory containing the server executable, run with [./server]. Be sure to run this AFTER the arduino upload is fully complete.

4. Use the joystick to manuver the piece laterally and push down to drop. Use the pushbuttons to rotate the piece. After the program and server initialize, press the pushbutton at any time to activate the server AI.

5. At game end, press the stick to reset. Rerun the AI as in step 3 to use it again. Wait a bit between resetting the game, rerunning server, and restarting the AI to give time to initialize everything.

<shared functions between client and server cpp files>
Functions are not exactly identical due to variable changes between files, but maintain the same core functions.

activeShift: Moves the current active piece over in a specified direction, with 1 = right, 2 = down, 3 = left. Function assumes that the given movement is legal as checked in canMove.
canMove: Takes in a direction for movement and gives a boolean return if that movement is legal or not. Move checks are done for each individual tile in the active tetromino. Inputs are done as a pair of corrdinates; the first beign for a horizontal movement and the second for a vertical one.Input range: (-1) to (1) for x and y, with up and right being positive.
convertCoord: Converts data stored as an integer into usable offset x-y coordinate data
rotateTile: Takes in coordinates of a tile and the position of the rotation reference block and performs a rotation. Additional inputs include the rotation direction and piece type. The server implementation has an additional parameter for the specific tile we are moving, as the server simulates multiple tetrominos at once.
runRotTest: Runs a rotation offset test following a rotation attempt
attemptRotation: Tries to use rotateTile to rotate a piece, then checks if that move caused the piece to overlap the board or go out of bounds. If possible, it corrects for this by shifting the rotated piece slightly, else it simply reverts the rotation attempt.
clearCheck: Tests the board to see if any lines should be cleared, updating score if so and shifting the stack down. The server has a similar function realClearCheck as clearCheck is used to update the temporary board state for fitness testing.
lockPiece: Writes the currentPiece location to the board. The server has a similar function lockRealPiece as lock piece is used to update the temporary board state for fitness testing.

Server Directory:

The serialport cpp and h files are unmodified from Eclass.

Included Files:
* Makefile
* serialport.cpp
* serialport.h
* server.cpp

Makefile  Targets:
make (project): Builds  the  server executable.
make  clean: Removes  all  object  files  and  executables.

server.cpp: 

Styling issues still appear when checking style due to multiline statements and directory naming.

The server has functions to communicate with the arduino, iterate over moves, and score those moves. After initializing and ingesting the board state, the server calculates moves for the current hold piece, sending the move to the client when the client finishes the previous move and itself sends the next hold piece to the server.

<Server exclusive functions>
unlockPiece: Removes the temp piece from the temp board to reset it and test another piece location.
findFit: The fitness function for the AI. Takes in a potential board state and evaluates it using a vairety of tests. The best possible move is stored in a global variable to be sent back to the client upon request.
calculateMove: Attempts possible moves and stores the resultant board state for evaluation by the fitness function.
main: Runs the communication loop and

Client Directory:

Included Files:
* Makefile
* client.cpp

Makefile  Targets:
make (project): Builds  the  Arduino executable.
make  clean: Removes  all  object  files  and  executables.

client.cpp: 

Styling issues still appear when checking style due to multiline statements.

<Client exclusive functions>
setup: Initializes variables and hardware components.
getNext: Generates the next piece, with a 3 gap between identical pieces.
drawBlock: Draws a given block on the tft, for given coordinates.
moveAttempt: Checks if the stick is being moved, bool return.
updateScore: Increases score on line clear and updates display.
processJoystick: Runs user interaction for moves.
tetris: The main loop, handiling gameplay and serial communication.
main: Runs tetris and resets variables at end of game.
