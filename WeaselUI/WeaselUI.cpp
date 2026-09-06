#include "stdafx.h"
#include <WeaselUI.h>
#include "BangkePanel.h"

using namespace weasel;

class weasel::UIImpl {
 public:
  BangkePanel panel;

  UIImpl(weasel::UI& ui) : panel(ui), shown(false) {}
  ~UIImpl() {}
  void Refresh() {
    if (!panel.IsWindow())
      return;
    if (timer) {
      Hide();
      KillTimer(panel.hwnd(), AUTOHIDE_TIMER);
      timer = 0;
    }
    panel.Refresh();
  }
  void Show();
  void Hide();
  void ShowWithTimeout(size_t millisec);
  bool IsShown() const { return shown; }

  static VOID CALLBACK OnTimer(_In_ HWND hwnd,
                               _In_ UINT uMsg,
                               _In_ UINT_PTR idEvent,
                               _In_ DWORD dwTime);
  static const int AUTOHIDE_TIMER = 20121220;
  static UINT_PTR timer;
  bool shown;
};

UINT_PTR UIImpl::timer = 0;

void UIImpl::Show() {
  if (!panel.IsWindow())
    return;
  panel.Show(SW_SHOWNA);
  shown = true;
  if (timer) {
    KillTimer(panel.hwnd(), AUTOHIDE_TIMER);
    timer = 0;
  }
}

void UIImpl::Hide() {
  if (!panel.IsWindow())
    return;
  panel.Show(SW_HIDE);
  shown = false;
  if (timer) {
    KillTimer(panel.hwnd(), AUTOHIDE_TIMER);
    timer = 0;
  }
}

void UIImpl::ShowWithTimeout(size_t millisec) {
  if (!panel.IsWindow())
    return;
  DLOG(INFO) << "ShowWithTimeout: " << millisec;
  panel.Show(SW_SHOWNA);
  shown = true;
  SetTimer(panel.hwnd(), AUTOHIDE_TIMER, static_cast<UINT>(millisec),
           &UIImpl::OnTimer);
  timer = UINT_PTR(this);
}
VOID CALLBACK UIImpl::OnTimer(_In_ HWND hwnd,
                              _In_ UINT uMsg,
                              _In_ UINT_PTR idEvent,
                              _In_ DWORD dwTime) {
  DLOG(INFO) << "OnTimer:";
  KillTimer(hwnd, idEvent);
  UIImpl* self = (UIImpl*)timer;
  timer = 0;
  if (self) {
    self->Hide();
    self->shown = false;
  }
}

bool UI::Create(HWND parent) {
  if (pimpl_) {
    pimpl_->panel.Create(parent);
    return true;
  }

  pimpl_ = new UIImpl(*this);
  if (!pimpl_)
    return false;

  return pimpl_->panel.Create(parent);
}

void UI::Destroy(bool full) {
  if (pimpl_) {
    if (pimpl_->panel.IsWindow()) {
      pimpl_->panel.Destroy();
    }
    if (full) {
      delete pimpl_;
      pimpl_ = 0;
      pDWR.reset();
    }
  }
}

bool UI::GetIsReposition() {
  if (pimpl_)
    return pimpl_->panel.GetIsReposition();
  else
    return false;
}

void UI::Show() {
  if (pimpl_) {
    pimpl_->Show();
  }
}

void UI::Hide() {
  if (pimpl_) {
    pimpl_->Hide();
  }
}

void UI::ShowWithTimeout(size_t millisec) {
  if (pimpl_) {
    pimpl_->ShowWithTimeout(millisec);
  }
}

bool UI::IsCountingDown() const {
  return pimpl_ && pimpl_->timer != 0;
}

bool UI::IsShown() const {
  return pimpl_ && pimpl_->IsShown();
}

void UI::SetAsyncRefresh(std::function<void()> cb) {
  if (pimpl_)
    pimpl_->panel.on_async_refresh = std::move(cb);
}

void UI::Refresh() {
  if (pimpl_) {
    pimpl_->Refresh();
  }
}

void UI::UpdateInputPosition(RECT const& rc) {
  if (pimpl_ && pimpl_->panel.IsWindow()) {
    pimpl_->panel.MoveTo(rc);
  }
}

void UI::Update(const Context& ctx, const Status& status) {
  if (ctx_ == ctx && status_ == status)
    return;
  ctx_ = ctx;
  status_ = status;
  if (style_.candidate_abbreviate_length > 0) {
    for (auto& c : ctx_.cinfo.candies) {
      if (c.str.length() > (size_t)style_.candidate_abbreviate_length) {
        c.str =
            c.str.substr(0, (size_t)style_.candidate_abbreviate_length - 1) +
            L"..." + c.str.substr(c.str.length() - 1);
      }
    }
  }
  Refresh();
}
