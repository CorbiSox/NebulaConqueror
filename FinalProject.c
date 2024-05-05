/*
===============================================================================
 Name        : FinalProject.c
 Author      : Danny Morgan, Corbin Daniel
 Version     : 1.0
 Description :
===============================================================================
*/

#include <stdlib.h>

#define FIO0PIN (*(volatile unsigned int *) 0x2009c014)
#define FIO0DIR (*(volatile unsigned int *) 0x2009c000)
#define PINMODE0 (*(volatile unsigned int *) 0x4002c040)
#define PINMODE1 (*(volatile unsigned int *) 0x4002c044)

// 8 bit gameMap info  000     00      000
//					   ships   weapon  stars

#define noStar 0x00
#define star1A 0x02
#define star1B 0x03
#define star2A 0x04
#define star2B 0x05
#define star3A 0x06
#define star3B 0x07

#define doubleBlast 0x08
#define specialBlast 0x10
#define enemyBlast 0x18

#define shipFB 0x20
#define playerShip 0x40
#define enemy1 0x60
#define enemy2 0x80
#define enemy3 0xA0
#define enemy4 0xC0
#define collisionAtPosition 0xE0

// Timer interrupt registers
#define T0IR (*(volatile unsigned int *) 0x40004000)
#define T0TCR (*(volatile unsigned int *) 0x40004004)
#define T0TC (*(volatile unsigned int *) 0x40004008)
#define T0MCR (*(volatile unsigned int *) 0x40004014)
#define T0MR0 (*(volatile unsigned int *) 0x40004018)
#define T0MR1 (*(volatile unsigned int *) 0x4000401C)
#define ISER0 (*(volatile unsigned int *) 0xE000E100)

// Arrays
int AddressCodes[80];
int gameMap[80];
int gameMapLast[80];

const int DB[] = {9, 8, 7, 6, 0, 1, 18, 17};		// Bits corresponding to pins 5-12

const int Control[] = {15, 16, 23};					// Bits corresponding pins 13-15
// Bit 15 is E, 16 is R/W, and 23 is RS
// output pins for keypad
const int KeyBitsOut[] = {24, 25};					// Pins 16,17 keypad outputs

// input pins for keypad
const int KeyBitsIn[] = {26, 2, 3};					// Pins 18,21,22 keypad inputs

// wait functions
void wait_100us(void);
void wait_ms(int);
void wait_ticks(int);

// write command and write data functions
void LCDwriteCommand(int);
void LCDwriteData(int);

// initializes the lcd
void InitializeLCD(void);

// puts the display address codes into an array for ease of use
void createDispAdd(void);

// initialize the output pins
void initOutPins(void);

// configure the input pins
void configInPins(void);

// detects a keypress from the numpad
void keyDetect(void);

// places the initial elements into the gameMap array
void populateBackground(void);

// moves the stars across the screen
void scrollBackground(void);

// twinkles the stars
void animateStars(void);

// writes the gameMap array to the display
void writeDisplay(void);

// writes appropriate star to display
void writeStar(int);

// writes a collision animation to the display
void writeCollision(int, int, int);

// writes a weapon to the display
void writeWeapon(int);

// writes a ship to the display
void writeShip(int, int);

// moves weapons across the display
void moveWeapons(void);

// cycles through the collision animation
void collisionAnimation(void);

// spawns random enemy on random line
void spawnEnemy(void);

// moves enemies across display
void moveEnemy(void);

// causes enemies to fire after a set time
void enemyFire(void);

// function for displaying title screen
void displayTitleScreen(void);

// sets all start conditions for arrays
void startGame(void);

// timer interrupt functions
void TimerInterruptInitialize(void);
void TIMER0_IRQHandler(void);

// used for masking off the individual elements out of the 8bit gameMap data
const int starMask = 0x07;
const int weaponMask = 0x18;
const int shipMask = 0xE0;

// flag used to check if player has been killed
int playerDown;

// flag used for setting start game conditions
int startGameNow = 1;

// loop variables for telling functions when to run an animation or debounce the keypad
int loopCountShiftStars = 0;
int loopCountAniStars = 0;
int loopWeaponBlast = 0;
int loopDebounceCount = 0;
int loopCollision = 0;
int loopSpam = 0;
int loopSpawnEnemy = 0;
int loopMoveEnemy = 0;
int loopEnemyFire = 0;

// fastest speed in milliseconds that the screen can update
int baseAnimateSpeed = 1;

// starting note pitch for laser blast
int noteValue = 1000;

// animate flag is used when a function updates the gameMap and needs the animation updated to the display
int animateFlag;

// Flag to determine if title screen should be displayed
int titleScreenFlag;

// Flag to determine when to play lazer beep
int soundFlag = -1;

// moveWeapons function will only run if this is greater than zero
int collisionsOnScreen = 0;

// character codes for display on screen
int blank = 0x20;
int ships[10] = {0x00, 0x01, 0x3C, 0xB4, 0xCC, 0xF6, 0x28, 0xD3, 0xE0, 0xE5}; // ship characters, player ship is position 0,1
int weapons[3] = {0x3D, 0x10, 0xAA};
int star1[2] = {0xA1, 0x2C};
int star2[2] = {0xDF, 0xDE};
int star3[2] = {0x2C, 0x2E};
int collisionArr[3] = {0xA5, 0xDB, 0x2A};

// collision animation at display position
int collisionAnimAtPos[2][10];

// enemy locations and fire countdown for individual ships
int enemyPosFire[2][4];

// positions of weapons and player on screen
int weaponPositions[20];
int enemyWeaponPos[10];
int playerPosition[2];

// initial starting positions of stars and types at each location on display
int starPositions[14] = {0, 6, 14, 24, 30, 38, 41, 47, 52, 57, 64,
                         68, 74, 78};
int starTypes[14] = {star1A, star2B, star3A, star2B, star1A, star2B, star2A,
                     star3B, star1A, star2B, star2A, star3B, star2A, star3B};

