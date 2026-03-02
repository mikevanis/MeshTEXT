#pragma once
#include "buttons.h"

enum NavState {
    NAV_DIRECTORY,
    NAV_PAGE_VIEW
};

void navInit();
void navRefreshDirectory();
void navHandleButton(ButtonEvent evt);
void navRender();
NavState navGetState();
