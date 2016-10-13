#pragma once

struct GLFWwindow;

class Input
{
public:
	Input(GLFWwindow* window);
	~Input(void);

	int keyPressed(int key);

	void poll();

private:
	GLFWwindow* m_pWindow;
};