// title screen positions and characters
int titleScreenPositions[2] = {22, 42};
int titleScreenChars[2][16] = {{0x4E, 0x45, 0x42, 0x55, 0x4C, 0x41, 0x10, 0x43,
                                0x4F, 0x4E, 0x51, 0x55, 0x45, 0x52, 0x4F, 0x52},
							   {0x50, 0x72, 0x65, 0x73, 0x73, 0x10, 0x23, 0x10,
                                0x74, 0x6F, 0x10, 0x53, 0x74, 0x61, 0x72, 0x74}};

// notes for intro song
int notesForSong[8] = {370, 280, 370, 230, 230, 370, 205 , 230};



int main(void) {

    // seed the rand function
    srand((unsigned) 100);

    InitializeLCD();

    TimerInterruptInitialize();

    // flag for starting the title screen on first run
    titleScreenFlag = 1;

    // once title screen is displayed, this is flagged so the
    // display will not continually update
    int titleScreenOn = 0;

    while(1){

        // set the starting conditions
    	if(startGameNow){
			startGame();
			startGameNow = 0;
		}

        // display title screen
    	while (titleScreenFlag) {

    		if(!titleScreenOn){
    			playerDown = 0;
    			displayTitleScreen();
    			titleScreenOn = 1;
    		}

            // detect hash key to start
    		keyDetect();
    	}

        // set flag back to zero
    	titleScreenOn = 0;

        // run gameloop functions
        animateStars();
        scrollBackground();
        keyDetect();
        moveWeapons();
        collisionAnimation();
        spawnEnemy();
        moveEnemy();
        enemyFire();

        // if animateFlag is true, run writeDisplay function
        if(animateFlag == 1){
            writeDisplay();
            animateFlag = 0;
        }

        // wait one ms per loop
        wait_ms(baseAnimateSpeed);
    }
    return 0;
}

// initialize two interrupt timers
void TimerInterruptInitialize() {
	T0MR0 = T0TC + noteValue / 2;	// 1st interrupt 1000 clocks from now (2.7 ms)
	T0MR1 = T0TC + noteValue;	    // 1st interrupt 1000 clocks from now (2.7 ms)
	T0IR = (1<<0);			        // Clear old MR0 match events
	T0IR = (1<<1);                  // Clear old MR1 match events
    T0MCR |= (1<<0); 		        // Interrupt on MR0 match
    T0MCR |= (1<<3);                // Interrupt on MR1 match
    T0TCR = 1; 				        // Make sure timer enabled
    ISER0 = (1<<1); 		        // Enable Timer0 interrupts
}

// interrupt function called when match events are detected
void TIMER0_IRQHandler() {
	// Only need to check timer’s IR if using multiple
	// interrupt conditions with the same timer
	if ((T0IR>>0) & 1) { 			    // check for MR0 event
		T0MR0 = T0MR0 + noteValue; 		// noteValue added
		T0IR = (1<<0); 				    // clear MR0 event

        // if soundFlag is detected, set pin 23 high, increment soundFlag
		if(soundFlag != -1 && soundFlag < 150){
			FIO0PIN |= (1 << 21);
			soundFlag++;
		}
	}

	if ((T0IR>>1) & 1) { 			    // check for MR1 event
		T0MR1 = T0MR1 + noteValue; 		// noteValue added
		T0IR = (1<<1); 				    // clear MR1 event

        // if soundFlag detected, set pin 23 low, increment noteValue by 1 percent
        // and increment soundFlag. incrementing noteValue lowers the pitch of the
        // note slightly on the next loop
		if(soundFlag != -1 && soundFlag < 151){
			noteValue += (int)(noteValue * 0.01);
			FIO0PIN &= ~(1 << 21);
			soundFlag++;
		}

        // if soundFlag equal to 151, reset soundFlag and noteValue
		else if( soundFlag >= 151){
			soundFlag = -1;
			noteValue = 1000;
		}
	}
}

// writes gameMap data to display. Handles all logic for what gets displayed and what does not
void writeDisplay(){

    // step through each location in the gameMap
    for(int i = 0; i < 80; i++){

        // if the gameMap location equals the gameMapLast location, continue the loop immediately.
        // This saves unecessary writes and speeds up the display
        if(gameMapLast[i] == gameMap[i]){
            continue;
        }

            // if not, update the gameMapLast array with the new information
        else{
            gameMapLast[i] = gameMap[i];
        }

        // create necessary variables
        int tempShip;
        int tempWeapon;

        // mask the data off with ship and weapon mask, if greater than zero display either a weapon, ship or
        // collision animation
        if((gameMap[i] & (shipMask + weaponMask)) > 0){

            // call the LCDwriteCommand function to prepare sending the ASCII data

            // if both ship data and weapon data present at location, call the writeCollision function
            // this has the effect of writing gameMap with a collisionAtPosition marker. writeDisplay is called
            // again immediately and the collision animation happens within the writeShip function
            if((gameMap[i] & shipMask) > 0 && (gameMap[i] & weaponMask) > 0){
                tempShip = gameMap[i] & shipMask;
                tempWeapon = gameMap[i] & weaponMask;

                if(tempShip == collisionAtPosition){
                    LCDwriteCommand(AddressCodes[i]);
                    writeShip(tempShip, i);
                }

                // if the shipFB marker is detected the ship type data is saved in the gameMap location
                // in the previous array location, so this is extracted and sent to writeCollision function
                else if(tempShip == shipFB) {
                    tempShip = gameMap[i - 1] & shipMask;
                    writeCollision(tempShip, tempWeapon, i);
                }
                else{
                    writeCollision(tempShip, tempWeapon, i);
                }

            }

            // if just ship data, write ship characters
            else if((gameMap[i] & shipMask) > 0){
                tempShip = gameMap[i] & shipMask;
                LCDwriteCommand(AddressCodes[i]);
                writeShip(tempShip, i);
            }

            // if just weapon, write a weapon character
            else{
                tempWeapon = gameMap[i] & weaponMask;
                LCDwriteCommand(AddressCodes[i]);
                writeWeapon(tempWeapon);
            }
        }

        // if only star data at location, write a star
        else if((gameMap[i] & starMask) > 0){
            int star = gameMap[i] & starMask;
            LCDwriteCommand(AddressCodes[i]);
            writeStar(star);
        }

        // if nothing, write a blank
        else if(gameMap[i] == noStar){
            LCDwriteCommand(AddressCodes[i]);
            LCDwriteData(blank);
        }
    }
}

