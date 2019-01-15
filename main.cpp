#include <Windows.h>

#include "Engine.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "DirectXTK.lib")

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	Engine engine;
	
	if (engine.Initialize(hInstance, "ninEngine", "WindowClass1", SCREEN_WIDTH, SCREEN_HEIGHT)) {
		while (engine.ProcessMessages()) {
			engine.Update();
			engine.RenderFrame();
		}
	}

	return 0;
}