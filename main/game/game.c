#import "./screen/lcd.c"
#include <stdio.h>

void game_thread(void* pvParameters);
void displayGame();

// The value positionX is 0-19
// The value positionY is 0-3
// The value exits means 1 = exist, 0 != exist

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

extern int isClicked;

void game() {
    clearScreen();
    score = 0;
    speed = 200;

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

        // Creates new object if no object exists.

        if (object.exists == 0)
        {
            object.exists = 1;
            object.positionX = 19;
            object.positionY = 3;
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

            // Checks if the player touches the object. If this is true, the game ends.

            if (object.positionX == player.positionX && object.positionY == player.positionY)
            {
                //playing = false;
                //break;
            }

            displayGame();

            vTaskDelay(speed / portTICK_RATE_MS);
        }
    }
    vTaskDelete(NULL);
}

void displayGame() {
    clearScreen();

    if (object.exists == 1)
    {
        writeLineOnPosition(object.positionX, 3, "O");
    }

    writeLineOnPosition(player.positionX, player.positionY, "|");
    writeLineOnPosition(player.positionX, player.positionY - 1, "O");
}
