#ifndef CHROME_BROWSER_UI_WINDOW_TABS_H_
#define CHROME_BROWSER_UI_WINDOW_TABS_H_

#include "base/win/windows_types.h"

#include <string>

#include "chrome/browser/ui/views/frame/browser_view.h"

namespace gfx {
class ImageSkia;
}

namespace WindowTabs {

void Enable();

void Disable();

void Observe(BrowserView* browser_view,
             bool update = false,
             HWND window = nullptr);

gfx::ImageSkia GetIcon(HWND window);

std::u16string GetTitle(HWND window);

}  // namespace WindowTabs

#endif  // CHROME_BROWSER_UI_WINDOW_TABS_H_