// moves weapons once created
void moveWeapons(){


    // ms until next move
    int loopsTillMove = 35;


    // checks if loop condition satisfied
    if (loopWeaponBlast >= loopsTillMove){

        // checks the weapon location array for weapons and increments/decrements accordingly
        for(int i = 0; i < 20; i++){

            // checks if there is weapon location data at this location in the array
            if(weaponPositions[i] != -1){

                // if the weapon has not reached the edge of the screen, increment its location
                if(weaponPositions[i] != 19 && weaponPositions[i] != 39 &&
                   weaponPositions[i] != 59 && weaponPositions[i] != 79){
                    weaponPositions[i]++;
                }

                // if the weapon has reached the edge of the screen, set the weaponsPosition array back to no
                // weapon in the array location and decrement the weaponsOnScreen variable
                else if(weaponPositions[i] == 19 || weaponPositions[i] == 39 ||
                        weaponPositions[i] == 59 || weaponPositions[i] == 79){

                    gameMap[weaponPositions[i]] = gameMap[weaponPositions[i]] & (shipMask + starMask);
                    weaponPositions[i] = -1;
                }
            }

        }

        for(int i = 0; i < 10; i++){

            // checks if there is weapon location data at this location in the array
            if(enemyWeaponPos[i] != -1){

                // if the weapon has not reached the edge of the screen, decrement its location
                if(enemyWeaponPos[i] != 0 && enemyWeaponPos[i] != 20 &&
                   enemyWeaponPos[i] != 40 && enemyWeaponPos[i] != 60){

                    enemyWeaponPos[i]--;
                }

                    // if the weapon has reached the edge of the screen, set the weaponsPosition array back to no
                    // weapon in the array location and decrement the weaponsOnScreen variable
                else if(enemyWeaponPos[i] == 0  || enemyWeaponPos[i] == 20 ||
                        enemyWeaponPos[i] == 40 || enemyWeaponPos[i] == 60){

                    gameMap[enemyWeaponPos[i]] = gameMap[enemyWeaponPos[i]] & (shipMask + starMask);
                    enemyWeaponPos[i] = -1;
                }
            }

        }



        // remove previous position player weapons blasts
        for(int i = 0; i < 20; i++){
            if(weaponPositions[i] != -1){
                gameMap[weaponPositions[i] - 1] = gameMap[weaponPositions[i] - 1] & (shipMask + starMask);

            }
        }

        // remove previous position enemy weapons blasts
        for(int i = 0; i < 10; i++){
            if(enemyWeaponPos[i] != -1){
				gameMap[enemyWeaponPos[i] + 1] = gameMap[enemyWeaponPos[i] + 1] & (shipMask + starMask);
            }
        }

        // check for player and enemy weapon collisions. Delete from corresponding arrays and
        // write collision animation to screen
        for(int i = 0; i < 20; i++){
        	for(int j = 0; j < 10; j++){
				if(weaponPositions[i] != -1 && enemyWeaponPos[j] != -1 && weaponPositions[i] == enemyWeaponPos[j]){
					LCDwriteCommand(AddressCodes[weaponPositions[i]]);
					LCDwriteData(0xFF);
					wait_ms(50);
					LCDwriteCommand(AddressCodes[weaponPositions[i]]);
					LCDwriteData(blank);
					weaponPositions[i] = -1;
					enemyWeaponPos[j] = -1;
				}

				if(weaponPositions[i] != -1 && enemyWeaponPos[j] != -1 && weaponPositions[i] + 1 == enemyWeaponPos[j]){
					LCDwriteCommand(AddressCodes[weaponPositions[i]]);
					LCDwriteData(0xFF);
					LCDwriteCommand(AddressCodes[weaponPositions[i] + 1]);
					LCDwriteData(0xFF);
					wait_ms(50);
					LCDwriteCommand(AddressCodes[weaponPositions[i]]);
					LCDwriteData(blank);
					LCDwriteCommand(AddressCodes[weaponPositions[i] + 1]);
					LCDwriteData(blank);
					weaponPositions[i] = -1;
					enemyWeaponPos[j] = -1;
				}
        	}

        }

        // write moved weapons blast to appropriate gameMap position
        for(int i = 0; i < 20; i++){
            if(weaponPositions[i] != -1){
                gameMap[weaponPositions[i]] = gameMap[weaponPositions[i]] & (shipMask + starMask);
                gameMap[weaponPositions[i]] += doubleBlast;

            }
        }

        for(int i = 0; i < 10; i++){
            if(enemyWeaponPos[i] != -1){
                gameMap[enemyWeaponPos[i]] = gameMap[enemyWeaponPos[i]] & (shipMask + starMask);
        		gameMap[enemyWeaponPos[i]] += enemyBlast;
            }
        }

        // update the animateFlag variable and set the loop back to zero
        animateFlag = 1;
        loopWeaponBlast = 0;
    }


    // increment loopWeaponBlast
    loopWeaponBlast++;


}

// write a weapon to the display
void writeWeapon(int toWeapon) {
    switch(toWeapon){
        case doubleBlast:
            LCDwriteData(weapons[0]);
            break;
        case specialBlast:
        	LCDwriteData(weapons[1]);
        	break;
        case enemyBlast:
        	LCDwriteData(weapons[2]);
        	break;
        default:
            break;
    }

}

