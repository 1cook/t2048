#define OPTIMIZE_ROM_CALLS
#define USE_TI89
#define NO_CALC_DETECT
#define SAVE_SCREEN

#include<stdio.h>
#include<graph.h>
#include<dialogs.h>
#include<menus.h>
#include<stdlib.h>
#include<kbd.h>
#include<statline.h>
#include<compat.h>
#include<files.h>
#include<error.h>
#include<args.h>
#include<alloc.h>
#include<string.h>
#include<system.h>
#include<vat.h>
#include<timath.h>

#define COMMENT_STRING "The sliding puzzle game"
#define COMMENT_PROGRAM_NAME "2048"
#define VERSION_STRING "1.1"
#define VERSION_NUMBER 1,1,0,0
#define AUTHORS "Harrison Cook"

	
char *scoreString = NULL;
HANDLE pauseMenu = H_NULL;
HANDLE nameBox = H_NULL;
HANDLE gameOver = H_NULL;
HANDLE aboutBox = H_NULL;
	
//Frees up heap memory for a certain number of things, then kills the program
void memkill(char lvl, short exitcode){
	switch(lvl){
		case 0:
			HeapFree(aboutBox);
		case 1:
			HeapFree(gameOver);
		case 2:
			HeapFree(nameBox);
		case 3:
			HeapFree(pauseMenu);
		case 4:
			free(scoreString);		
	}
	exit(exitcode);
}

const char *SAVE_FILE_NAME = "main\\scor2048";
const char *strConv[] = {NULL,"2","4","8","16","32","64","128","256","512","1024","2048","4096","8192","16384","32768","65536","131072"};

//Data structure for a move animation
typedef struct MOVE{
	char xFrom,yFrom; //Starting x coordinate [0-3]
	char beforeValue;
} MOVE; 
//Data structure for the save file
typedef struct SAVE{
	//current Game State
	char grid[4][4];
	unsigned long int score;
	
	//HighScore
	unsigned long int bscore;
	char name[10];
	
	//Do you play the animations.
	char animations;
} SAVE;

//zeros the grid and adds inital tiles
void initGrid(char gridIn[4][4]){
	memset(gridIn,0,16);
	pushNewTile(gridIn);
	pushNewTile(gridIn);
}

//initializes initial values of the save structure
void makeSave(SAVE *saveFile){
	saveFile->score = 0;
	saveFile->bscore = 0;
	strncpy(saveFile->name,"...",10);
	saveFile->animations = 1;
	initGrid(saveFile->grid);
}
//Takes the save file and turns it to the struct SAVE
//Returns: 0 if the file exists, 1 if it does not
char readSave(SAVE *saveFile){
	FILE *f = NULL;
	if ((f = fopen(SAVE_FILE_NAME,"rb")) == NULL){
		return 1;
	}
	if ((fread(saveFile,sizeof(SAVE),1,f)) != 1) {
		fclose(f);
		exit(ER_PROTECTED);
	}
	fclose(f);
	
	//Validate the data from the file
	char x,y;
	for(x=0; x<=3; x++){
		for(y=0; y<=3; y++){
			if((saveFile->grid[x][y] < 0) || (saveFile->grid[x][y] > 17))
				exit(ER_PROTECTED);
		}
	}
	
	return 0;
}

//Takes the SAVE struct and writes it to the file
void writeSave(SAVE *p){
	FILE *f = NULL;
	if ((f = fopen(SAVE_FILE_NAME,"wb")) == NULL){
		exit(ER_PROTECTED);
	}
	if ((fwrite(p,sizeof(SAVE),1,f)) != 1){
		fclose(f);
		exit(ER_PROTECTED);
	}
	fputc(0,f);
	fputs(strConv[11],f);
	fputc(0,f);
	fputc(OTH_TAG,f);
	fclose(f);
}

enum direction{UP,RIGHT,DOWN,LEFT};
const char shiftX[] = {0,1,0,-1};
const char shiftY[] = {-1,0,1,0};



