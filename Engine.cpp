#include "Engine.h"

bool Engine::Initialize(HINSTANCE hInstance, std::string window_title, std::string window_class, int width, int height)
{
	if (!this->render_window.Initialize(this, hInstance, window_title, window_class, width, height))
		return false;
	
	HWND hwnd = this->render_window.GetHWND();

	if (!gfx.Initialize(hwnd, width, height))
		return false;

	return true;
}

bool Engine::ProcessMessages()
{
	return this->render_window.ProcessMessages();
}

void Engine::Update()
{
	while (!keyboard.CharBufferIsEmpty())
	{
		unsigned char ch = keyboard.ReadChar();	
	}

	while (!keyboard.KeyBufferIsEmpty())
	{
		KeyboardEvent keyboardevent = keyboard.ReadKey();
		unsigned char keycode = keyboardevent.GetKeyCode();		
	}

	while (!mouse.EventBufferIsEmpty())
	{
		MouseEvent me = mouse.ReadEvent();		
	}
}

void Engine::RenderFrame()
{
	gfx.RenderFrame();
}