// write a ship to the display
void writeShip(int toShip, int toLocation){
    switch(toShip){

        // this case ignores the shipFront case as the front is written in sequence with the rear
        // of the ship when that character is called.
        case shipFB:
            break;
        case playerShip:
            LCDwriteData(ships[0]);
            LCDwriteData(ships[1]);
            break;
        case enemy1:
            LCDwriteData(ships[2]);
            LCDwriteData(ships[3]);
            break;
        case enemy2:
            LCDwriteData(ships[4]);
            LCDwriteData(ships[5]);
            break;
        case enemy3:
            LCDwriteData(ships[6]);
            LCDwriteData(ships[7]);
            break;
        case enemy4:
            LCDwriteData(ships[8]);
            LCDwriteData(ships[9]);
            break;

        // this case checks the collisionAnimAtPos array to decide when frame in the collision animation
        // should be played when called
        case collisionAtPosition:
            for(int i = 0; i < 10; i++){
                if(collisionAnimAtPos[0][i] == toLocation){
                    switch (collisionAnimAtPos[1][i]) {
                        case 0:
                            LCDwriteData(collisionArr[0]);
                            break;

                        case 1:
                            LCDwriteData(collisionArr[1]);
                            break;

                        case 2:
                            LCDwriteData(collisionArr[2]);
                            break;
                    }
                }
            }
            break;
        default:
            break;
    }
}

// write a collision to the display
void writeCollision(int toShip, int toWeapon, int toLocation){

    // if player ship and collides with enemy blast
    if(toShip == playerShip && toWeapon == enemyBlast){

        // this flag effectively ends the game, but allows the collision animation to play
    	playerDown = 1;

        // write the first collision array character to display
        LCDwriteData(collisionArr[0]);

        // remove ship and weapons data
        gameMap[toLocation] = gameMap[toLocation] & starMask;
        gameMap[toLocation - 1] = gameMap[toLocation - 1] & starMask;
        playerPosition[0] = 20;
        playerPosition[1] = 21;

        // remove an enemy blast from its array
        for(int i = 0; i < 10; i++){
            if(enemyWeaponPos[i] == toLocation){
                enemyWeaponPos[i] = -1;
            }
        }


        // add collision markers
        gameMap[toLocation] += collisionAtPosition;
        gameMap[toLocation - 1] += collisionAtPosition;

        // create collision animation at first available array location with location marker
        // and start collision animation at zero
        for(int i = 0; i < 10; i++){
            if(collisionAnimAtPos[0][i] == -1){
                collisionAnimAtPos[0][i] = toLocation - 1;
                collisionAnimAtPos[1][i] = 0;
                break;
            }
        }

        // create collision animation at first available array location with location marker
        // and start collision animation at zero (twice for two character display locations)
        for(int i = 0; i < 10; i++){
            if(collisionAnimAtPos[0][i] == -1){
                collisionAnimAtPos[0][i] = toLocation;
                collisionAnimAtPos[1][i] = 0;
                break;
            }
        }
    }

    // if player laser blast hits an enemy ship
    else if(toShip !=playerShip && toWeapon == doubleBlast){

        // remove ship and weapons data
        gameMap[toLocation] = gameMap[toLocation] & starMask;
        gameMap[toLocation + 1] = gameMap[toLocation + 1] & starMask;
        for(int i = 0; i < 4; i++){
        	if(enemyPosFire[0][i] == toLocation){
        		enemyPosFire[0][i] = -1;
        		enemyPosFire[1][i] = -1;
        	}
        }

        // remove player blast from its array
        for(int i = 0; i < 20; i++){
            if(weaponPositions[i] == toLocation){
                weaponPositions[i] = -1;
            }
        }

        // add collision markers
        gameMap[toLocation] += collisionAtPosition;
        gameMap[toLocation + 1] += collisionAtPosition;

        // create collision animation at first available array location with location marker
        // and start collision animation at zero
        for(int i = 0; i < 10; i++){
            if(collisionAnimAtPos[0][i] == -1){
                collisionAnimAtPos[0][i] = toLocation;
                collisionAnimAtPos[1][i] = 0;
                break;
            }
        }

        // create collision animation at first available array location with location marker
        // and start collision animation at zero (twice for two character display locations)
        for(int i = 0; i < 10; i++){
            if(collisionAnimAtPos[0][i] == -1){
                collisionAnimAtPos[0][i] = toLocation + 1;
                collisionAnimAtPos[1][i] = 0;
                break;
            }
        }

    }

    // increment collisionsOnScreen
    collisionsOnScreen+= 2;
//    collisionsOnScreen++;
    animateFlag = 1;
}

// increment the collisionAnimAtPos array for displaying collision animation
void collisionAnimation(){

    // return if no collisions on screen
    if(collisionsOnScreen == 0) return;

    // time between increments
    int loopsTillCollisionAnimation = 200;

    if(loopCollision >= loopsTillCollisionAnimation){

        // check array for collision animations to increment
        for (int i = 0; i < 10; i++){

            // continue if none at array location
            if(collisionAnimAtPos[0][i] == -1) continue;

            // increment collision animation if less than 2
            if(collisionAnimAtPos[1][i] < 2 && collisionAnimAtPos[1][i] != -1){
                collisionAnimAtPos[1][i]++;

                // increment the gameMap by doubleBlast. This acts as a flag to writeDisplay that the
                // gameMap has changed, otherwise the function will not know that a change needs to
                // written to the display
                gameMap[collisionAnimAtPos[0][i]] += doubleBlast;

                // call the writeDisplay function
                animateFlag = 1;
            }
            else{
                // remove collision marker
                gameMap[collisionAnimAtPos[0][i]] = gameMap[collisionAnimAtPos[0][i]] & starMask;

                // reset collisionAnimAtPos array to initialized at location
                collisionAnimAtPos[0][i] = -1;
                collisionAnimAtPos[1][i] = -1;

                // decrement collisionsOnScreen;
                collisionsOnScreen--;

                // when the player is down, this resets the game after the collision animation plays
                if(playerDown){
                	titleScreenFlag = 1;
                	startGameNow = 1;
                }
            }
        }
        loopCollision = 0;
    }
    else{
        loopCollision++;
    }
}

