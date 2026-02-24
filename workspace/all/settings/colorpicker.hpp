#pragma once

#include "menu.hpp"
#include <vector>
#include <string>
#include <utility>

// A full-screen color picker that allows the user to compose a color via
// individual R, G, B sliders. Below the sliders a row of "in-use" color
// swatches lets the user load any existing theme color as a starting point.
//
// Navigation:
//   UP/DOWN     – move between the three slider rows and the swatch row
//   LEFT/RIGHT  – adjust the selected channel value by 1 (or navigate swatches)
//   L1/R1       – adjust the selected channel value by 16
//   A           – on swatch row: load the highlighted swatch into the sliders
//   B           – close and keep the current color
class ColorPickerMenu : public MenuList
{
    int r, g, b;          // current RGB channel values [0, 255]

    ValueSetCallback on_set;

    // in-use colors as (short label, 0xRRGGBB value) pairs
    std::vector<std::pair<std::string, uint32_t>> swatches;

    // 0 = R slider, 1 = G slider, 2 = B slider, 3 = swatch row
    int selected_row;
    int selected_swatch;

public:
    ColorPickerMenu(uint32_t initialColor,
                    ValueSetCallback on_set,
                    std::vector<std::pair<std::string, uint32_t>> swatches);

    // Re-initialise with a new base color and a fresh swatch list.
    // Call this just before deferring to the picker so that it always
    // reflects the live state of the settings.
    void reinit(uint32_t color,
                std::vector<std::pair<std::string, uint32_t>> new_swatches);

    InputReactionHint handleInput(int &dirty, int &quit) override;
    void drawCustom(SDL_Surface *surface, const SDL_Rect &dst) override;

private:
    uint32_t currentColor() const;
    void applyColor();

    void drawColorSquare(SDL_Surface *surface, const SDL_Rect &rect) const;

    // Draw a single slider row.
    // channel_color: the fill color used for the bar (red/green/blue tint)
    void drawSlider(SDL_Surface *surface, const SDL_Rect &rect,
                    const char *label, SDL_Color channel_color,
                    int value, bool selected) const;

    // Draw the row of in-use color swatches.
    void drawSwatches(SDL_Surface *surface, const SDL_Rect &rect,
                      bool row_selected) const;
};