/*Finds 1 random empty spot to push a 2 or 4 tile into*/
void pushNewTile(char gridIn[4][4]){
	char tileValue, row, col;
	for( row = 0 ; row <= 3 ; row ++){
		for ( col = 0 ; col <= 3 ; col++){
			if(gridIn[row][col] == 0){
				goto skipCheck;
			}
		}
	}
	return;
	skipCheck:
	do{
		tileValue = (char)(((random(10))==0) + 1); //Probabilty of 2 is 9/10. 4 is 1/10
		row = (char)(random(4));
		col = (char)(random(4));
	}while(gridIn[row][col] != 0);
	gridIn[row][col] = tileValue;
}

//shifts a single tile in the specified direction and marks that tile as already having been moved into, returns the updated score
char shiftTile(char gridIn[4][4], char row, char col, char direction, unsigned long int *score, char isGridCompleted[4][4]){
	char xPos, yPos;
	switch (direction){
		case UP:
			xPos = row - 1;
			yPos = col;
			break;
		case RIGHT:
			xPos =  row;
			yPos = col + 1;
			break;
		case DOWN:
			xPos = row + 1;
			yPos = col;
			break;
		case LEFT:
			xPos = row;
			yPos = col - 1;
			break;
	}
	if(xPos == 4 || xPos == -1 || yPos == 4 || yPos == -1)
		return 0;
	if(gridIn[xPos][yPos] == 0 && gridIn[row][col] == 0)
		return 0;
	if(!((gridIn[xPos][yPos] == gridIn[row][col]) || (gridIn[xPos][yPos] == 0)))
		return 0;
	if(isGridCompleted == NULL)
		return 1;
	
	if(isGridCompleted[row][col])
		return 0;
	
	if ( gridIn[xPos][yPos] == 0)
		gridIn[xPos][yPos] = gridIn[row][col];
	else{
		gridIn[xPos][yPos] += 1;
		if(gridIn[xPos][yPos] > 17)
			memkill(0,ER_OVERFLOW);
			/*This game does not support going over 2^17 = 131072
			and you can't exceed that value in a normal game anyways
			this check prevents accessing bad memory*/
		*score += (1 << (unsigned long int)gridIn[xPos][yPos]);
		isGridCompleted[xPos][yPos] = 1;
		isGridCompleted[row][col] = 1;
	}
	gridIn[row][col] = 0;
	return 1;
}

//detects if the grid can be moved in the specified direction
char isDirectionAvailable(char gridIn[4][4], char direction){
	char xPos, yPos;
	for(xPos = 0; xPos<=3; xPos++){
		for(yPos = 0; yPos<= 3; yPos++){
			if(shiftTile(gridIn,xPos,yPos,direction,NULL, NULL)){
				return 1;
			}
		}
	}
	return 0;
}

//detects if there are available moves
char isGridAvailable(char gridIn[4][4]){
	char direction;
	for(direction = UP; direction <= LEFT; direction++){
		if(isDirectionAvailable(gridIn,direction)){
			return 1;
		}
	}
	return 0;
}

//Define the rectangle points for the tiles;
const unsigned char LeftX[] = {4,43,82,123};
const unsigned char RightX[] = {39,78,119,158};
const unsigned char TopY[] = {4,26,48,70};
const unsigned char BottomY[] = {22,44,66,88};

//draws the box on the screen
void drawBox(char col, char row, char direction, char shift, char value,short attr){
	unsigned char Xmodifier = (shiftX[direction]*shift);
	unsigned char Ymodifier = (shiftY[direction]*shift);
	unsigned char lx = LeftX[row]+Xmodifier;
	unsigned char rx = RightX[row]+Xmodifier;
	unsigned char ty = TopY[col]+Ymodifier;
	unsigned char by = BottomY[col]+Ymodifier;

	DrawLine(lx,ty,lx,by, attr);
	DrawLine(lx,ty,rx,ty, attr);
	DrawLine(rx,ty,rx,by,attr);
	DrawLine(lx,by,rx,by,attr);
	if(value == 17)
		FontSetSys(F_4x6);//Make Text even more smaller for even more bigger numbers
	else if(value>13)
		FontSetSys(F_6x8);//Make text smaller for bigger numbers
	DrawStr(lx+2, ty+2, strConv[value],attr);
	FontSetSys(F_8x10);
}

