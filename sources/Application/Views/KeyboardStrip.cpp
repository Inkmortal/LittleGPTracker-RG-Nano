#include "KeyboardStrip.h"
#include "Application/AppWindow.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif

const int KeyboardWhiteIndex[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
const bool KeyboardIsBlack[12] = {false, true,  false, true,  false, false,
                                  true,  false, true,  false, true,  false};

void DrawKeyboardStrip(SDLGUIWindowImp *imp, const KeyboardStrip &k, int key,
                       int mask, int cursor) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    GUIColor background = AppWindow::ThemeColor(CD_BACKGROUND);
    GUIColor whiteOff = AppWindow::ThemeBlend(CD_BACKGROUND, CD_NORMAL, 40);
    GUIColor blackOff = AppWindow::ThemeBlend(CD_BACKGROUND, CD_NORMAL, 10);
    GUIColor whiteOn = AppWindow::ThemeColor(CD_HILITE2);
    GUIColor blackOn = AppWindow::ThemeBlend(CD_BACKGROUND, CD_HILITE2, 70);
    GUIColor root = AppWindow::ThemeColor(CD_CURSOR);
    GUIColor cursorColor = AppWindow::ThemeColor(CD_CURSOR);

    imp->SetColor(background);
    GUIRect area(k.x - 2, k.top - 2, k.x + 7 * k.whiteW + 2, k.top + k.height + 6);
    imp->DrawRect(area);

    for (int pass = 0; pass < 2; pass++) {
        for (int n = 0; n < 12; n++) {
            if (KeyboardIsBlack[n] != (pass == 1))
                continue; // white keys first
            bool in = (key >= 0) && ((mask >> ((n - key + 12) % 12)) & 1);
            bool isRoot = (key == n);
            GUIColor fill = whiteOff;
            if (isRoot)
                fill = root;
            else if (in)
                fill = KeyboardIsBlack[n] ? blackOn : whiteOn;
            else
                fill = KeyboardIsBlack[n] ? blackOff : whiteOff;
            int x0, x1, y1;
            if (KeyboardIsBlack[n]) {
                int centre = k.x + k.whiteW * (KeyboardWhiteIndex[n] + 1);
                x0 = centre - k.blackW / 2;
                x1 = centre + k.blackW / 2;
                y1 = k.top + k.blackH;
                // A dark edge so a black key stands out on lit white keys
                imp->SetColor(background);
                GUIRect edge(x0 - 1, k.top, x1 + 1, y1 + 1);
                imp->DrawRect(edge);
            } else {
                x0 = k.x + k.whiteW * KeyboardWhiteIndex[n] + 1;
                x1 = k.x + k.whiteW * (KeyboardWhiteIndex[n] + 1) - 1;
                y1 = k.top + k.height;
            }
            imp->SetColor(fill);
            GUIRect body(x0, k.top, x1, y1);
            imp->DrawRect(body);
            if (n == cursor) {
                // Cursor: a bar under the key (under a black key: on it)
                imp->SetColor(cursorColor);
                int barTop = KeyboardIsBlack[n] ? y1 + 2 : k.top + k.height + 2;
                GUIRect bar(x0 + 2, barTop, x1 - 2, barTop + 3);
                imp->DrawRect(bar);
            }
        }
    }
#endif
}
