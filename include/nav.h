#pragma once
#include "buttons.h"

enum NavState {
    NAV_DIRECTORY,
    NAV_PAGE_VIEW,
    NAV_LOADING,
    NAV_NODE_PAGES
};

void navInit();
void navRefreshDirectory();
void navHandleButton(ButtonEvent evt);
void navRender();
void navLoop();  // called every loop for async updates (loading state)
NavState navGetState();
