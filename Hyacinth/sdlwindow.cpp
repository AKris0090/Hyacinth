#include "sdlwindow.h"
#pragma comment(lib, "Dwmapi.lib") 

void enableDarkTitleBar(SDL_Window* window) {
	SDL_PropertiesID props = SDL_GetWindowProperties(window);
	HWND hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
	if (hwnd) {
		DWORD useDark = 1;
		DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
	}
}

SDLWindow::~SDLWindow()
{
	SDL_DestroyWindow(m_window);
}

void SDLWindow::init(std::string title, int width, int height)
{
	std::cout << ":::    ::: :::   :::   :::      :::::::: ::::::::::: ::::    ::: ::::::::::: :::    :::" << std::endl;
	std::cout << ":+:    :+: :+:   :+: :+: :+:   :+:    :+:    :+:     :+:+:   :+:     :+:     :+:    :+:" << std::endl;
	std::cout << "+:+    +:+  +:+ +:+ +:+   +:+  +:+           +:+     :+:+:+  +:+     +:+     +:+    +:+" << std::endl;
	std::cout << "+#++:++#++   +#++: +#++:++#++: +#+           +#+     +#+ +:+ +#+     +#+     +#++:++#++" << std::endl;
	std::cout << "+#+    +#+    +#+  +#+     +#+ +#+           +#+     +#+  +#+#+#     +#+     +#+    +#+" << std::endl;
	std::cout << "#+#    #+#    #+#  #+#     #+# #+#    #+#    #+#     #+#   #+#+#     #+#     #+#    #+#" << std::endl;
	std::cout << "###    ###    ###  ###     ###  ######## ########### ###    ####     ###     ###    ###" << std::endl;
	std::cout << std::endl;
	std::cout << std::endl;

	SDL_Init(SDL_INIT_VIDEO);
	m_window = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_SetWindowRelativeMouseMode(m_window, true);

	enableDarkTitleBar(m_window);
}

void SDLWindow::pollEvents(SDL_Event& event)
{
	if (event.type == SDL_EVENT_QUIT) {
		running = false;
	}
}
