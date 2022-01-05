// # ---------------------------------------------------
// #    Made By: Aaron Diep, Calvin Choi, & Ryden Graham
// #
// #    CMPUT 275, Winter 2020
// #
// #    Arduino Tetris AI
// #
// #    The program runs an implementation of
// #    tetiris on the Arduino,with a
// #    server-based AI.
// #    Control is done using a joystick to move and
// #    pushbuttons for rotation.
// #    Pressing the joystick while the game and
// #    server is active activates the AI.
// # ---------------------------------------------------

#include <Arduino.h>
// core graphics library (written by Adafruit)
#include <Adafruit_GFX.h>

// Hardware-specific graphics library for MCU Friend 3.5" TFT LCD shield
#include <MCUFRIEND_kbv.h>
// LCD and SD card will communicate using the Serial Peripheral Interface (SPI)
// e.g., SPI is used to display images stored on the SD card
#include <SPI.h>

// needed for reading/writing to SD card
#include <SD.h>

#include <string.h>

#include <TouchScreen.h>
using namespace std;

#define JOY_CENTER	 512
#define JOY_DEADZONE 64

// many, many constant declarations
#define SD_CS 10
#define JOY_VERT A9  // should connect A9 to pin VRx
#define JOY_HORIZ A8  // should connect A8 to pin VRy
#define JOY_SEL	 53
#define CLOCKWISE_BUTTON 47
#define COUNTER_BUTTON 45

// physical dimensions of the tft display (# of pixels)
#define DISPLAY_WIDTH	480
#define DISPLAY_HEIGHT 320

// thresholds to determine if there was a touch
#define MINPRESSURE	 10
#define MAXPRESSURE 1000

MCUFRIEND_kbv tft;

// Communication States
enum States {
	InitialSend, WaitingForAck, SendingPiece, Error, ProcessingPiece
};

// Global game board array (0,0 = bottom/left)
int tiles[10][20] = {0};

// colour array: black, cyan, blue, orange, yellow, green, purple, red, white.
const int32_t colors[9] = {0x0000, 0x07FF, 0x001F, 0xFA60, 0xFFE0,
						   0x07E0, 0xF81F, 0xF800, 0xFFFF};

// score vars
int score = 0;
int combo = 0;
int level = 0;
int linesCleared = 0;
uint32_t speedUp = 800;

// offset vars
int offTotalX, offTotalY;

// rotation data
int JLSTZoffset[5][4];
int Ioffset[5][4];

// block setup: block coordinates represent relative position
// of each block in the tetromino, wrt the previous position.
// base block is the tile at [4][20] on the play field
// 0-3: locations on row 20; 4-7: locations on row 19
// Blocks: I, J, L, O, S, T, Z
const int tetromino[7][4] = {{5, 4, 6, 7},  // I
							{6, 5, 1, 7},   // J
							{6, 5, 7, 3},   // L
							{5, 2, 1, 6},   // O
							{6, 5, 2, 3},   // S
							{6, 5, 2, 7},   // T
							{6, 2, 1, 7}};  // Z

// current tile vars
// stores each block ([i][0] = x coordinate, [i][1] = y coordinate)
int currentPiece[4][2];
int beforeRot[4][2];
int currentColour;
int currentRotIndex = 0;
bool activePiece = false;
int randomNums[7] = {3, 3, 3, 3, 3, 3, 3};

// timer locks
int aiLock = 0;
int shiftLock = 0;
int rotLock = 0;
uint32_t fallTimer = millis();


