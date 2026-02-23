#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COLS 10
#define ROWS 10
#define CELL_SIZE 40
#define STATUS_BAR_HEIGHT 40
#define MAX_INPUT_CHARS 9

// state machine
typedef enum GameState { STATE_MENU, STATE_DIFFICULTY, STATE_GAME, STATE_HIGHSCORES, STATE_INPUT } GameState;

typedef struct {
	char name[MAX_INPUT_CHARS + 1];
	int score;
} HighScore;

// game cell
typedef struct Cell {
	bool hasMine, isRevealed, isFlagged;
	int neighborCount;
} Cell;

// game field
Cell grid[COLS][ROWS];
GameState currentState = STATE_MENU;
bool gameOver = false, gameWon = false;
Texture2D mineTexture, flagTexture;
Sound winSound, loseSound, clickSound;

// top score
HighScore topPlayers[5] = { {"Duke Nukem", 15}, {"Doom guy", 10}, {"Ranger", 5}, {"Peasant", 3}, {"Chicken", 1} };
char playerName[MAX_INPUT_CHARS + 1] = "\0";
int letterCount = 0;
int minesToPlace = 0;
double startTime = 0;
int finalTimeInSeconds = 0;

// fills the field with mines
void initGame(int mineCount) {
	gameOver = false;
	gameWon = false;
	for (int i = 0; i < COLS; i++) {
		for (int j = 0; j < ROWS; j++) {
			grid[i][j] = (Cell){0};
		}
	}

	int m = 0;
	while (m < mineCount) {
		int rx = rand() % COLS, ry = rand() % ROWS;
		if (!grid[rx][ry].hasMine) { 
			grid[rx][ry].hasMine = true;
		       	m++;
	       	}
	}

	for (int x = 0; x < COLS; x++) {
		for (int y = 0; y < ROWS; y++) {
			if (grid[x][y].hasMine) {
			       	continue;
			}
			int c = 0;
			for (int i = -1; i <= 1; i++) { 
				for (int j = -1; j <= 1; j++) {
					int nx = x + i, ny = y + j;
					if (nx >= 0 && nx < COLS && ny >= 0 && ny < ROWS && grid[nx][ny].hasMine) {
					       	c++;
					}
				}
			}

			grid[x][y].neighborCount = c;
		}
	}
}

void revealCell(int x, int y) {
	if (x < 0 || x >= COLS || y < 0 || y >= ROWS || grid[x][y].isRevealed) {
		return;
	}

	grid[x][y].isRevealed = true;
	if (grid[x][y].neighborCount == 0 && !grid[x][y].hasMine) {
		for (int i = -1; i <= 1; i++) {
			for (int j = -1; j <= 1; j++) {
				revealCell(x + i, y + j);
			}
		}
	}
}

void revealAllMines() {
	for (int x = 0; x < COLS; x++) {
		for (int y = 0; y < ROWS; y++) {
			if (grid[x][y].hasMine) { 
			       grid[x][y].isRevealed = true;
			}
		}
	}
}

bool checkWin() {
	for (int x = 0; x < COLS; x++) {
		for (int y = 0; y < ROWS; y++) {
			if (!grid[x][y].hasMine && !grid[x][y].isRevealed) {
				return false;
			}
		}
	}

	return true;
}

int countFlags() {
	int count = 0;
	for (int i = 0; i < COLS; i++) {
		for (int j = 0; j < ROWS; j++) {
			if (grid[i][j].isFlagged && !grid[i][j].isRevealed) {
				count++;
			}
		}
	}

	return count;
}

void addScore(const char* name, int score) {
	for (int i = 0; i < 5; i++) {
		if (score > topPlayers[i].score) {
			for (int j = 4; j > i; j--) {
				topPlayers[j] = topPlayers[j-1];
			}

			strcpy(topPlayers[i].name, name);
			topPlayers[i].score = score;

			break;
		}
	}
}

