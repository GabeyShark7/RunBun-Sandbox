// i will only be making icons MYSELF. I love doing icons.

#include "IconManager.h"
#include "TextureLoader.h"

static unsigned int dropdownIconID = 0;
static bool loaded = false;

unsigned int GetDropdownIcon() {
    if (!loaded) {
        dropdownIconID = LoadTexture("icons_made/drop_down.png"); // this is the little dropdown icon
        loaded = true;
    }
    return dropdownIconID;
}