#pragma once

struct PanelCollapseState {
    bool collapsed = false;
    float expandedHeight = 300.0f;
};

bool DrawPanelHeader(PanelCollapseState& state);