//clears the area on the screen with the box
void clearBox(char col, char row, char direction, char shift){
	char Xmodifier = (shiftX[direction]*shift); char Ymodifier = (shiftY[direction]*shift);
	SCR_RECT *s = &(SCR_RECT){{LeftX[row]+Xmodifier,TopY[col]+Ymodifier,RightX[row]+Xmodifier,BottomY[col]+Ymodifier}};
	ScrRectFill(s,ScrRect,A_REVERSE);
}
//number of pixels to move in each direction
const char directionShift[] = {22,39,22,39};

//plays a single frame of the movement. only draws the number of tiles listed in char amount

/*warning, only set amount up to the number of tile movements you have specified in frame,
otherwise you might get garbled stuff on the screen*/
void playFrame(MOVE frame[12], char direction, char amount){
	char id, currentFrame;
	for(currentFrame = 0; currentFrame <= directionShift[direction]; currentFrame++){
		for(id = 0; id< amount; id++){
			drawBox(frame[id].xFrom,frame[id].yFrom, direction, currentFrame, frame[id].beforeValue,A_NORMAL);
		}
		for(id = 0; id< amount; id++){
			clearBox(frame[id].xFrom,frame[id].yFrom, direction, currentFrame);
		}
	}
}
//Shifts the entire grid in the specified direction, playing animations if specified. Also updates the score. Returns the new score
void shiftGrid(char gridIn[4][4], char direction, unsigned long int *score, char animations){
	MOVE frame[12];
	char isGridCompleted[4][4];
	memset(isGridCompleted,0,16);
	char rounds, xPos, yPos, start, end, change, currentMove;
	if (direction == UP || direction == LEFT){
		start = 0;
		end = 3;
		change = 1;
	}
	else{
		start = 3;
		end = 0;
		change = -1;
	}
	
	for(rounds = 0; rounds <= 3; rounds++){
		currentMove = 0;
		for(xPos = start; (xPos>=0) && (xPos <= 3); xPos += change){
			for(yPos = start; (yPos>=0) && (yPos <= 3); yPos += change){
				if(shiftTile(gridIn,xPos,yPos,direction,score,isGridCompleted)){
					frame[currentMove].xFrom = xPos;
					frame[currentMove].yFrom = yPos;
					frame[currentMove].beforeValue = gridIn[xPos][yPos];
					currentMove++;
				}
			}
		}
		if(animations && currentMove != 0){
			playFrame(frame,direction,currentMove);
			drawGrid(gridIn);
		}
	}
}

//draws the entire grid on the screen without animations
void drawGrid(char gridIn[4][4]){
	char row, col;
	for(row=0; row<=3; row++){
		for(col=0; col<=3; col++){
			if(gridIn[row][col] != 0){
				drawBox(row,col,0,0,gridIn[row][col], A_NORMAL);
			}
		}
	}
}