// write a star to the display
void writeStar(int toWrite){
    switch (toWrite){
        case star1A:
            LCDwriteData(star1[0]);
            break;
        case star1B:
            LCDwriteData(star1[1]);
            break;
        case star2A:
            LCDwriteData(star2[0]);
            break;
        case star2B:
            LCDwriteData(star2[1]);
            break;
        case star3A:
            LCDwriteData(star3[0]);
            break;
        case star3B:
            LCDwriteData(star3[1]);
            break;
        default:
            LCDwriteData(blank);
    }

}
// This function spawns enemies every 1.5 seconds
void spawnEnemy(){

    // check to see whether there are enough enemies on screen
	int enemyArrCheck = 0;
	for(int i = 0; i < 4; i++){
		if(enemyPosFire[0][i] >= 0){
		enemyArrCheck++;
		}
	}

    // if there are four enemies on screen, return from this function
	if(enemyArrCheck == 4) return;

    // spawn time every 1.5 seconds
    int loopSpawnTime = 1500;

    if(loopSpawnEnemy <= loopSpawnTime){
        loopSpawnEnemy++;
        return;
    }

    // random number 0 - 15
    int lineSpawn = rand() % 16;

    // set enemy position based on lineSpawn within the enemyPosFire array
    if(lineSpawn < 4){

        // check if there is a ship or weapons already at this location and return if so
        if((gameMap[18] & shipMask) > 0 || (gameMap[19] & shipMask) > 0 ||
		(gameMap[18] & weaponMask) == doubleBlast || (gameMap[19] & weaponMask) == doubleBlast) {
        	return;
		}

        // set the enemy position in the enemyPosFire array as the end of the first line
    	for(int i = 0; i < 4; i++){
    		if (enemyPosFire[0][i] == -1){
    			enemyPosFire[0][i] = 18;
    			break;
    		}
    	}
    }

    if(lineSpawn >= 4 && lineSpawn < 8){
        if((gameMap[38] & shipMask) > 0 || (gameMap[39] & shipMask) > 0 ||
		(gameMap[38] & weaponMask) == doubleBlast || (gameMap[39] & weaponMask) == doubleBlast) {
        	return;
		}

        // set the enemy position in the enemyPosFire array as the end of the second line
    	for(int i = 0; i < 4; i++){
    		if (enemyPosFire[0][i] == -1){
    			enemyPosFire[0][i] = 38;
    			break;
    		}
    	}
    }

    if(lineSpawn >= 8 && lineSpawn < 12){
        if((gameMap[58] & shipMask) > 0 || (gameMap[59] & shipMask) > 0 ||
		(gameMap[58] & weaponMask) == doubleBlast || (gameMap[59] & weaponMask) == doubleBlast) {
        	return;
		}

        // set the enemy position in the enemyPosFire array as the end of the third line
    	for(int i = 0; i < 4; i++){
    		if (enemyPosFire[0][i] == -1){
    			enemyPosFire[0][i] = 58;
    			break;
    		}
    	}
    }

    if(lineSpawn >= 12){
        if((gameMap[78] & shipMask) > 0 || (gameMap[79] & shipMask) > 0 ||
		(gameMap[78] & weaponMask) == doubleBlast || (gameMap[79] & weaponMask) == doubleBlast) {
        	return;
		}

        // set the enemy position in the enemyPosFire array as the end of the fourth line
    	for(int i = 0; i < 4; i++){
    		if (enemyPosFire[0][i] == -1){
    			enemyPosFire[0][i] = 78;
    			break;
    		}
    	}
	}

    // write enemies to gameMap based on lineSpawn value. This determines both location (1 of 4 lines)
    // and enemy type (1 of 4 enemies) hence 16 cases
    switch (lineSpawn) {
        case 0:
        	gameMap[18] += enemy1;
        	gameMap[19] += shipFB;
            break;
		case 1:
			gameMap[18] += enemy2;
			gameMap[19] += shipFB;
			break;
		case 2:
			gameMap[18] += enemy3;
			gameMap[19] += shipFB;
			break;
		case 3:
			gameMap[18] += enemy4;
			gameMap[19] += shipFB;
			break;
		case 4:
			gameMap[38] += enemy1;
			gameMap[39] += shipFB;
            break;
        case 5:
        	gameMap[38] += enemy2;
			gameMap[39] += shipFB;
			break;
		case 6:
			gameMap[38] += enemy3;
			gameMap[39] += shipFB;
			break;
		case 7:
			gameMap[38] += enemy4;
			gameMap[39] += shipFB;
            break;
		case 8:
			gameMap[58] += enemy1;
			gameMap[59] += shipFB;
			break;
		case 9:
			gameMap[58] += enemy2;
			gameMap[59] += shipFB;
			break;
		case 10:
			gameMap[58] += enemy3;
			gameMap[59] += shipFB;
			break;
		case 11:
			gameMap[58] += enemy4;
			gameMap[59] += shipFB;
			break;
		case 12:
			gameMap[78] += enemy1;
			gameMap[79] += shipFB;
			break;
		case 13:
			gameMap[78] += enemy2;
			gameMap[79] += shipFB;
			break;
		case 14:
			gameMap[78] += enemy3;
			gameMap[79] += shipFB;
			break;
		case 15:
			gameMap[78] += enemy4;
			gameMap[79] += shipFB;
			break;
		default:
			break;
    }

    loopSpawnEnemy = 0;
    animateFlag = 1;

}

