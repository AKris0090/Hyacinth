#include "sdlwindow.h"

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
}

void SDLWindow::pollEvents(SDL_Event& event)
{
	if (event.type == SDL_EVENT_QUIT) {
		running = false;
	}
}