// setup
void setup() {
	init();
	// seed rng
	randomSeed(analogRead(15));
	// burn first number (required)
	random();
	Serial.begin(9600);
	pinMode(JOY_SEL, INPUT_PULLUP);
	// initialize button pins
	pinMode(CLOCKWISE_BUTTON, INPUT_PULLUP);
	pinMode(COUNTER_BUTTON, INPUT_PULLUP);

	// rotation matrix data used for piece rotation
	// storage legend - x -> 0s digit, y -> 10s digit
	// -2 -> 1, -1 -> 2, 0 -> 3, 1 -> 4, 2 -> 5
	JLSTZoffset[0][0] = 33;
	JLSTZoffset[0][1] = 33;
	JLSTZoffset[0][2] = 33;
	JLSTZoffset[0][3] = 33;

	JLSTZoffset[1][0] = 33;
	JLSTZoffset[1][1] = 43;
	JLSTZoffset[1][2] = 33;
	JLSTZoffset[1][3] = 23;

	JLSTZoffset[2][0] = 33;
	JLSTZoffset[2][1] = 42;
	JLSTZoffset[2][2] = 33;
	JLSTZoffset[2][3] = 22;

	JLSTZoffset[3][0] = 33;
	JLSTZoffset[3][1] = 35;
	JLSTZoffset[3][2] = 33;
	JLSTZoffset[3][3] = 35;

	JLSTZoffset[4][0] = 33;
	JLSTZoffset[4][1] = 45;
	JLSTZoffset[4][2] = 33;
	JLSTZoffset[4][3] = 25;

	// I piece data
	Ioffset[0][0] = 33;
	Ioffset[0][1] = 23;
	Ioffset[0][2] = 24;
	Ioffset[0][3] = 34;

	Ioffset[1][0] = 23;
	Ioffset[1][1] = 33;
	Ioffset[1][2] = 44;
	Ioffset[1][3] = 34;

	Ioffset[2][0] = 53;
	Ioffset[2][1] = 33;
	Ioffset[2][2] = 14;
	Ioffset[2][3] = 34;

	Ioffset[3][0] = 23;
	Ioffset[3][1] = 34;
	Ioffset[3][2] = 43;
	Ioffset[3][3] = 32;

	Ioffset[4][0] = 53;
	Ioffset[4][1] = 31;
	Ioffset[4][2] = 13;
	Ioffset[4][3] = 35;

	// tft display initialization
	uint16_t ID = tft.readID();
	tft.begin(ID);
	tft.setRotation(0);
	tft.fillScreen(TFT_BLACK);
	tft.drawLine(241, 0, 241, 480, colors[8]);
}

int getNext() {
	/*
		Generates a number that dictates which piece is to be spawned in
		with a 3 roll gap between identical pieces.

		Arguments:
			N/A

		Return:
			int temp: The id number of the next piece

	*/
	int temp;
	bool done = false;
	// check if it has been 3 rolls since last piece
	while (!done) {
		temp = random(7);
		if (randomNums[temp] == 3) {
			done = true;
		}
	}
	// increase rolls counter
	for (int i = 0; i < 7; i ++) {
		if (randomNums[i] < 3) {
			randomNums[i] += 1;
		}
	}
	// return piece id
	randomNums[temp] = 0;
	return temp;
}

void drawblock(int x, int y) {
	/*
		Draws a block on the TFT based on inputed x and y with a black border

		Arguments:
			int x: x coordinate
			int y: y coordinate

		Return:
			N/A

	*/
	tft.fillRect(x*24, 456 - y*24, 24, 24, colors[tiles[x][y]]);
	tft.drawRect(x*24, 456 - y*24, 24, 24, colors[0]);
}

bool moveAttempt(int X, int Y) {
	/*
		Check if joystick is being moved.

		Arguments:
			int X: joystick x coordinate
			int Y: joystick y coordinate

		Return:
			bool: true if is moving else false

	*/
	if (abs(X-JOY_CENTER) > JOY_DEADZONE || abs(Y-JOY_CENTER) > JOY_DEADZONE) {
		return true;
	}
	return false;
}

bool canMove(int directionX, int directionY) {
	/*
		Checks if movement is within playgrid. (1 = right/up, -1 = left/down)

		Arguments:
			int directionX: check if can move left/right
			int directionY: check if can move up/down

		Return:
			bool: true if can move else false

	*/
	int tempX;
	int tempY;
	for (int i = 0; i < 4; i ++) {
		tempX = currentPiece[i][0];
		tempY = currentPiece[i][1];
		if (tempX+directionX < 0 || tempX+directionX > 9|| tempY+directionY < 0
			|| tempY+directionY > 19 || tiles[tempX+directionX][tempY+directionY] != 0) {
			return 0;
		}
	}
	return 1;
}

