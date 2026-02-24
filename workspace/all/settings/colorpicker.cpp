#include "colorpicker.hpp"

extern "C"
{
#include "defines.h"
#include "api.h"
}

#include <algorithm>
#include <cstdio>

// ── helpers ───────────────────────────────────────────────────────────────────

static inline void unpackColor(uint32_t color, int &r, int &g, int &b)
{
    r = (color >> 16) & 0xff;
    g = (color >> 8) & 0xff;
    b = color & 0xff;
}

// ── construction ─────────────────────────────────────────────────────────────

ColorPickerMenu::ColorPickerMenu(uint32_t initialColor,
                                 ValueSetCallback set_cb,
                                 std::vector<std::pair<std::string, uint32_t>> swatches_list)
    : MenuList(MenuItemType::Custom, "", {})
    , on_set(set_cb)
    , swatches(std::move(swatches_list))
    , selected_row(0)
    , selected_swatch(0)
{
    unpackColor(initialColor, r, g, b);
}

void ColorPickerMenu::reinit(uint32_t color,
                              std::vector<std::pair<std::string, uint32_t>> new_swatches)
{
    unpackColor(color, r, g, b);
    swatches = std::move(new_swatches);
    selected_row = 0;
    selected_swatch = 0;
}

// ── internal helpers ─────────────────────────────────────────────────────────

uint32_t ColorPickerMenu::currentColor() const
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

void ColorPickerMenu::applyColor()
{
    if (on_set)
        on_set(currentColor());
}

// ── input handling ────────────────────────────────────────────────────────────

InputReactionHint ColorPickerMenu::handleInput(int &dirty, int &quit)
{
    const int num_swatches = (int)swatches.size();
    const int max_row = (num_swatches > 0) ? 3 : 2;

    // ── Row navigation ──────────────────────────────────────────────────────
    if (PAD_justPressed(BTN_UP))
    {
        if (selected_row > 0)
        {
            selected_row--;
            dirty = 1;
        }
        return NoOp;
    }
    else if (PAD_justPressed(BTN_DOWN))
    {
        if (selected_row < max_row)
        {
            selected_row++;
            dirty = 1;
        }
        return NoOp;
    }

    // ── Slider rows (R / G / B) ─────────────────────────────────────────────
    if (selected_row < 3)
    {
        int *channel = (selected_row == 0) ? &r
                     : (selected_row == 1) ? &g
                                           : &b;

        if (PAD_justRepeated(BTN_LEFT))
        {
            *channel = std::max(0, *channel - 1);
            applyColor();
            dirty = 1;
            return NoOp;
        }
        else if (PAD_justRepeated(BTN_RIGHT))
        {
            *channel = std::min(255, *channel + 1);
            applyColor();
            dirty = 1;
            return NoOp;
        }
        else if (PAD_justRepeated(BTN_L1))
        {
            *channel = std::max(0, *channel - 16);
            applyColor();
            dirty = 1;
            return NoOp;
        }
        else if (PAD_justRepeated(BTN_R1))
        {
            *channel = std::min(255, *channel + 16);
            applyColor();
            dirty = 1;
            return NoOp;
        }
    }
    // ── Swatch row ──────────────────────────────────────────────────────────
    else if (num_swatches > 0)
    {
        if (PAD_justRepeated(BTN_LEFT) && selected_swatch > 0)
        {
            selected_swatch--;
            dirty = 1;
            return NoOp;
        }
        else if (PAD_justRepeated(BTN_RIGHT) && selected_swatch < num_swatches - 1)
        {
            selected_swatch++;
            dirty = 1;
            return NoOp;
        }
        else if (PAD_justPressed(BTN_A))
        {
            // Load the highlighted swatch colour into the sliders and apply it.
            unpackColor(swatches[selected_swatch].second, r, g, b);
            applyColor();
            dirty = 1;
            return NoOp;
        }
    }

    // ── Global ──────────────────────────────────────────────────────────────
    if (PAD_justPressed(BTN_B))
    {
        quit = 1;
        return NoOp;
    }

    return Unhandled;
}

// ── drawing ───────────────────────────────────────────────────────────────────

void ColorPickerMenu::drawColorSquare(SDL_Surface *surface, const SDL_Rect &rect) const
{
    // White border
    SDL_FillRect(surface, const_cast<SDL_Rect *>(&rect), RGB_WHITE);

    // Colour fill (inset by 2 px)
    SDL_Rect inner = {rect.x + SCALE1(1), rect.y + SCALE1(1),
                      rect.w - SCALE1(2), rect.h - SCALE1(2)};
    uint32_t col = SDL_MapRGB(surface->format,
                              (currentColor() >> 16) & 0xff,
                              (currentColor() >> 8) & 0xff,
                              currentColor() & 0xff);
    SDL_FillRect(surface, &inner, col);

    // "#RRGGBB" label centred below the square
    char hex[10];
    snprintf(hex, sizeof(hex), "#%06X", currentColor());
    SDL_Surface *text = TTF_RenderUTF8_Blended(font.small, hex, COLOR_WHITE);
    if (text)
    {
        int tx = rect.x + (rect.w - text->w) / 2;
        int ty = rect.y + rect.h + SCALE1(PADDING / 2);
        SDL_BlitSurfaceCPP(text, {}, surface, {tx, ty});
        SDL_FreeSurface(text);
    }
}