// This function moves enemies on screen at appropriate intervals
void moveEnemy(){

    // move enemies forward every quarter second
    int moveEnemyTime = 250;

    if(loopMoveEnemy < moveEnemyTime){
        loopMoveEnemy++;
        return;
    }

    for(int i = 0; i < 4; i++){

        // check the gameMap for the locations found in the enemyPosFire array for enemies to move
        if((gameMap[enemyPosFire[0][i]] & shipMask) == enemy1 || (gameMap[enemyPosFire[0][i]] & shipMask) == enemy2 ||
        (gameMap[enemyPosFire[0][i]] & shipMask) == enemy3 || (gameMap[enemyPosFire[0][i]] & shipMask) == enemy4){

            // if the enemy is at the edge of the screen, remove from gameMap and enemyPosFire array
            if(enemyPosFire[0][i] == 0 || enemyPosFire[0][i] == 20 || enemyPosFire[0][i] == 40 || enemyPosFire[0][i] == 60){
            	gameMap[enemyPosFire[0][i]] = gameMap[enemyPosFire[0][i]] & (starMask + weaponMask);
            	gameMap[enemyPosFire[0][i] + 1] = gameMap[enemyPosFire[0][i] + 1] & (starMask + weaponMask);
            	enemyPosFire[0][i] = -1;
            }

            // move the enemy forward
            else {

                // grab ship info and place in temp variable
                int tempShip = gameMap[enemyPosFire[0][i]] & shipMask;

                // clear info from cell both ship cells
                gameMap[enemyPosFire[0][i]] = gameMap[enemyPosFire[0][i]] & (starMask + weaponMask);
                gameMap[enemyPosFire[0][i] + 1] = gameMap[enemyPosFire[0][i] + 1] & (starMask + weaponMask);

                // add shipFront flag current cell
                gameMap[enemyPosFire[0][i]] += shipFB;

                // clear leading cell and add ship info
                gameMap[enemyPosFire[0][i] - 1] = gameMap[enemyPosFire[0][i] - 1] & (starMask + weaponMask);
                gameMap[enemyPosFire[0][i] - 1] += tempShip;

                // decrement the position info to align with its location on screen
                enemyPosFire[0][i]--;
            }
        }
    }

    // reset the loop and call the writeDisplay function
    loopMoveEnemy = 0;
    animateFlag = 1;

}

// this function controls when an enemy fires its weapon
void enemyFire() {

    // loop time
	int loopsTillFire = 150;

	if (loopEnemyFire < loopsTillFire){
		loopEnemyFire++;
		return;
	}

    // check the four array locations for enemy fire timer info
	for (int i = 0; i < 4; i++) {

        // if it equals three, add an enemy blast in front of the appropriate enemy and add
        // a position to the enemyWeaponPos array
		if(enemyPosFire[1][i] == 3){
			for(int j = 0; j < 10; j++){
				if(enemyWeaponPos[j] == -1){
					gameMap[enemyPosFire[0][i] - 1] += enemyBlast;
					enemyWeaponPos[j] = enemyPosFire[0][i] - 1;
					break;
				}
			}

            // call the writeDisplay function
			animateFlag = 1;
		}

        // increment the enemy fire timer info for all ships on screen. This loops every four iterations
		if(enemyPosFire[0][i] != -1 && enemyPosFire[0][i] != 0 &&
		enemyPosFire[0][i] != 20 && enemyPosFire[0][i] != 40 && enemyPosFire[0][i] != 60){
			enemyPosFire[1][i] = (enemyPosFire[1][i] + 1) % 4;
		}
	}

	loopEnemyFire = 0;
}

// populates the gameMap with the initial on screen elements
void populateBackground() {


    // blank the gameMap
    for(int i = 0; i < 80; i++){
        gameMap[i] = noStar;
    }

    // set the stars at their initial positions
    for (int i = 0; i < 14; i++) {
        gameMap[starPositions[i]] += starTypes[i];
    }

    // set the player positions on the gameMap
    gameMap[playerPosition[0]] += playerShip;
    gameMap[playerPosition[1]] += shipFB;

}

// twinkles the stars in the background on a set loop
void animateStars() {

    // ms per loop between switch over in star animation
    int loopsTillAnimate = 175;
    if (loopCountAniStars == loopsTillAnimate) {

        // check all gameMap array locations for stars
        for (int i = 0; i < 80; i++) {

            // set gameMap star data to tempStar variable
            int tempStar = (gameMap[i] & starMask);
            if (tempStar == noStar) continue;

            // switch case changes star data based on tempStar data, twinkles star
            switch (tempStar) {
                case star1A:
                    gameMap[i] -= star1A;
                    gameMap[i] += star1B;
                    break;
                case star1B:
                    gameMap[i] -= star1B;
                    gameMap[i] += star1A;
                    break;
                case star2A:
                    gameMap[i] -= star2A;
                    gameMap[i] += star2B;
                    break;
                case star2B:
                    gameMap[i] -= star2B;
                    gameMap[i] += star2A;
                    break;
                case star3A:
                    gameMap[i] -= star3A;
                    gameMap[i] += star3B;
                    break;
                case star3B:
                    gameMap[i] -= star3B;
                    gameMap[i] += star3A;
                    break;
                default:
                    break;
            }
        }

        // set animate flag to 1 to call writeDisplay function
        animateFlag = 1;

        // set loop count back to zero
        loopCountAniStars = 0;
    }
    else {
        loopCountAniStars++;
    }
}

// shift the star data one location to the left
void scrollBackground(){

    // ms before next shift
    int loopsTillShift = 1050 / 2;
    int tempEdgeStar = 0x00;

    if (loopCountShiftStars == loopsTillShift){
        for(int i = 0; i < 80; i++){

            // mask gameMap data for star data only
            int tempPotStar = gameMap[i] & starMask;

            // if the star reaches the edge of the screen (left), assign it to the edge star
            if(i == 0 || i == 20 || i == 40 || i == 60){
                tempEdgeStar = tempPotStar;
                gameMap[i] -= tempPotStar;
            }

            // retrieve the edge star and assign it the screen edge (right)
            else if (i == 19 || i == 39 || i == 59 || i == 79){
                gameMap[i] -= tempPotStar;
                gameMap[i] += tempEdgeStar;
                tempEdgeStar = 0x00;
                gameMap[i - 1] += tempPotStar;
            }

            // move star one location to the left on gameMap
            else{
                gameMap[i] -= tempPotStar;
                gameMap[i - 1] += tempPotStar;
            }

        }

        // set loopCount variable to zero and set animateFlag
        loopCountShiftStars = 0;
        animateFlag = 1;
    }
    else {
        // loopCount increment
        loopCountShiftStars++;
    }
}