// moves the active piece can move in a given direction
// intput: (int) direction: the direction to move
// 1 = right, 2 = down, 3 = left, 4 = up
// void return
void activeShift(int direction) {
	/*
		Move the current piece in the selected direction
		(1 = right, 2 = down, 3 = left, 4 = up)

		Arguments:
			int direction: the direction to move the piece

		Return:
			N/A

	*/
	int tempX;
	int tempY;
	// reset movement timer
	shiftLock = 2500;
	// right
	if (direction == 1) {
		for (int i = 0; i < 4; i ++) {
			tempX = currentPiece[i][0];
			tempY = currentPiece[i][1];
			tft.fillRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
			tempX++;
			currentPiece[i][0] = tempX;
		}
	// left
	} else if (direction == 3) {
		for (int i = 0; i < 4; i ++) {
			tempX = currentPiece[i][0];
			tempY = currentPiece[i][1];
			tft.fillRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
			tempX--;
			currentPiece[i][0] = tempX;
		}
	// down
	} else if (direction == 2) {
		shiftLock = 0;
		for (int i = 0; i < 4; i ++) {
			tempX = currentPiece[i][0];
			tempY = currentPiece[i][1];
			tft.fillRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
			tempY--;
			currentPiece[i][1] = tempY;
		}
	}
	// draw piece in new location
	for (int i = 0; i < 4; i++) {
		tempX = currentPiece[i][0];
		tempY = currentPiece[i][1];
		tft.fillRect(tempX*24, 456 - tempY*24, 24, 24, colors[currentColour]);
		tft.drawRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
	}
}

void updateScore() {
	/*
		Calculate and display score.
		Also updates game speed every 10 line clears

		Arguments:
			N/A

		Return:
			N/A

	*/
	// erase previous score
	tft.fillRect(250, 130, 60, 80, colors[0]);
	// award extra points for tetris and consecutive tetris
	if ((combo % 4) != 0) {
		score += (combo % 4)*100;
		linesCleared += (combo % 4);
		combo = 0;
	} else if (combo == 4) {
		score += 800;
		linesCleared += 4;
	} else {
		score += 1200;
		linesCleared += 4;
	}
	tft.setCursor(255, 140);
	tft.println(score);
	// check lines cleared and update game drop speed
	level = linesCleared/10;
	if (level < 9) {
		speedUp = (48 - level*5)*100/6;
	} else if (level < 27) {
		speedUp = (9 - level/3)*100/6;
	} else {
		speedUp = 100/6;
	}
}

void clearCheck() {
	/*
		Check if any lines need to be cleared

		Arguments:
			N/A

		Return:
			N/A

	*/
	bool shift = false;
	int shiftLevel = 1;
	int lowest = 20;
	int unique[4] = {-1, -1, -1, -1};
	bool clear = false;
	int temp;
	// check which lines to test from placed piece
	for (int i = 0; i < 4; i ++) {
		temp = currentPiece[i][1];
		if (unique[0] != temp && unique[1] != temp &&
				unique[2] != temp && unique[3] != temp) {
			unique[i] = currentPiece[i][1];
		}
	}
	// test each line on the playgrid
	for (int i = 0; i < 4; i ++) {
		if (unique[i] != -1) {
			clear = true;
			for (int j = 0; j < 10; j ++) {
				if (tiles[j][unique[i]] == 0) {
					clear = false;
				}
			}
			// remove cleared lines
			if (clear) {
				if (lowest > unique[i]) {
					lowest = unique[i];
				}
				clear = false;
				shift = true;
				for (int j = 0; j < 10; j ++) {
					tft.fillRect(j*24, 456 - unique[i]*24, 24, 24, colors[0]);
				}
				unique[i] += 20;
			}
		}
	}
	// if any lines are cleared move blocks down
	if (shift) {
		// shift down blocks
		for (int i = lowest + 1; i < 20; i ++) {
			if (unique[0] - 20 != i && unique[1] - 20 != i &&
				unique[2] - 20 != i && unique[3] - 20 != i) {
				for (int j = 0; j < 10; j ++) {
					tiles[j][i - shiftLevel] = tiles[j][i];
					drawblock(j, i - shiftLevel);
				}
			} else {
				shiftLevel++;
			}
		}
		combo += shiftLevel;
		updateScore();
		// fill in previous location
		for (int i = 20 - shiftLevel; i < 20; i++) {
			for (int j = 0; j < 10; j ++) {
				tiles[j][i] = 0;
				tft.fillRect(j*24, 456 - i*24, 24, 24, colors[0]);
			}
		}
	}
}

