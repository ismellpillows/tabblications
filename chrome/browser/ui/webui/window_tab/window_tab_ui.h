#ifndef CHROME_BROWSER_UI_WEBUI_WINDOW_TAB_WINDOW_TAB_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WINDOW_TAB_WINDOW_TAB_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The WebUI for chrome://window-tab
class WindowTabUI : public content::WebUIController {
 public:
  explicit WindowTabUI(content::WebUI* web_ui);
  ~WindowTabUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WINDOW_TAB_WINDOW_TAB_UI_H_