void playIntroSong() {

    // play all eight notes in array
	for (int i = 0; i < 8; i++) {
		int freq = notesForSong[i];

        // play notes for indicated duration
		for (int j = 0; j < (int)(50000 / freq); j++) {
			FIO0PIN |= (1 << 21);

			wait_ticks(freq);

			FIO0PIN &= ~(1 << 21);

			wait_ticks(freq);
		}
	}
}

// detects whether a key has been pressed
void keyDetect(){

    int debounceTimeMin = 50;
    int loopSpamMin = 100;

    if (loopDebounceCount < debounceTimeMin) {
    	loopDebounceCount++;
    	return;
    }

    // when the game is on the title screen, this function will only try to detect
    // the start button, aka the hash button
    if (titleScreenFlag) {
    	FIO0PIN |= (1 <<KeyBitsOut[0]);
		FIO0PIN &= ~(1 << KeyBitsOut[1]);

        // when start button is detected, play intro song and then proceed
		if((FIO0PIN >> KeyBitsIn[0] & 1) == 1){

			playIntroSong();

			titleScreenFlag = 0;
		}
		return;
    }

    // lock out key presses when player is killed
    if(playerDown) return;


    // set output pins high/low
	FIO0PIN |= (1 <<KeyBitsOut[0]);
	FIO0PIN &= ~(1 << KeyBitsOut[1]);

    // check input pins
    // if hash key pressed, add player blast to gameMap
	if((FIO0PIN >> KeyBitsIn[0] & 1) == 1){

		if(loopSpam >= loopSpamMin){
			loopDebounceCount = 0;
			gameMap[playerPosition[1] + 1] += doubleBlast;
			for(int i = 0; i < 20; i++){
				if(weaponPositions[i] == -1){
					if(playerPosition[1] != 19 && playerPosition[1] != 39 && playerPosition[1] != 59 && playerPosition[1] != 79) {
						weaponPositions[i] = playerPosition[1] + 1;

					}
					break;
				}
			}
			loopSpam = 0;
			animateFlag = 1;
			soundFlag = 0;
			return;
		}
	}

    // if 9 key detected, move player forward
	if((FIO0PIN >> KeyBitsIn[1] & 1) == 1){
		if(playerPosition[1] != 19 && playerPosition[1] != 39 && playerPosition[1] != 59 && playerPosition[1] != 79){
			loopDebounceCount = 0;
			gameMap[playerPosition[1]] -= shipFB;
			gameMap[playerPosition[1] + 1] += shipFB;
			playerPosition[1]++;
			gameMap[playerPosition[0]] -= playerShip;
			gameMap[playerPosition[0] + 1] += playerShip;
			playerPosition[0]++;
			animateFlag = 1;
		}
		return;
	}

    // do nothing when the 6 key is pressed
	if((FIO0PIN >> KeyBitsIn[2] & 1) == 1){
		return;
	}

    // invert the output pins, low/high
	FIO0PIN |= (1 <<KeyBitsOut[1]);
	FIO0PIN &= ~(1 << KeyBitsOut[0]);

    // if 0 key detected, move player down
	if((FIO0PIN >> KeyBitsIn[0] & 1) == 1){
		if(playerPosition[1] < 60){
			loopDebounceCount = 0;
			gameMap[playerPosition[1]] -= shipFB;
			gameMap[playerPosition[1] + 20] += shipFB;
			playerPosition[1] += 20;
			gameMap[playerPosition[0]] -= playerShip;
			gameMap[playerPosition[0] + 20] += playerShip;
			playerPosition[0] += 20;
			animateFlag = 1;
		}
		return;
	}

    // if 8 key detected, move player backwards
	if((FIO0PIN >> KeyBitsIn[1] & 1) == 1){
		if(playerPosition[0] != 0 && playerPosition[0] != 20 && playerPosition[0] != 40 && playerPosition[0] != 60){
			loopDebounceCount = 0;
			gameMap[playerPosition[0]] -= playerShip;
			gameMap[playerPosition[0] - 1] += playerShip;
			playerPosition[0]--;
			gameMap[playerPosition[1]] -= shipFB;
			gameMap[playerPosition[1] - 1] += shipFB;
			playerPosition[1]--;
			animateFlag = 1;
		}
		return;
	}

    // if 5 key detected, move player up
	if((FIO0PIN >> KeyBitsIn[2] & 1) == 1){
		if(playerPosition[0] > 19){
			loopDebounceCount = 0;
			gameMap[playerPosition[1]] -= shipFB;
			gameMap[playerPosition[1] - 20] += shipFB;
			playerPosition[1] -= 20;
			gameMap[playerPosition[0]] -= playerShip;
			gameMap[playerPosition[0] -20] += playerShip;
			playerPosition[0] -= 20;
			animateFlag = 1;
		}
		return;
	}

    if(loopSpam <= loopSpamMin) loopSpam++;
    if(loopDebounceCount <= debounceTimeMin) loopDebounceCount++;
}