void ColorPickerMenu::drawSlider(SDL_Surface *surface, const SDL_Rect &rect,
                                  const char *label, SDL_Color channel_color,
                                  int value, bool selected) const
{
    // Selection highlight pill
    if (selected)
        GFX_blitPillLightCPP(ASSET_BUTTON, surface, rect);

    const int gap     = SCALE1(OPTION_PADDING / 2);   // ~4 at scale 2
    const int label_w = SCALE1(14);                    // room for "R"/"G"/"B"
    const int value_w = SCALE1(22);                    // room for "FF"

    // ── Label ────────────────────────────────────────────────────────────────
    SDL_Color label_col = selected ? uintToColour(THEME_COLOR5_255) : channel_color;
    SDL_Surface *label_surf = TTF_RenderUTF8_Blended(font.small, label, label_col);
    if (label_surf)
    {
        int ly = rect.y + (rect.h - label_surf->h) / 2;
        SDL_BlitSurfaceCPP(label_surf, {}, surface, {rect.x + gap, ly});
        SDL_FreeSurface(label_surf);
    }

    // ── Slider bar ───────────────────────────────────────────────────────────
    int bar_x = rect.x + label_w + gap * 2;
    int bar_w = rect.w - label_w - value_w - gap * 4;
    int bar_y = rect.y + rect.h / 4;
    int bar_h = rect.h / 2;

    // Background
    SDL_Rect bg = {bar_x, bar_y, bar_w, bar_h};
    SDL_FillRect(surface, &bg, SDL_MapRGB(surface->format, 0x33, 0x33, 0x33));

    // Coloured fill
    int fill_w = (bar_w * value) / 255;
    if (fill_w > 0)
    {
        SDL_Rect fill = {bar_x, bar_y, fill_w, bar_h};
        SDL_FillRect(surface, &fill,
                     SDL_MapRGB(surface->format,
                                channel_color.r, channel_color.g, channel_color.b));
    }

    // Indicator line at the fill edge (shows current position clearly)
    if (fill_w > 0 && fill_w < bar_w)
    {
        int ix = bar_x + fill_w;
        SDL_Rect indicator = {ix - SCALE1(1), bar_y - SCALE1(1),
                              SCALE1(2), bar_h + SCALE1(2)};
        SDL_FillRect(surface, &indicator, RGB_WHITE);
    }

    // ── Hex value ────────────────────────────────────────────────────────────
    char hex[4];
    snprintf(hex, sizeof(hex), "%02X", value);
    SDL_Color val_col = selected ? uintToColour(THEME_COLOR5_255) : COLOR_WHITE;
    SDL_Surface *val_surf = TTF_RenderUTF8_Blended(font.small, hex, val_col);
    if (val_surf)
    {
        int vx = rect.x + rect.w - value_w - gap + (value_w - val_surf->w) / 2;
        int vy = rect.y + (rect.h - val_surf->h) / 2;
        SDL_BlitSurfaceCPP(val_surf, {}, surface, {vx, vy});
        SDL_FreeSurface(val_surf);
    }
}

void ColorPickerMenu::drawSwatches(SDL_Surface *surface, const SDL_Rect &rect,
                                    bool row_selected) const
{
    const int swatch_size = SCALE1(BUTTON_SIZE);   // the coloured square
    const int slot_w      = SCALE1(BUTTON_SIZE + PADDING);  // square + gap
    const int label_gap   = SCALE1(2);

    int x = rect.x;

    for (int i = 0; i < (int)swatches.size(); i++)
    {
        bool is_selected = row_selected && (i == selected_swatch);

        SDL_Rect sw = {x, rect.y, swatch_size, swatch_size};

        // Border (white if selected, dark-gray otherwise)
        uint32_t border_col = is_selected
                                ? RGB_WHITE
                                : SDL_MapRGB(surface->format, 0x40, 0x40, 0x40);
        SDL_FillRect(surface, &sw, border_col);

        // Colour fill (inset by 1–2 px)
        int inset = is_selected ? SCALE1(2) : SCALE1(1);
        SDL_Rect inner = {sw.x + inset, sw.y + inset,
                          sw.w - inset * 2, sw.h - inset * 2};
        uint32_t col = SDL_MapRGB(surface->format,
                                  (swatches[i].second >> 16) & 0xff,
                                  (swatches[i].second >> 8) & 0xff,
                                  swatches[i].second & 0xff);
        SDL_FillRect(surface, &inner, col);

        // Short label below the swatch
        SDL_Color text_col = is_selected
                               ? uintToColour(THEME_COLOR2_255)
                               : uintToColour(THEME_COLOR4_255);
        SDL_Surface *label_surf = TTF_RenderUTF8_Blended(
            font.tiny, swatches[i].first.c_str(), text_col);
        if (label_surf)
        {
            int lx = x + (swatch_size - label_surf->w) / 2;
            int ly = rect.y + swatch_size + label_gap;
            SDL_BlitSurfaceCPP(label_surf, {}, surface, {lx, ly});
            SDL_FreeSurface(label_surf);
        }

        x += slot_w;
    }
}

