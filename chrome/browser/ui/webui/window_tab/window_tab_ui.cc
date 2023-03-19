#include "chrome/browser/ui/webui/window_tab/window_tab_ui.h"

#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "chrome/grit/window_tab_resources.h"
#include "chrome/grit/window_tab_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

WindowTabUI::WindowTabUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://window-tab source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIWindowTabHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kWindowTabResources, kWindowTabResourcesSize),
      IDR_WINDOW_TAB_WINDOW_TAB_HTML);
}

WindowTabUI::~WindowTabUI() = default;
