bool UI::Create(HWND parent) {
  if (pimpl_) {
    pimpl_->panel.Create(parent);
    return true;
  }

  pimpl_ = new UIImpl(*this);
  if (!pimpl_)
    return false;

  pimpl_->panel.Create(parent);
  return true;
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