void lockPiece() {
	/*
		Locks the current piece in the playgrid

		Arguments:
			N/A

		Return:
			N/A

	*/
	for (int i = 0; i < 4; i++) {
		tiles[currentPiece[i][0]][currentPiece[i][1]] = currentColour;
	}
	activePiece = false;
	// check if any lines are cleared
	clearCheck();
}

int convertCoord(int num, bool isX) {
	/*
		Converts coordinates into usable rotation offsets

		Arguments:
			num (short): number to convert
			isX (bool): number is an x-coordinate

		Return:
			int convertedCoord: the converted coordinate

	*/
	if (isX) {
		return (num / 10) - 3;
	} else {
		return (num % 10) - 3;
	}
}

void rotateTile(int xCoord, int yCoord, int refX,
				int refY, int clockRot, int tileIndex) {
	/*
		Rotates individual tiles.

		Parameters:
			xCoord (int): x-coordinate of tile to rotate
			yCoord (int): y-coordinate of tile to rotate
			refX (int): x-coordinate of pivot tile
			refY (int): y-coordinate of pivot tile
			clockRot (int): clockwise or not [1 for CW]
			tileIndex (int): index of tile in piece
		
		Return:
			N/A

	*/
	// variable declaration
	int relativeX = xCoord - refX;
	int relativeY = yCoord - refY;
	int newX;
	int newY;
	// check for CW or CCW
	if (clockRot == 1) {
		newX = relativeY;
		newY = -relativeX;
	} else {
		newX = -relativeY;
		newY = relativeX;
	}
	newX += refX;
	newY += refY;
	// store into globals
	int tempX = currentPiece[tileIndex][0];
	int tempY = currentPiece[tileIndex][1];
	beforeRot[tileIndex][0] = tempX;
	beforeRot[tileIndex][1] = tempY;
	currentPiece[tileIndex][0] = newX;
	currentPiece[tileIndex][1] = newY;
}

void runRotTest(int blockType, int oldRotIndex,
				 int newRotIndex, int testNum) {
	/*
		Runs rotation offset tests.

		Parameters:
			blockType (int): Indicates the type of piece
			oldRotIndex (int): Rotation index before rotation performed
			newRotIndex (int): Rotation index after rotation performed
			testNum (int): Which test is being used (5 tests total)

		Return:
			N/A

	*/
	// I block test
	if (blockType == 1) {
		offTotalX = convertCoord(Ioffset[testNum][oldRotIndex], true)
				  - convertCoord(Ioffset[testNum][newRotIndex], true);
		offTotalY = convertCoord(Ioffset[testNum][oldRotIndex], false)
				  - convertCoord(Ioffset[testNum][newRotIndex], false);
	// everything else test
	} else {
		offTotalX = convertCoord(JLSTZoffset[testNum][oldRotIndex], true)
				  - convertCoord(JLSTZoffset[testNum][newRotIndex], true);
		offTotalY = convertCoord(JLSTZoffset[testNum][oldRotIndex], false)
				  - convertCoord(JLSTZoffset[testNum][newRotIndex], false);
	}
}

