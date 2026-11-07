// i will only be making icons MYSELF. I love doing icons.

#include "ui/IconManager.h"
#include "render/TextureLoader.h"

static unsigned int dropdownIconID = 0;
static bool loaded = false;

unsigned int GetDropdownIcon() {
    if (!loaded) {
        dropdownIconID = LoadTexture("icons/drop_down.png"); // this is the little dropdown icon
        loaded = true;
    }
    return dropdownIconID;
}