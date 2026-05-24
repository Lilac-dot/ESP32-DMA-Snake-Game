#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "esp_timer.h"


#define TFT_MOSI   23
#define TFT_SCLK   18
#define TFT_CS      5
#define TFT_DC      2
#define TFT_RST     4


#define BTN_UP      32
#define BTN_DOWN    33
#define BTN_LEFT    25
#define BTN_RIGHT   26

//tft dimensions
#define TFT_WIDTH   128
#define TFT_HEIGHT  160

//colors
#define COLOR_BG       0x001F
#define COLOR_BORDER   0xF81F
#define COLOR_SNAKE    0xFFE0
#define COLOR_FOOD     0xFFFF
#define COLOR_TEXT     0xFFFF
#define COLOR_BLACK    0x0000
#define COLOR_WHITE    0xFFFF
#define COLOR_YELLOW   0xFFE0


#define GRID_SIZE 8
#define GRID_W (TFT_WIDTH / GRID_SIZE)
#define GRID_H (TFT_HEIGHT / GRID_SIZE)

spi_device_handle_t spi;

uint16_t framebuffer[TFT_WIDTH * TFT_HEIGHT];

enum GameState
{
    MENU,
    PLAYING,
    GAME_OVER
};

GameState currentState = MENU;

//for snake
struct Point
{
    int x;
    int y;
};

Point snake[320];
Point food;

int snakeLength = 3;
int dirX = 1;
int dirY = 0;
int score = 0;
int highScore = 0;

//timing
uint64_t lastMove = 0;
int moveDelay = 120000;

//fps
int fps = 0;
int frames = 0;
uint64_t lastFPS = 0;

//font
// SPACE = 0
// 0-9 = 1-10
// A-Z = 11-36

const uint8_t font5x7[][5] =
{
    // SPACE
    {0x00,0x00,0x00,0x00,0x00},
    // 0
    {0x3E,0x51,0x49,0x45,0x3E},
    // 1
    {0x00,0x42,0x7F,0x40,0x00},
    // 2
    {0x42,0x61,0x51,0x49,0x46},
    // 3
    {0x21,0x41,0x45,0x4B,0x31},
    // 4
    {0x18,0x14,0x12,0x7F,0x10},
    // 5
    {0x27,0x45,0x45,0x45,0x39},
    // 6
    {0x3C,0x4A,0x49,0x49,0x30},
    // 7
    {0x01,0x71,0x09,0x05,0x03},
    // 8
    {0x36,0x49,0x49,0x49,0x36},
    // 9
    {0x06,0x49,0x49,0x29,0x1E},
    // A
    {0x7E,0x11,0x11,0x11,0x7E},
    // B
    {0x7F,0x49,0x49,0x49,0x36},
    // C
    {0x3E,0x41,0x41,0x41,0x22},
    // D
    {0x7F,0x41,0x41,0x22,0x1C},
    // E
    {0x7F,0x49,0x49,0x49,0x41},
    // F
    {0x7F,0x09,0x09,0x09,0x01},
    // G
    {0x3E,0x41,0x49,0x49,0x7A},
    // H
    {0x7F,0x08,0x08,0x08,0x7F},
    // I
    {0x00,0x41,0x7F,0x41,0x00},
    // J
    {0x20,0x40,0x41,0x3F,0x01},
    // K
    {0x7F,0x08,0x14,0x22,0x41},
    // L
    {0x7F,0x40,0x40,0x40,0x40},
    // M
    {0x7F,0x02,0x04,0x02,0x7F},
    // N
    {0x7F,0x04,0x08,0x10,0x7F},
    // O
    {0x3E,0x41,0x41,0x41,0x3E},
    // P
    {0x7F,0x09,0x09,0x09,0x06},
    // Q
    {0x3E,0x41,0x51,0x21,0x5E},
    // R
    {0x7F,0x09,0x19,0x29,0x46},
    // S
    {0x46,0x49,0x49,0x49,0x31},
    // T
    {0x01,0x01,0x7F,0x01,0x01},
    // U
    {0x3F,0x40,0x40,0x40,0x3F},
    // V
    {0x1F,0x20,0x40,0x20,0x1F},
    // W
    {0x7F,0x20,0x18,0x20,0x7F},
    // X
    {0x63,0x14,0x08,0x14,0x63},
    // Y
    {0x03,0x04,0x78,0x04,0x03},
    // Z
    {0x61,0x51,0x49,0x45,0x43}
};