// initialize the LCD
void InitializeLCD(){

    // initialize and configure all pins
    initOutPins();
    configInPins();

    // Drive R/W, RS, and E low
    for (int i = 0; i < 3; i++) {
        FIO0PIN &= ~(1<<Control[i]);
    }

    wait_ms(4);

    LCDwriteCommand(0x38);

    // Set cursor to advance to the right after each character sent
    LCDwriteCommand(0x06);

    // Enable display only, cursor will be invisible
    LCDwriteCommand(0x0c);

    // Clear display and move cursor to upper left position
    LCDwriteCommand(0x01);

    wait_ms(4);

    // Defining custom character 0
    LCDwriteCommand(0x40);
    LCDwriteData(0b11100); //Byte 1
    LCDwriteData(0b01110); //Byte 2
    LCDwriteData(0b00111);
    LCDwriteData(0b00010);
    LCDwriteData(0b00010);
    LCDwriteData(0b00111);
    LCDwriteData(0b01110);
    LCDwriteData(0b11100); //Byte 8

    // Defining custom character 1
    LCDwriteData(0b00000); //Byte 1
    LCDwriteData(0b11100); //Byte 2
    LCDwriteData(0b11000);
    LCDwriteData(0b10111);
    LCDwriteData(0b10111);
    LCDwriteData(0b11000);
    LCDwriteData(0b11100);
    LCDwriteData(0b00000); //Byte 8

    wait_ms(4);

    // create the display addresses array
    createDispAdd();
    startGame();

}

// start game function initalizes all arrays to their original values

void startGame(){

    // initialize the weaponsPosition array to all -1 to signify no weapons on screen
    for(int i = 0; i < 20; i++){
        weaponPositions[i] = -1;
    }

    for(int i = 0; i < 10; i++){
    	enemyWeaponPos[i] = -1;
    }

    // initialize the gamMapLast array to -1 at all positions
    for(int i = 0; i < 80; i++){
        gameMapLast[i] = -1;
    }

    // initialize collisionAnimAtPos to -1 at all positions
    for(int i = 0; i < 2; i++){
        for(int j = 0; j < 10; j++){
            collisionAnimAtPos[i][j] = -1;
        }
    }
    // initialize enemyPosFire to -1 at all positions
    for(int i = 0; i < 2; i++){
        for(int j = 0; j < 4; j++){
            enemyPosFire[i][j] = -1;
        }
    }

    // set animateFlag to 1 for initial draw
    animateFlag= 1;

    // set the player position array to starting positions
    playerPosition[0] = 20;
    playerPosition[1] = 21;

    // execute populateBackground function
	populateBackground();
}

// display the title screen for the game
void displayTitleScreen() {

	// Clear screen
	for (int i = 0; i < 80; i++) {
		LCDwriteCommand(AddressCodes[i]);
		LCDwriteData(blank);
	}

	// Write title screen
	for (int i = 0; i < 2; i++) {
		LCDwriteCommand(AddressCodes[titleScreenPositions[i]]);
		for (int j = 0; j < 16; j++) {
			LCDwriteData(titleScreenChars[i][j]);
		}
	}
}

// configures the keypad input pins
void configInPins(){

    // Configure keypad inputs
    PINMODE0 |= (1 << 4) | (1 << 5);
    PINMODE0 |= (1 << 6) | (1 << 7);
    PINMODE1 |= (1 << 20) | (1 << 21);

}

// initialize the output pins
void initOutPins(){
    // Initializing pins 5 - 12
    for (int i = 0; i < 8; i++) {
        FIO0DIR |= (1 << DB[i]);
    }

    // Initializing pins 13 - 15
    for (int i = 0; i < 3; i++) {
        FIO0DIR |= (1 << Control[i]);
    }

    // Initializing pins 16, 17, used for keypad
    for (int i = 0; i < 2; i++) {
        FIO0DIR |= (1 << KeyBitsOut[i]);
    }

    // Initialize pin 23 for piezo
    FIO0DIR |= (1 << 21);
}

// Assigning Address codes to be in order of appearance on display
/*
 * 0  1  2  ... 19
 * 40 41 42 ... 59
 * 20 21 22 ... 39
 * 60 61 62 ... 79
 */
void createDispAdd(){

    for (int i = 0; i < 20; i++) {
        AddressCodes[i] = i + 0x80;
    }

    for (int i = 20; i < 40; i++) {
        AddressCodes[i] = i + 44 + 0x80;
    }

    for (int i = 40; i < 60; i++) {
        AddressCodes[i] = i - 20 + 0x80;
    }

    for (int i = 60; i < 80; i++) {
        AddressCodes[i] = i + 24 + 0x80;
    }
}

// Approximately 37 ticks in 100 us
void wait_100us() {
    volatile int count;
    int wait_ticks = 37;
    for (count = 0; count < wait_ticks; count++) {
        // Do nothing
    }
}

// wait function based on individual system ticks
void wait_ticks(int tick){
	volatile int count;
	for (count = 0; count < tick; count++) {
	        // Do nothing
	}
}

// Call wait_100us() 10 times per ms
void wait_ms(int ms){
    for(int i = 0; i < ms * 10; i++){
        wait_100us();
    }
}

// send the write command to the display
void LCDwriteCommand(int CommandData) {

    // Update DB0-DB7 to match command code
    for (int i = 0; i < 8; i++) {
        if ((CommandData >> i) & 1) {
            FIO0PIN |= (1<<DB[i]);
        }
        else {
            FIO0PIN &= ~(1<<DB[i]);
        }
    }

    // Drive R/W low
    FIO0PIN &= ~(1<<Control[1]);

    // Drive RS low to indicate this is a command
    FIO0PIN &= ~(1<<Control[2]);

    // Drive E high, then low to generate the pulse
    FIO0PIN |= (1<<Control[0]);
    wait_100us();
    FIO0PIN &= ~(1<<Control[0]);

    // Wait 100 us
    wait_100us();
}

// send data to be written to the display
void LCDwriteData(int ASCIIData) {

    // Update DB0-DB7 to match data (ASCII) code
    for (int i = 0; i < 8; i++) {
        if ((ASCIIData >> i) & 1) {
            FIO0PIN |= (1<<DB[i]);
        }
        else {
            FIO0PIN &= ~(1<<DB[i]);
        }
    }

    // Drive R/W low
    FIO0PIN &= ~(1<<Control[1]);

    // Drive RS high to indicate this is data
    FIO0PIN |= (1<<Control[2]);

    // Drive E high, then low to generate the pulse
    FIO0PIN |= (1<<Control[0]);
    wait_100us();
    FIO0PIN &= ~(1<<Control[0]);

    // Wait 100 us
    wait_100us();
}