void ColorPickerMenu::drawCustom(SDL_Surface *surface, const SDL_Rect &dst)
{
    // ── Unscaled layout constants ─────────────────────────────────────────────
    const int SQUARE_SIZE  = 70;  // colour-preview square (unscaled)
    const int SLIDER_H     = BUTTON_SIZE;  // 20
    const int SLIDER_GAP   = PADDING / 2;  // 5 between sliders
    const int SECTION_GAP  = PADDING * 2;  // 20 between major sections

    // ── Top section: colour square (left) + RGB sliders (right) ──────────────
    SDL_Rect square_rect = {
        dst.x + SCALE1(PADDING),
        dst.y + SCALE1(PADDING),
        SCALE1(SQUARE_SIZE),
        SCALE1(SQUARE_SIZE)
    };
    drawColorSquare(surface, square_rect);

    // Slider area: to the right of the square
    int slider_x = dst.x + SCALE1(PADDING + SQUARE_SIZE + PADDING);
    int slider_w = dst.w - SCALE1(PADDING + SQUARE_SIZE + PADDING * 2);

    // Vertically centre the three sliders within the square height
    int total_slider_h = 3 * SCALE1(SLIDER_H) + 2 * SCALE1(SLIDER_GAP);
    int slider_start_y = dst.y + SCALE1(PADDING) +
                         (SCALE1(SQUARE_SIZE) - total_slider_h) / 2;

    static const char *labels[] = {"R", "G", "B"};
    static const SDL_Color channel_colors[] = {
        {220, 60,  60,  255},   // red tint for R
        {60,  200, 60,  255},   // green tint for G
        {60,  100, 220, 255},   // blue tint for B
    };
    const int values[] = {r, g, b};

    for (int i = 0; i < 3; i++)
    {
        int sy = slider_start_y + i * (SCALE1(SLIDER_H) + SCALE1(SLIDER_GAP));
        SDL_Rect sr = {slider_x, sy, slider_w, SCALE1(SLIDER_H)};
        drawSlider(surface, sr, labels[i], channel_colors[i], values[i],
                   selected_row == i);
    }

    // ── Swatch section ────────────────────────────────────────────────────────
    if (!swatches.empty())
    {
        int section_y = dst.y + SCALE1(PADDING + SQUARE_SIZE + SECTION_GAP);

        // "Currently in use:" header text
        SDL_Surface *hdr = TTF_RenderUTF8_Blended(
            font.tiny, "Currently in use:", uintToColour(THEME_COLOR4_255));
        if (hdr)
        {
            SDL_BlitSurfaceCPP(hdr, {}, surface,
                               {dst.x + SCALE1(PADDING), section_y});
            section_y += hdr->h + SCALE1(PADDING / 2);
            SDL_FreeSurface(hdr);
        }

        // If the swatch row is active, show the highlighted swatch name
        if (selected_row == 3 && selected_swatch < (int)swatches.size())
        {
            SDL_Surface *name_surf = TTF_RenderUTF8_Blended(
                font.tiny,
                swatches[selected_swatch].first.c_str(),
                uintToColour(THEME_COLOR2_255));
            if (name_surf)
            {
                // Place it at the right of the header line
                int nx = dst.x + SCALE1(PADDING) + SCALE1(SQUARE_SIZE + PADDING);
                SDL_BlitSurfaceCPP(name_surf, {}, surface,
                                   {nx, section_y - name_surf->h - SCALE1(PADDING / 2)});
                SDL_FreeSurface(name_surf);
            }
        }

        SDL_Rect swatches_rect = {
            dst.x + SCALE1(PADDING),
            section_y,
            dst.w - SCALE1(PADDING * 2),
            SCALE1(BUTTON_SIZE + PADDING + FONT_TINY)
        };
        drawSwatches(surface, swatches_rect, selected_row == 3);
    }

    // ── Button hints ──────────────────────────────────────────────────────────
    if (selected_row == 3 && !swatches.empty())
    {
        char *hints_l[] = {(char *)"B", (char *)"BACK",
                           (char *)"A", (char *)"LOAD", nullptr};
        GFX_blitButtonGroup(hints_l, 0, surface, 0);
        char *hints_r[] = {(char *)"L/R", (char *)"NAVIGATE", nullptr};
        GFX_blitButtonGroup(hints_r, 1, surface, 1);
    }
    else
    {
        char *hints_l[] = {(char *)"B", (char *)"BACK", nullptr};
        GFX_blitButtonGroup(hints_l, 0, surface, 0);
        char *hints_r[] = {(char *)"L/R", (char *)"ADJUST",
                           (char *)"L1/R1", (char *)"+/-16", nullptr};
        GFX_blitButtonGroup(hints_r, 1, surface, 1);
    }
}