//spi
void sendCommand(uint8_t cmd)
{
    gpio_set_level((gpio_num_t)TFT_DC, 0);
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_transmit(spi, &t);
}


void sendData(const uint8_t *data, int len)
{
    gpio_set_level((gpio_num_t)TFT_DC, 1);
    spi_transaction_t t = {};
    t.length = len * 8;
    t.tx_buffer = data;
    spi_device_transmit(spi, &t);
}

//display init
void initDisplay()
{
    gpio_set_level((gpio_num_t)TFT_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level((gpio_num_t)TFT_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    sendCommand(0x01);
    vTaskDelay(pdMS_TO_TICKS(150));
    sendCommand(0x11);
    vTaskDelay(pdMS_TO_TICKS(150));
    sendCommand(0x3A);

    uint8_t colorMode = 0x05;

    sendData(&colorMode, 1);
    sendCommand(0x36);

    uint8_t madctl = 0xC8;

    sendData(&madctl, 1);
    sendCommand(0x29);
    vTaskDelay(pdMS_TO_TICKS(100));
}

//address window
void setAddrWindow(
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1)
{
    uint8_t data[4];

    sendCommand(0x2A);

    data[0] = x0 >> 8;
    data[1] = x0 & 0xFF;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xFF;

    sendData(data, 4);

    sendCommand(0x2B);

    data[0] = y0 >> 8;
    data[1] = y0 & 0xFF;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xFF;

    sendData(data, 4);

    sendCommand(0x2C);
}


void pushFramebuffer()
{
    setAddrWindow(
        0,
        0,
        TFT_WIDTH - 1,
        TFT_HEIGHT - 1);

    gpio_set_level((gpio_num_t)TFT_DC, 1);

    static spi_transaction_t trans;

    memset(&trans, 0, sizeof(trans));

    trans.length =
        TFT_WIDTH * TFT_HEIGHT * 16;

    trans.tx_buffer = framebuffer;

    spi_device_queue_trans(
        spi,
        &trans,
        portMAX_DELAY);

    spi_transaction_t *rtrans;

    spi_device_get_trans_result(
        spi,
        &rtrans,
        portMAX_DELAY);
}

void drawPixel(
    int x,
    int y,
    uint16_t color)
{
    if (x < 0 || x >= TFT_WIDTH)
        return;

    if (y < 0 || y >= TFT_HEIGHT)
        return;

    framebuffer[y * TFT_WIDTH + x] = color;
}


void fillScreen(uint16_t color)
{
    for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++)
    {
        framebuffer[i] = color;
    }
}

void fillRect(
    int x,
    int y,
    int w,
    int h,
    uint16_t color)
{
    for (int py = y; py < y + h; py++)
    {
        for (int px = x; px < x + w; px++)
        {
            drawPixel(px, py, color);
        }
    }
}


void drawBorder()
{
    for (int x = 0; x < TFT_WIDTH; x++)
    {
        drawPixel(x, 0, COLOR_BORDER);

        drawPixel(x, TFT_HEIGHT - 1, COLOR_BORDER);
    }

    for (int y = 0; y < TFT_HEIGHT; y++)
    {
        drawPixel(0, y, COLOR_BORDER);

        drawPixel(TFT_WIDTH - 1, y, COLOR_BORDER);
    }
}

void drawChar(
    int x,
    int y,
    char c,
    uint16_t color)
{
    int index = 0;
    //space
    if (c == ' ')
    {
        index = 0;
    }

    //numbers
    else if (c >= '0' && c <= '9')
    {
        index = (c - '0') + 1;
    }

    //letters
    else if (c >= 'A' && c <= 'Z')
    {
        index = (c - 'A') + 11;
    }

    else
    {
        return;
    }

    for (int col = 0; col < 5; col++)
    {
        uint8_t line = font5x7[index][col];

        for (int row = 0; row < 7; row++)
        {
            if (line & (1 << row))
            {
                drawPixel(
                    x + col,
                    y + row,
                    color);
            }
        }
    }
}
    

void drawString(
    int x,
    int y,
    const char* text,
    uint16_t color)
{
    while (*text)
    {
        drawChar(
            x,
            y,
            *text,
            color);

        x += 6;

        text++;
    }
}


void drawNumber(
    int x,
    int y,
    int value,
    uint16_t color)
{
    char buffer[16];

    sprintf(buffer, "%d", value);

    drawString(x, y, buffer, color);
}

void spawnFood()
{
    food.x = rand() % GRID_W;
    food.y = rand() % GRID_H;
}


//reset game
void resetGame()
{
    snakeLength = 3;

    snake[0] = {5, 5};
    snake[1] = {4, 5};
    snake[2] = {3, 5};

    dirX = 1;
    dirY = 0;
    score = 0;
    moveDelay = 120000;

    spawnFood();
    fillScreen(COLOR_BG);
    drawBorder();

    for (int i = 0; i < snakeLength; i++)
    {
        fillRect(
            snake[i].x * GRID_SIZE,
            snake[i].y * GRID_SIZE,
            GRID_SIZE - 1,
            GRID_SIZE - 1,
            COLOR_SNAKE);
    }

    fillRect(
        food.x * GRID_SIZE,
        food.y * GRID_SIZE,
        GRID_SIZE - 1,
        GRID_SIZE - 1,
        COLOR_FOOD);

    pushFramebuffer();
    currentState = PLAYING;
}


bool anyButtonPressed()
{
    return !gpio_get_level((gpio_num_t)BTN_UP) ||
           !gpio_get_level((gpio_num_t)BTN_DOWN) ||
           !gpio_get_level((gpio_num_t)BTN_LEFT) ||
           !gpio_get_level((gpio_num_t)BTN_RIGHT);
}


void handleInput()
{
    if (!gpio_get_level((gpio_num_t)BTN_UP)
        && dirY != 1)
    {
        dirX = 0;
        dirY = -1;
    }

    if (!gpio_get_level((gpio_num_t)BTN_DOWN)
        && dirY != -1)
    {
        dirX = 0;
        dirY = 1;
    }

    if (!gpio_get_level((gpio_num_t)BTN_LEFT)
        && dirX != 1)
    {
        dirX = -1;
        dirY = 0;
    }

    if (!gpio_get_level((gpio_num_t)BTN_RIGHT)
        && dirX != -1)
    {
        dirX = 1;
        dirY = 0;
    }
}


void moveSnake()
{
    Point oldTail = snake[snakeLength - 1];
    Point newHead;
    newHead.x = snake[0].x + dirX;
    newHead.y = snake[0].y + dirY;

    if (newHead.x < 0 ||
        newHead.x >= GRID_W ||
        newHead.y < 0 ||
        newHead.y >= GRID_H)
    {
        currentState = GAME_OVER;
        if (score > highScore)
        {
            highScore = score;
        }

        return;
    }

    for (int i = 0; i < snakeLength; i++)
    {
        if (snake[i].x == newHead.x &&
            snake[i].y == newHead.y)
        {
            currentState = GAME_OVER;

            if (score > highScore)
            {
                highScore = score;
            }

            return;
        }
    }

    //erase old tail
    fillRect(
        oldTail.x * GRID_SIZE,
        oldTail.y * GRID_SIZE,
        GRID_SIZE - 1,
        GRID_SIZE - 1,
        COLOR_BG);

    //move body
    for (int i = snakeLength; i > 0; i--)
    {
        snake[i] = snake[i - 1];
    }
    snake[0] = newHead;

    //draw new head
    fillRect(
        snake[0].x * GRID_SIZE,
        snake[0].y * GRID_SIZE,
        GRID_SIZE - 1,
        GRID_SIZE - 1,
        COLOR_SNAKE);

        //food
    if (newHead.x == food.x &&
        newHead.y == food.y)
    {
        snakeLength++;
        score++;

        if (moveDelay > 50000)
        {
            moveDelay -= 3000;
        }

        spawnFood();

        fillRect(
            food.x * GRID_SIZE,
            food.y * GRID_SIZE,
            GRID_SIZE - 1,
            GRID_SIZE - 1,
            COLOR_FOOD);
    }
}

void drawMenu()
{
    fillScreen(COLOR_BG);

    drawBorder();

    fillRect(20, 20, 88, 25, COLOR_BORDER);

    drawString(30, 30, "ESP32 SNAKE", COLOR_YELLOW);

    drawString(18, 70, "PRESS BUTTON", COLOR_WHITE);

    drawString(35, 90, "TO START", COLOR_WHITE);

    pushFramebuffer();
}


void drawGame()
{
    drawString(5, 5, "SCORE", COLOR_WHITE);
    drawNumber(45, 5, score, COLOR_YELLOW);
    drawString(5, 18, "HIGH", COLOR_WHITE);
    drawNumber(45, 18, highScore, COLOR_YELLOW);

    if (currentState == GAME_OVER)
    {
        fillRect(15, 50, 95, 35, COLOR_BORDER);
        drawString(25, 60, "GAME OVER", COLOR_YELLOW);
        drawString(15, 75, "PRESS BUTTON", COLOR_WHITE);
    }
}


void updateFPS()
{
    static uint64_t frameStart = 0;
    uint64_t now = esp_timer_get_time();
    //first frame
    if (frameStart == 0)
    {
        frameStart = now;

        return;
    }

    uint64_t frameTime = now - frameStart;
    fps = 1000000.0f / frameTime;
    frameStart = now;
    static int printCounter = 0;
    printCounter++;

    if (printCounter >= 30)
    {
        printCounter = 0;

        printf(
            "FPS: %.2f | Score: %d | HighScore: %d | SnakeLength: %d\n",
            (float)fps,
            score,
            highScore,
            snakeLength
        );
    }
}

void gameTask(void *arg)
{
    while (true)
    {
        switch (currentState)
        {
            case MENU:

                drawMenu();

                if (anyButtonPressed())
                {
                    vTaskDelay(pdMS_TO_TICKS(150));

                    resetGame();
                }

                break;


            case PLAYING:

                handleInput();

                if (esp_timer_get_time() - lastMove >= moveDelay)
                {
                    moveSnake();

                    lastMove = esp_timer_get_time();
                }

                drawGame();

                pushFramebuffer();

                break;


            case GAME_OVER:

                drawGame();

                pushFramebuffer();

                if (anyButtonPressed())
                {
                    vTaskDelay(pdMS_TO_TICKS(150));

                    resetGame();
                }

                break;
        }

        updateFPS();

        vTaskDelay(pdMS_TO_TICKS(16));
    }
}


extern "C" void app_main()
{
    gpio_config_t inputConfig = {};

    inputConfig.mode = GPIO_MODE_INPUT;

    inputConfig.pull_up_en = GPIO_PULLUP_ENABLE;

    inputConfig.pin_bit_mask =
        (1ULL << BTN_UP) |
        (1ULL << BTN_DOWN) |
        (1ULL << BTN_LEFT) |
        (1ULL << BTN_RIGHT);

    gpio_config(&inputConfig);



    gpio_set_direction(
        (gpio_num_t)TFT_DC,
        GPIO_MODE_OUTPUT);

    gpio_set_direction(
        (gpio_num_t)TFT_RST,
        GPIO_MODE_OUTPUT);

    gpio_set_direction(
        (gpio_num_t)TFT_CS,
        GPIO_MODE_OUTPUT);



    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = TFT_MOSI;
    buscfg.miso_io_num = -1;
    buscfg.sclk_io_num = TFT_SCLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz =
        TFT_WIDTH * TFT_HEIGHT * 2;



    spi_bus_initialize(
        SPI2_HOST,
        &buscfg,
        SPI_DMA_CH_AUTO);



    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 27000000;
    devcfg.mode = 0;
    devcfg.spics_io_num = TFT_CS;
    devcfg.queue_size = 7;


    spi_bus_add_device(
        SPI2_HOST,
        &devcfg,
        &spi);



    initDisplay();



    xTaskCreatePinnedToCore(
        gameTask,
        "gameTask",
        4096,
        NULL,
        1,
        NULL,
        1);
}