// Main Function
void _main( void ){
	//Create or load savefile
	SAVE gameState;
	if(readSave(&gameState))
		makeSave(&gameState);
	writeSave(&gameState); /*just to test whether writing is possible here
	so the player doesn't spend a long time and lose their progress*/
	//Buffer for the text at the bottom of the
	scoreString = (char*)malloc(60*sizeof(char));
	if(scoreString == NULL)
		exit(ER_MEMORY);
	
	//Pause menu
	pauseMenu = PopupNew(strConv[11], 50);
	if(pauseMenu == H_NULL)
		memkill(4, ER_MEMORY);
		PopupAddText(pauseMenu, -1, "New Game", 1);
		PopupAddText(pauseMenu, 0, "Options", 2);
			PopupAddText(pauseMenu,2,"Toggle Animations",5);
			PopupAddText(pauseMenu,2,"Reset Save Data",6);
			PopupAddText(pauseMenu, -1, "Exit", 3);
			PopupAddText(pauseMenu, -1, "About", 4);
	
	//Hiscore Name Entry
	nameBox = DialogNewSimple(140,35);
	if(nameBox == H_NULL)
		memkill(3, ER_MEMORY);
		DialogAddTitle(nameBox,"NEW HISCORE!",BT_OK,BT_NONE);
		DialogAddRequest(nameBox,3,14,"Name:", 0, 10, 14);
	
	//Game Over Entry
	gameOver = PopupNew("GAME OVER!", 40);
	if(gameOver == H_NULL)
		memkill(2, ER_MEMORY);
		PopupAddText(gameOver,-1,"Try Again",1);
		PopupAddText(gameOver,-1,"Exit",2);
	
	
	//About Dialouge
	aboutBox = DialogNewSimple(140,80);
	if(aboutBox == H_NULL)
		memkill(1, ER_MEMORY);	
		DialogAddTitle(aboutBox,"ABOUT",BT_OK,BT_NONE);

			DialogAddText(aboutBox, 3, 13, "2048: A sliding tile puzzle");
			DialogAddText(aboutBox, 3, 21, "Ti68k port by Harrison Cook");
			DialogAddText(aboutBox, 3, 29, "Original game by Gabriele Cirulli");
			DialogAddText(aboutBox, 3, 37, "\"Based on\" 1024 by Veewo Studio");
			DialogAddText(aboutBox, 3, 45, "\"Conceptually Similar to\":");
			DialogAddText(aboutBox, 3, 52, "Threes by Asher Vollmer");
			DialogAddText(aboutBox, 3, 59, "Built using TIGCC");
	
	ClrScr();
	FontSetSys(F_8x10);
	randomize();
	short int key;
	for(;;){
		ClrScr();
		drawGrid(gameState.grid);
		//DrawLine(0,LCD_HEIGHT-7,LCD_WIDTH,LCD_HEIGHT-7,A_NORMAL);
		sprintf(scoreString, "Score: %lu Hiscore: %lu by %s", gameState.score, gameState.bscore, gameState.name);
		if(!isGridAvailable(gameState.grid)){
			
			if(gameState.score > gameState.bscore){
					DialogDo(nameBox,CENTER,CENTER,	gameState.name,NULL);
					gameState.bscore = gameState.score;
					FontSetSys(F_8x10);
			}
			gameState.score = 0;
			initGrid(gameState.grid);
			switch(PopupDo(gameOver,CENTER,CENTER,0)){
			case 1:
				continue;
				break;
			default:
				goto endGame;
				
			}
		}
		ST_helpMsg(scoreString);
		ST_busy(ST_IDLE);
		key = ngetchx();
		ST_busy(ST_BUSY);
		ST_helpMsg(scoreString);
		char move = 4;
		if(key == KEY_UP)
			move = UP;
		else if(key == KEY_RIGHT)
			move = RIGHT;
		else if(key == KEY_DOWN)
			move = DOWN;
		else if(key == KEY_LEFT)
			move = LEFT;
		else if(key == KEY_OFF || key==KEY_ON)
			//Turn the calculator off during a game. Resume it later
			off();
		else if(key == KEY_ESC){ //Brings up the pause menu
			switch (PopupDo(pauseMenu,CENTER,CENTER,0)){
				case 1: //1. New Game
					gameState.score = 0;
					initGrid(gameState.grid);
					break;
				//case 2: //2. Options
					//this is a submenu
				case 3: //3: Exit
					endGame:
					writeSave(&gameState);
					memkill(0,0);
					break;
				case 4: //4: About
					DialogDo(aboutBox,CENTER,CENTER,NULL,NULL);
					//Dialog sets the font weirdly, set it back
					FontSetSys(F_8x10);
					break;
				case 5: //1. Toggle Animations
					gameState.animations = !gameState.animations;
					break;
				case 6: //2. Reset Saved Data
					makeSave(&gameState);
					break;
			}
		}
		if(move != 4 && isDirectionAvailable(gameState.grid, move)){
			shiftGrid(gameState.grid,move,&gameState.score, gameState.animations);
			pushNewTile(gameState.grid);
		}
	}
}