void attemptRotation(int clockwise, bool doOffset) {
	/*
		Attempts to rotate a piece.

		Parameters:
			clockwise (int): Indicates if rotation is CW or CCW [1 for CW, -1 for CCW]
			doOffset (bool): Indicate if offset tests should be run

		Return:
			N/A

	*/
	// variable declaration
	int blockType;
	int oldRotIndex = currentRotIndex;
	int tempX;
	int tempY;
	int befX;
	int befY;
	String inLine;
	currentRotIndex += clockwise;
	// 4 is amount of possibl rot. indices
	currentRotIndex = (currentRotIndex % 4 + 4) % 4;

	// perform rotation for each tile
	for (int i = 0; i < 4; ++i) {
		rotateTile(currentPiece[i][0], currentPiece[i][1],
				   currentPiece[0][0], currentPiece[0][1], clockwise, i);
	}
	// stop if no offset necessary
	if (!doOffset) {
		for (int i = 0; i < 4; ++i) {
			int tempX = currentPiece[i][0];
			int tempY = currentPiece[i][1];
			// draw on board
			tft.fillRect(tempX*24, 456 - tempY*24, 24, 24, colors[currentColour]);
			tft.drawRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
		}
		return;
	}
	// determine piece type
	if (colors[currentColour] == 0x07FF) {  // cyan = I piece
		blockType = 1;
	} else if (colors[currentColour] == 0xFFE0) {  // yellow = O piece
		attemptRotation(-clockwise, false);
		return;
	} else {  // other
		blockType = 2;
	}
	// run offset tests
	for (int i = 0; i < 5; ++i) {
		runRotTest(blockType, oldRotIndex, currentRotIndex, i);
		// check if can move
      	if(canMove(offTotalX, offTotalY)) {
        	for (int j = 0; j < 4; ++j) {
          	// apply offset
          	currentPiece[j][0] += offTotalX;
          	currentPiece[j][1] += offTotalY;
          	// erase previous position
          	befX = beforeRot[j][0];
          	befY = beforeRot[j][1];
          	tft.fillRect(befX*24, 456 - befY*24, 24, 24, colors[0]);
        	}
        	for (int j = 0; j < 4; ++j) {
          	// draw new position
          	tempX = currentPiece[j][0];
          	tempY = currentPiece[j][1];
          	tft.fillRect(tempX*24, 456 - tempY*24, 24, 24,
          				 colors[currentColour]);
          	tft.drawRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
        }
        return;
      }
	}
	// rotate backwards if all offset tests fail
	attemptRotation(-clockwise, false);
}


void processJoystick() {
	/*
		Evaluate joystick movement

		Parameters:
			N/A

		Return:
			N/A

	*/
	// joystick inputs
	int xVal = analogRead(JOY_HORIZ);
	int yVal = analogRead(JOY_VERT);
	int buttonVal = digitalRead(JOY_SEL);
	// check move
	if (moveAttempt(xVal, yVal)) {
		// check direction, move if able
		if (yVal < JOY_CENTER - JOY_DEADZONE) {
		} else if ((yVal > JOY_CENTER + JOY_DEADZONE)) {
			if (canMove(0, -1)) {
				activeShift(2);
			} else {
				// hit the bottom
				lockPiece();
			}
		} else if ((xVal > JOY_CENTER + JOY_DEADZONE) && canMove(-1, 0)) {
			activeShift(3);
		} else if ((xVal < JOY_CENTER - JOY_DEADZONE) && canMove(1, 0)) {
			activeShift(1);
		}
	}
}

