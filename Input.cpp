#include "Input.h"

#include "glcommon.h"

Input::Input(GLFWwindow* window)
	: m_pWindow(window)
{
}


Input::~Input(void)
{
}


int Input::keyPressed(int key)
{
	return glfwGetKey(m_pWindow, key);
}


void Input::poll()
{
	glfwPollEvents();
}