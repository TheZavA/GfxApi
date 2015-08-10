#include <stdio.h>
#include <iostream>

#include "MainClass.h"

MainClass* g_pMainClass;

int main(int argc, char *argv[]) 
{

    try 
    {
        MainClass* g_pMainClass = new MainClass();
        g_pMainClass->mainLoop();
    } 
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}