int main() {
	InitWindow(COLS * CELL_SIZE, (ROWS * CELL_SIZE) + STATUS_BAR_HEIGHT, "Minesweeper");
	InitAudioDevice(); 
	SetExitKey(KEY_NULL);
	SetTargetFPS(60);
	srand(time(NULL));

	mineTexture = LoadTexture("mine.jpg");
	flagTexture = LoadTexture("flag.jpg");
	winSound = LoadSound("win.wav");
	loseSound = LoadSound("explosion.wav");
	clickSound = LoadSound("click.wav");

	while (!WindowShouldClose()) {
		Vector2 mousePosition = GetMousePosition();
		
		switch (currentState) {
			case STATE_MENU:
				if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
					if (CheckCollisionPointRec(mousePosition, (Rectangle){ 80, 140, 240, 50 })) {
					       	currentState = STATE_DIFFICULTY;
					}
					if (CheckCollisionPointRec(mousePosition, (Rectangle){ 80, 210, 240, 50 })) {
						currentState = STATE_HIGHSCORES;
					}
				}
				break;

			case STATE_DIFFICULTY:
				if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
					if (CheckCollisionPointRec(mousePosition, (Rectangle){ 80, 100, 240, 50 })) { 
						minesToPlace = 10;
					} else if (CheckCollisionPointRec(mousePosition, (Rectangle){ 80, 170, 240, 50 })) {
					       	minesToPlace = 15;
					} else if (CheckCollisionPointRec(mousePosition, (Rectangle){ 80, 240, 240, 50 })) { 
						minesToPlace = 20;
					} else {
					       	break;
					}
					
					initGame(minesToPlace);
					startTime = GetTime();
					currentState = STATE_GAME;
				}

				if (IsKeyPressed(KEY_ESCAPE)) {
					currentState = STATE_MENU;
				}

				break;

			case STATE_GAME:
				if (!gameOver && !gameWon) {
					int x = (int)mousePosition.x / CELL_SIZE;
					int y = (int)mousePosition.y / CELL_SIZE;
					
					if (x >= 0 && x < COLS && y >= 0 && y < ROWS) {
						if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !grid[x][y].isFlagged && !grid[x][y].isRevealed) {
							if (grid[x][y].hasMine) { 
								gameOver = true; 
								PlaySound(loseSound);
								revealAllMines(); 
							} else { 
								PlaySound(clickSound);
								revealCell(x, y); 
								if (checkWin()) { 
									gameWon = true; 
									PlaySound(winSound);
									finalTimeInSeconds = (int)(GetTime() - startTime);
									currentState = STATE_INPUT; 
								} 
							}
						}

						if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && !grid[x][y].isRevealed) {
							PlaySound(clickSound); 
							grid[x][y].isFlagged = !grid[x][y].isFlagged;
						}
					}
				} else if (IsKeyPressed(KEY_R)) {
					initGame(minesToPlace);
					startTime = GetTime();
				}

				if (IsKeyPressed(KEY_ESCAPE)) {
					currentState = STATE_MENU;
				}

				break;

			case STATE_INPUT: {
				int key = GetCharPressed();
				while (key > 0) {
					if ((key >= 32) && (key <= 125) && (letterCount < MAX_INPUT_CHARS)) {
						playerName[letterCount] = (char)key;
						playerName[letterCount+1] = '\0';
						letterCount++;
					}
					key = GetCharPressed();
				}

				if (IsKeyPressed(KEY_BACKSPACE) && letterCount > 0) {
				       	letterCount--; playerName[letterCount] = '\0';
			       	}

				if (IsKeyPressed(KEY_ENTER) && letterCount > 0) {
					int penalty = finalTimeInSeconds / 60;
					int finalScore = (minesToPlace - penalty < 0) ? 0 : minesToPlace - penalty;
					addScore(playerName, finalScore);
					currentState = STATE_HIGHSCORES;
					playerName[0] = '\0'; letterCount = 0;
				}

				break;
			}

			case STATE_HIGHSCORES:
				if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePosition, (Rectangle){ 80, 320, 240, 50 })) {
					currentState = STATE_MENU;
				}

				if (IsKeyPressed(KEY_ESCAPE)) {
					currentState = STATE_MENU;
				}

				break;
		}

		BeginDrawing();
			ClearBackground(RAYWHITE);
			
			if (currentState == STATE_MENU) {
				DrawText("MINESWEEPER", 60, 60, 40, DARKGRAY);
				DrawRectangleRec((Rectangle){ 80, 140, 240, 50 }, LIGHTGRAY);
			       	DrawText("START GAME", 135, 155, 20, BLACK);
				DrawRectangleRec((Rectangle){ 80, 210, 240, 50 }, LIGHTGRAY);
			       	DrawText("TOP PLAYERS", 135, 225, 20, BLACK);
			} else if (currentState == STATE_DIFFICULTY) {
				DrawText("SELECT DIFFICULTY", 65, 40, 25, DARKGRAY);
				DrawRectangleRec((Rectangle){ 80, 100, 240, 50 }, GREEN);
			       	DrawText("EASY (10)", 145, 115, 20, WHITE);
				DrawRectangleRec((Rectangle){ 80, 170, 240, 50 }, ORANGE);
			       	DrawText("NORMAL (15)", 135, 185, 20, WHITE);
				DrawRectangleRec((Rectangle){ 80, 240, 240, 50 }, RED);
			       	DrawText("HARD (20)", 145, 255, 20, WHITE);
			} else if (currentState == STATE_GAME) {
				for (int i = 0; i < COLS; i++) {
					for (int j = 0; j < ROWS; j++) {
						Rectangle dest = { i * CELL_SIZE, j * CELL_SIZE, CELL_SIZE - 2, CELL_SIZE - 2 };
						if (grid[i][j].isRevealed) {
							DrawRectangleRec(dest, LIGHTGRAY);
							if (grid[i][j].hasMine) {
								DrawTexturePro(mineTexture, (Rectangle){0,0,mineTexture.width,mineTexture.height}, dest, (Vector2){0,0}, 0, WHITE);
							} else if (grid[i][j].neighborCount > 0) {
								DrawText(TextFormat("%d", grid[i][j].neighborCount), i * CELL_SIZE + 15, j * CELL_SIZE + 10, 20, DARKGRAY);
							}
						} else {
							DrawRectangleRec(dest, GRAY);
							if (grid[i][j].isFlagged) {
								DrawTexturePro(flagTexture, (Rectangle){0,0,flagTexture.width,flagTexture.height}, dest, (Vector2){0,0}, 0, WHITE);
							}
						}
					}
				}
				DrawRectangle(0, ROWS*CELL_SIZE, GetScreenWidth(), STATUS_BAR_HEIGHT, DARKGRAY);
				int timeNow = (gameOver || gameWon) ? finalTimeInSeconds : (int)(GetTime() - startTime);
				DrawText(TextFormat("MINES: %d", minesToPlace - countFlags()), 15, ROWS*CELL_SIZE+10, 18, WHITE);
				DrawText(TextFormat("TIME: %03ds", timeNow), 280, ROWS*CELL_SIZE+10, 18, WHITE);

				if (gameOver) {
					DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));
					DrawText("WASTED", (GetScreenWidth() - MeasureText("WASTED", 60)) / 2, GetScreenHeight() / 2 - 50, 60, RED);
					DrawText("Press [R] to Restart", (GetScreenWidth() - MeasureText("Press [R] to Restart", 20)) / 2, GetScreenHeight() / 2 + 20, 20, RAYWHITE);
				}

			} else if (currentState == STATE_INPUT) {
				DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.8f));
				DrawText("YOU WON!", 140, 50, 30, GREEN);
				DrawText("ENTER YOUR NAME:", 110, 130, 15, RAYWHITE);
				DrawRectangle(80, 160, 240, 50, RAYWHITE);
				DrawText(playerName, 95, 175, 25, MAROON);
			} else if (currentState == STATE_HIGHSCORES) {
				DrawText("TOP PLAYERS", 80, 40, 30, DARKGRAY);
				for (int i = 0; i < 5; i++) {
					 DrawText(TextFormat("%d. %s - %d pts", i+1, topPlayers[i].name, topPlayers[i].score), 80, 100 + (i*40), 20, BLACK);
				}

				DrawRectangleRec((Rectangle){ 80, 320, 240, 50 }, LIGHTGRAY);
				DrawText("BACK", 175, 335, 20, BLACK);
			}
		EndDrawing();
	}

	UnloadSound(winSound); UnloadSound(loseSound); UnloadSound(clickSound);
	CloseAudioDevice();
	UnloadTexture(mineTexture); UnloadTexture(flagTexture);
	CloseWindow();
	return 0;
}
