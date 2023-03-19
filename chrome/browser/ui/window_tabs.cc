#include "chrome/browser/ui/window_tabs.h"

#include <dwmapi.h>
#include <windows.h>

#include <string>

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/gfx/icon_util.h"

namespace WindowTabs {

namespace {

void CALLBACK
    HandleWinEvent(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);

class Observer : public views::ViewObserver,
                 public views::WidgetObserver,
                 public TabStripModelObserver {
 public:
  ~Observer() override {
    for (const auto& [browser_view, _] : browser_view_to_window_) {
      browser_view->contents_container()->RemoveObserver(this);
      browser_view->contents_container()->GetWidget()->RemoveObserver(this);
      browser_view->browser()->tab_strip_model()->RemoveObserver(this);
    }
  }

  void Observe(BrowserView* browser_view, bool update, HWND window) {
    views::View* const contents_container = browser_view->contents_container();

    if (!browser_view_to_window_.contains(browser_view)) {
      HWND browser_window =
          browser_view->GetNativeWindow()->GetHost()->GetAcceleratedWidget();
      SetWindowLongPtr(
          browser_window, GWL_EXSTYLE,
          GetWindowLongPtr(browser_window, GWL_EXSTYLE) | WS_EX_LAYERED);
      SetLayeredWindowAttributes(browser_window, RGB(31, 33, 32), NULL,
                                 LWA_COLORKEY);

      views::Widget* const widget = contents_container->GetWidget();
      TabStripModel* const model = browser_view->browser()->tab_strip_model();

      contents_container->AddObserver(this);
      widget->AddObserver(this);
      model->AddObserver(this);

      contents_container_to_browser_view_[contents_container] = browser_view;
      widget_to_browser_view_[widget] = browser_view;
      model_to_browser_view_[model] = browser_view;
    }

    if (update) {
      Update(browser_view, window);
    }
  }

 private:
  void Update(BrowserView* browser_view, HWND window) {
    const HWND last_window = browser_view_to_window_[browser_view];

    browser_view_to_window_[browser_view] = window;
    OnViewBoundsChanged(browser_view->contents_container());

    if (last_window && last_window != window) {
      ShowWindow(last_window, SW_FORCEMINIMIZE);
      browser_view->Activate();
    }
  }

  void OnViewBoundsChanged(views::View* contents_container) override {
    BrowserView* const browser_view =
        contents_container_to_browser_view_[contents_container];
    if (const HWND window = browser_view_to_window_[browser_view]) {
      if (IsIconic(window)) {
        ShowWindow(window, SW_SHOWNOACTIVATE);
      }

      if (!window_to_margin_.contains(window)) {
        RECT rect, frame;

        GetWindowRect(window, &rect);
        DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &frame,
                              sizeof(frame));

        window_to_margin_[window] = {
            frame.left - rect.left, frame.top - rect.top,
            rect.right - frame.right, rect.bottom - frame.bottom};
      }

      RECT& margin = window_to_margin_[window];

      gfx::Rect bounds(display::Screen::GetScreen()->DIPToScreenRectInWindow(
          nullptr, contents_container->GetBoundsInScreen()));

      SetWindowPos(
          window,
          browser_view->GetNativeWindow()->GetHost()->GetAcceleratedWidget(),
          bounds.x() - margin.left + 1, bounds.y() - margin.top,
          bounds.width() + margin.left + margin.right,
          bounds.height() + margin.top + margin.bottom, SWP_NOACTIVATE);
    }
  }

  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    BrowserView* const browser_view = widget_to_browser_view_[widget];

    if (browser_view_to_window_[browser_view]) {
      const gfx::Size& new_size = new_bounds.size();
      if (new_size == old_size_) {
        OnViewBoundsChanged(browser_view->contents_container());
      }

      old_size_ = new_size;
    }
  }

  void OnTabStripModelChanged(
      TabStripModel* model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (!selection.active_tab_changed() || model->empty()) {
      return;
    }

    BrowserView* const browser_view = model_to_browser_view_[model];
    const HWND window =
        model->GetWindowForTab(selection.new_model.active().value());
    Update(browser_view, window);
  }

  void OnWidgetDestroying(views::Widget* widget) override {
    BrowserView* const browser_view = widget_to_browser_view_[widget];

    browser_view_to_window_.erase(browser_view);
    contents_container_to_browser_view_.erase(
        browser_view->contents_container());
    widget_to_browser_view_.erase(widget);
    model_to_browser_view_.erase(browser_view->browser()->tab_strip_model());
  }

