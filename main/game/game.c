#import "./screen/lcd.c"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

void game_thread(void* pvParameters);
void displayGame();

// The value positionX is 0-19
// The value positionY is 0-3

typedef struct
{
    int positionX;
    int positionY;
    int exists;
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

void game() {
    clearScreen();
    score = 0;
    speed = 200;

    playing = false;

    player.positionX = 4;
    player.positionY = 3;
    player.exists = 1;

    object.exists = 0;

    xTaskCreate(game_thread, "game_thread", 1024 * 8, NULL, 24, NULL);
}


void game_thread(void* pvParameters)
{
    playing = true;
    while (playing == true)
    {

        // Creates new object if no object exists. produces a random integer with the value 1 or 2.
        // When the value for the object is 2, it is a standing object. When the value for the object is 1, it is a hanging object.

        if (object.exists == 0)
        {
            srand(time(NULL));
            int r = rand() % 2 + 1;
            
            char random[128];
            sprintf(random, "%s%d", "Random number: ", r);

            printf(random);

            object.exists = r;
            object.positionX = 19;
            object.positionY = r + 1;
        }
        else
        {

            if (object.positionX == 1)
            {
                object.exists = 0;
            }
            object.positionX = object.positionX - 1;

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

            if (object.exists == 2 && object.positionX == player.positionX && object.positionY <= player.positionY)
            {
                playing = false;
                displayGame();
                break;
            }

            // Checks if the player touches the hanging object. If this is true, the game ends.

            if (object.exists == 1 && object.positionX == player.positionX && (object.positionY + 1) >= player.positionY)
            {
                playing = false;
                displayGame();
                break;
            }

            if (object.positionX == player.positionX)
            {
                score++;
                if (score > highScore)
                {
                    highScore = score;
                }
            }

            displayGame();

            vTaskDelay((speed - (score * 10)) / portTICK_RATE_MS);
        }
    }
    vTaskDelete(NULL);
}

void displayGame() {
    clearScreen();

    if (object.exists == 2)
    {
        writeLineOnPosition(object.positionX, 3, "O");
    }

    if (object.exists == 1)
    {
        writeLineOnPosition(object.positionX, 0, "|");
        writeLineOnPosition(object.positionX, 1, "|");
        writeLineOnPosition(object.positionX, 2, "O");
    }

    writeLineOnPosition(player.positionX, player.positionY, "|");
    writeLineOnPosition(player.positionX, player.positionY - 1, "O");

    char scoreString[128];
    sprintf(scoreString, "%s%d", "Score:", score);
    writeLineOnPosition(0, 0, scoreString);

    char highScoreString[128];
    sprintf(highScoreString, "%s%d", "HiScore:", highScore);
    writeLineOnPosition(10, 0, highScoreString);
}