// main loop
void tetris() {
	/*
		Run Tetris game

		Parameters:
			N/A

		Return:
			N/A

	*/
	// var dec
	bool gameActive = true;
	int tempX;
	int tempY;
	int next;
	int nextX;
	int nextY;
	int temp;
	int joyVal = digitalRead(JOY_SEL);
	bool aiActive = false;
	String strTemp;
	States clientState = InitialSend;

	// AI variables
	int moveInstr;
	String strInstr;
	int remActions = 0;
	bool moveLeft = false;
	int rots = 0;

	// set up side bar
	tft.setCursor(255, 10);
	tft.setTextSize(2);
	tft.print("NEXT:");
	tft.setCursor(253, 110);
	tft.print("SCORE");
	tft.setCursor(255, 140);
	tft.println(score);

	// spawn starting piece
	activePiece = true;
	temp = getNext();
	currentRotIndex = 0;
	currentColour = temp + 1;
	// create piece & draw it
	for (int i = 0; i < 4; i ++) {
		tempX = tetromino[temp][i]%4 + 3;
		tempY = (tetromino[temp][i] < 4) + 18;
		// set current piece x and y coordinate
		currentPiece[i][0] = tempX;
		currentPiece[i][1] = tempY;
		// draw piece on TFT
		tft.fillRect(tempX*24, 456 - tempY*24, 24, 24, colors[currentColour]);
		tft.drawRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
	}
	// spawn next piece
	next = getNext();
	for (int i = 0; i < 4; i ++) {
		nextX = tetromino[next][i]%4 + 3;
		nextY = (tetromino[next][i] < 4) + 18;
		// draw piece in next piece box on TFT
		tft.fillRect(nextX*12 + 220, 270 - nextY*12, 12, 12, colors[next + 1]);
		tft.drawRect(nextX*12 + 220, 270 - nextY*12, 12, 12, colors[0]);
	}
	// check if game over
	while (gameActive) {
		// check if joystick is pushed activating AI
		joyVal = digitalRead(JOY_SEL);
		// set current piece to next piece after placement
		if (!activePiece) {
			activePiece = true;
			currentRotIndex = 0;
			currentColour = next + 1;
			// create piece & draw it
			for (int i = 0; i < 4; i ++) {
				tempX = tetromino[next][i]%4 + 3;
				tempY = (tetromino[next][i] < 4) + 18;
				currentPiece[i][0] = tempX;
				currentPiece[i][1] = tempY;
				tft.fillRect(tempX*24, 456 - tempY*24, 24, 24, colors[currentColour]);
				tft.drawRect(tempX*24, 456 - tempY*24, 24, 24, colors[0]);
			}
			// generate new next piece
			next = getNext();
			tft.fillRect(245, 30, 60, 60, colors[0]);
			for (int i = 0; i < 4; i ++) {
				nextX = tetromino[next][i]%4 + 3;
				nextY = (tetromino[next][i] < 4) + 18;
				tft.fillRect(nextX*12 + 220, 270 - nextY*12, 12, 12, colors[next + 1]);
				tft.drawRect(nextX*12 + 220, 270 - nextY*12, 12, 12, colors[0]);
			}
		}
		// check if joystick was pushed recently and turn on AI
		if (aiLock == 0) {
			if (joyVal == 0 && !aiActive) {
				aiActive = true;
				strTemp = "I ";
				// concatenate tile data to send to server
				for (int i = 0; i < 200; ++i) {
					strTemp += String(tiles[i%10][i/10]);
				}
				Serial.println(strTemp);
				clientState = WaitingForAck;
				// wait for acknowledgement from server
				while (clientState == WaitingForAck) {
					Serial.setTimeout(50);
					strTemp = Serial.readString();
					if (strTemp[0] == 'A') {
						clientState = SendingPiece;
					}
				}
				// sending current piece x and y coordinates to server
				strTemp = "C ";
				for (int i = 0; i < 4; ++i) {
					strTemp += String(currentPiece[i][0]);
					strTemp += " ";
					strTemp += String(currentPiece[i][1]);
					strTemp += " ";
				}
				strTemp += currentRotIndex;
				Serial.println(strTemp);
				clientState = WaitingForAck;
				// waiting for acknowledgement from server
				while (clientState == WaitingForAck) {
					Serial.setTimeout(50);
					strTemp = Serial.readString();
					if (strTemp[0] == 'A') {
						clientState = SendingPiece;
					}
				}
			} else if (joyVal == 0) {
				// turn off AI
				aiActive = false;
			}
			aiLock = 1000;
		} else {
			aiLock--;
		}
		// Check if AI is off and evaluate player movement
		if (!aiActive) {
			// check player movement
			if (shiftLock == 0) {
				processJoystick();
			} else {
				// reduce movement cooldown
				shiftLock--;
			}
			if (activePiece) {
				// rotation check
				if(rotLock == 0) {
					if (digitalRead(CLOCKWISE_BUTTON) == LOW) {
						// 1 is for clockwise
						attemptRotation(1, true);
						rotLock = 500;
					} else if (digitalRead(COUNTER_BUTTON) == LOW) {
						// -1 is for counterclockwise
						attemptRotation(-1, true);
						rotLock = 500;
					}
				} else {
					// reduce rotation cooldown
					rotLock--;
				}
				// apply gravity to block
				if (fallTimer + speedUp <= millis()) {
					fallTimer = millis();
					if (canMove(0, -1)) {
						activeShift(2);
					} else {
						lockPiece();
					}
				}
			}
		// If AI is active
		} else {
			// send next piece ID
			if (clientState == SendingPiece) {
				strTemp = "R " + String(next);
				Serial.println(strTemp);
				clientState = WaitingForAck;
			} else if (clientState == WaitingForAck) {
				// recieve movement instructions from server
				Serial.setTimeout(50);
				strTemp = Serial.readString();
				if (strTemp[0] == 'A') {
					strInstr = "";
					clientState = ProcessingPiece;
					// check if moving left or right and store instruction
					if (strTemp[2] == '-') {
						moveLeft = true;
						for (int i = 0; i < 3; ++i) {
							strInstr += strTemp[2+i];
						}
					} else {
						moveLeft = false;
						for (int i = 0; i < 2; ++i) {
							strInstr += strTemp[2+i];
						}
					}
					moveInstr = strInstr.toInt();
				}
			} else if (clientState == ProcessingPiece) {
				// emulate move
				for (int i = 0; i < abs(moveInstr)%10; i++) {
					// rotate according to server instruction (ones digit)
					attemptRotation(1, true);
				}
				// move according to server instruction (tens digit)
				for (int i = 0; i < abs(moveInstr/10); i++) {
					// check if joystick is pushed to stop AI
					joyVal = digitalRead(JOY_SEL);
					if (joyVal == 0) {
						aiActive = false;
						aiLock = 1000;
					}
					// if tens digit = 9 no movement
					if (moveInstr/10 != 9) {
						if (moveInstr > 0 && canMove(1, 0)) {
							activeShift(1);
						} else if (canMove(-1, 0)) {
							activeShift(3);
						}
					}
				}
				// drop piece
				while (canMove(0, -1)) {
					joyVal = digitalRead(JOY_SEL);
					if (joyVal == 0) {
						aiActive = false;
						aiLock = 1000;
					}
					activeShift(2);
				}
				lockPiece();
				// send next piece
				clientState = SendingPiece;
			}
		}
		// End Game
		for (int i = 0; i < 10; i++) {
			if (tiles[i][19] != 0) {
				gameActive = false;
				if (aiActive) {
					Serial.println("X\n");
				}
			}
		}
	}
}

// main
int main() {
	setup();
	// main loop
	while (true) {
		// rungame
		tetris();
		// predd joystick after loss to restart game
		while (digitalRead(JOY_SEL) == 1) {}
		// reset variables and TFT screen
		score = 0;
		combo = 0;
		level = 0;
		linesCleared = 0;
		speedUp = 800;
		shiftLock = 0;
		rotLock = 0;
		fallTimer = millis();
		for (int i = 0; i < 7; i ++) {
			randomNums[i] = 3;
		}
		tft.fillScreen(TFT_BLACK);
		tft.drawLine(241, 0, 241, 480, colors[8]);
		for (int i = 0; i < 200; i ++) {
			tiles[i%10][i/10] = 0;
		}
	}
	Serial.flush();
	Serial.end();
	return 0;
}
