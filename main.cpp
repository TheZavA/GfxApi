#include <stdio.h>
#include <iostream>

#include "MainClass.h"

int main(int argc, char *argv[]) 
{
    try 
    {
        MainClass* mainApp = new MainClass();
        mainApp->mainLoop();
    } 
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}