  std::unordered_map<BrowserView*, HWND> browser_view_to_window_;
  std::unordered_map<views::View*, BrowserView*>
      contents_container_to_browser_view_;
  std::unordered_map<views::Widget*, BrowserView*> widget_to_browser_view_;
  std::unordered_map<TabStripModel*, BrowserView*> model_to_browser_view_;

  std::unordered_map<HWND, RECT> window_to_margin_;

  gfx::Size old_size_;
};

HWINEVENTHOOK move_size_hook = NULL;
HWINEVENTHOOK location_change_hook = NULL;

BrowserRootView* last_root_view = nullptr;

Observer* observer = nullptr;

HWINEVENTHOOK RegisterHook(UINT eventMin, UINT eventMax) {
  return SetWinEventHook(eventMin, eventMax, nullptr, HandleWinEvent, 0, 0,
                         WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
}

void RegisterMoveSizeHook() {
  if (!move_size_hook) {
    move_size_hook =
        RegisterHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND);
  }
}
void RegisterLocationChangeHook() {
  if (!location_change_hook) {
    location_change_hook =
        RegisterHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE);
  }
}

void Unhook(HWINEVENTHOOK& hook) {
  if (hook && UnhookWinEvent(hook)) {
    hook = nullptr;
  }
}

void CALLBACK HandleWinEvent(HWINEVENTHOOK hook,
                             DWORD event,
                             HWND hwnd,
                             LONG idObject,
                             LONG idChild,
                             DWORD dwEventThread,
                             DWORD dwmsEventTime) {
  switch (event) {
    case EVENT_OBJECT_LOCATIONCHANGE: {
      gfx::Point location =
          display::Screen::GetScreen()->GetCursorScreenPoint();

      if (gfx::NativeWindow window =
              display::Screen::GetScreen()->GetLocalProcessWindowAtPoint(
                  location, std::set<gfx::NativeWindow>())) {
        if (BrowserView* const view =
                BrowserView::GetBrowserViewForNativeWindow(window)) {
          BrowserRootView* const root_view = view->frame()->root_view();

          if (last_root_view && last_root_view != root_view) {
            last_root_view->OnDragExited();
          }

          last_root_view =
              root_view->HandleWindowDragged(location) ? root_view : nullptr;
        }
      } else if (last_root_view) {
        last_root_view->OnDragExited();
        last_root_view = nullptr;
      }

      break;
    }

    case EVENT_SYSTEM_MOVESIZESTART: {
      RegisterLocationChangeHook();

      break;
    }

    case EVENT_SYSTEM_MOVESIZEEND: {
      if (last_root_view) {
        last_root_view->HandleWindowDropped(hwnd);

        last_root_view = nullptr;
      }

      Unhook(location_change_hook);

      break;
    }
  }
}

}  // namespace

void Enable() {
  RegisterMoveSizeHook();

  if (!observer) {
    observer = new Observer();
  }
}

void Disable() {
  Unhook(move_size_hook);
  Unhook(location_change_hook);

  delete observer;
}

void Observe(BrowserView* browser_view, bool update, HWND window) {
  observer->Observe(browser_view, update, window);
}

gfx::ImageSkia GetIcon(HWND window) {
  HICON icon;

  SendMessageTimeout(window, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG, 5,
                     reinterpret_cast<PDWORD_PTR>(&icon));
  if (!icon) {
    icon = reinterpret_cast<HICON>(GetClassLongPtr(window, GCLP_HICON));
  }
  if (!icon) {
    SendMessageTimeout(window, WM_GETICON, ICON_SMALL, 0, SMTO_ABORTIFHUNG, 5,
                       reinterpret_cast<PDWORD_PTR>(&icon));
  }
  if (!icon) {
    SendMessageTimeout(window, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG, 5,
                       reinterpret_cast<PDWORD_PTR>(&icon));
  }
  if (!icon) {
    icon = reinterpret_cast<HICON>(GetClassLongPtr(window, GCLP_HICONSM));
  }

  if (icon) {
    return gfx::ImageSkia::CreateFrom1xBitmap(
        IconUtil::CreateSkBitmapFromHICON(icon));
  }
  return gfx::ImageSkia();
}

std::u16string GetTitle(HWND window) {
  if (const int length = GetWindowTextLength(window)) {
    std::wstring title(length, '\0');
    GetWindowText(window, &title.front(), length + 1);

    return std::u16string(title.begin(), title.end());
  }

  return std::u16string();
}

}  // namespace WindowTabs
