// Uses #import to prevent looping includes
#import "./screen/lcd.c"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

void game_thread(void* pvParameters);
void displayGame();

// The value positionX is 0-19
// The value positionY is 0-3
// The value state indicates 0 = doesnt exist, 1 = hanging object, 2 = standing object.

typedef struct
{
    int positionX;
    int positionY;
    int state;
}gameObject;

gameObject player;
gameObject object;

#define true 1
#define false 0
#define boolean int

boolean playing = false;

int score;
int speed;
int highScore = 0;

extern int isClicked;

// Method call to start the game. Resets all values except for highScore. 

void game() {
    playing = false;

    clearScreen();
    score = 0;
    speed = 200;

    player.positionX = 4;
    player.positionY = 3;
    player.state = 1;

    object.state = 0;

    xTaskCreate(game_thread, "game_thread", 1024 * 8, NULL, 24, NULL);
}

// The Gamethread. Runs continually until the player lost the game.

void game_thread(void* pvParameters)
{
    playing = true;
    while (playing == true)
    {

        // Creates new object if no object exists. produces a random integer with the value 1 or 2.
        // When the value for the object is 2, it is a standing object. When the value for the object is 1, it is a hanging object.

        if (object.state == 0)
        {
            srand(time(NULL));
            int r = rand() % 2 + 1;
            
            char random[128];
            sprintf(random, "%s%d", "Random number: ", r);

            printf(random);

            object.state = r;
            object.positionX = 19;
            object.positionY = r + 1;
        }
        else
        {
            // Checks of object is leaving the screen. When this happens, it deletes the object.

            if (object.positionX == 1)
            {
                object.state = 0;
            }
            object.positionX = object.positionX - 1;

            // Checks what value isClicked is.
            // isClicked > 0    |   player jumps
            // isClicked == 0   |   player does nothing
            // isClicked < 0    |   player crouches

            if (isClicked > 0)
            {
                player.positionY = 2;
                isClicked--;
            }

            if (isClicked == 0)
            {
                player.positionY = 3;
            }

            if (isClicked < 0)
            {
                player.positionY = 4;
                isClicked++;
            }

            // Checks if the player touches the standing object. If this is true, the game ends.

            if (object.state == 2 && object.positionX == player.positionX && object.positionY <= player.positionY)
            {
                playing = false;
                displayGame();
                break;
            }

            // Checks if the player touches the hanging object. If this is true, the game ends.

            if (object.state == 1 && object.positionX == player.positionX && (object.positionY + 1) >= player.positionY)
            {
                playing = false;
                displayGame();
                break;
            }

            // Checks if player gets past object. Updates score when this happens
            // Checks if score > highscore. Updates highscore when this happens

            if (object.positionX == player.positionX)
            {
                score++;
                if (score > highScore)
                {
                    highScore = score;
                }
            }

            displayGame();

            // Formula to increase the speed of the game as you get a higher score. When score is 20, the speed stays constant

            vTaskDelay((speed - (score * 10)) + 50 / portTICK_RATE_MS);
        }
    }
    vTaskDelete(NULL);
}

// Method to display game on LCD screen. Dimensions of the LCD screen are 4x20.

void displayGame() {
    clearScreen();

    // Checks if object is hanging or standing, and displays the correct form.

    if (object.state == 2)
    {
        writeLineOnPosition(object.positionX, 3, "O");
    }

    if (object.state == 1)
    {
        writeLineOnPosition(object.positionX, 0, "|");
        writeLineOnPosition(object.positionX, 1, "|");
        writeLineOnPosition(object.positionX, 2, "O");
    }

    // Displays player

    writeLineOnPosition(player.positionX, player.positionY, "|");
    writeLineOnPosition(player.positionX, player.positionY - 1, "O");

    // Displays score and highscore
    // NOTE: Score and highscore can never get past 999, otherwise they will not fit on the screen. 

    char scoreString[128];
    sprintf(scoreString, "%s%d", "Score:", score);
    writeLineOnPosition(0, 0, scoreString);

    char highScoreString[128];
    sprintf(highScoreString, "%s%d", "HiScore:", highScore);
    writeLineOnPosition(9, 0, highScoreString);
}
