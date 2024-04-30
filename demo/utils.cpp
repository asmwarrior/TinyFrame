//
// Created by MightyPork on 2017/10/15.
//

#include "utils.hpp"
#include <cstdio>

#ifdef _WIN32 // Windows specific code
#include <Windows.h>
#endif // _WIN32

namespace TinyFrame_n{


#ifdef _WIN32 // Windows specific code

// Function to set the console text color
void SetConsoleColor(WORD color)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

// Helper function for testing
void dumpFrame(const uint8_t* buff, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++)
    {
        printf("%3u ", buff[i]);
        SetConsoleColor(FOREGROUND_BLUE | FOREGROUND_INTENSITY); // Set color to blue (foreground intensity)
        printf("%02X", buff[i]);
        SetConsoleColor(FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Reset color to default

        if (buff[i] >= 0x20 && buff[i] < 127)
        {
            printf(" %c", buff[i]);
        }
        else
        {
            SetConsoleColor(FOREGROUND_RED); // Set color to red
            printf(" .");
            SetConsoleColor(FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Reset color to default
        }
        printf("\n");
    }
    printf("--- end of frame ---\n\n");
}

void dumpFrameInfo(Msg* msg)
{
    SetConsoleColor(FOREGROUND_GREEN | FOREGROUND_RED); // Set color to yellow (green + red = yellow)
    printf("Frame info\n");
    SetConsoleColor(FOREGROUND_GREEN); // Set color to green
    printf("  type: 0x%Xh\n", (int)msg->type);
    printf("  data: \"%.*s\"\n", (int)msg->len, msg->data);
    printf("   len: 0x%X\n", (int)msg->len);
    printf("    id: 0x%Xh\n", (int)msg->frame_id);
    SetConsoleColor(FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Reset color to default
    printf("\n");
}

#else // Linux system

// helper func for testing
void dumpFrame(const uint8_t *buff, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) {
        printf("%3u \033[94m%02X\033[0m", buff[i], buff[i]);
        if (buff[i] >= 0x20 && buff[i] < 127) {
            printf(" %c", buff[i]);
        }
        else {
            printf(" \033[31m.\033[0m");
        }
        printf("\n");
    }
    printf("--- end of frame ---\n\n");
}

void dumpFrameInfo(Msg *msg)
{
    printf("\033[33mFrame info\n"
               "  type: 0x%Xh\n"
               "  data: \"%.*s\"\n"
               "   len: 0x%X\n"
               "    id: 0x%Xh\033[0m\n\n",
           (int)msg->type, (int)msg->len, msg->data, (int)msg->len, (int)msg->frame_id);
}

#endif // _WIN32

} // TinyFrame_n
