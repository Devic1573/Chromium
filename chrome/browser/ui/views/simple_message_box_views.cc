// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/simple_message_box.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(USE_AURA)
#include "ui/aura/client/dispatcher_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#endif

namespace browser {

namespace {

class SimpleMessageBoxViews : public views::DialogDelegate,
                              public MessageLoop::Dispatcher {
 public:
  SimpleMessageBoxViews(const string16& title,
                        const string16& message,
                        MessageBoxType type);

  MessageBoxResult result() const { return result_; }

 private:
  virtual ~SimpleMessageBoxViews();

  // Overridden from views::DialogDelegate:
  virtual int GetDialogButtons() const OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // Overridden from MessageLoop::Dispatcher:
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

  const string16 window_title_;
  const MessageBoxType type_;
  MessageBoxResult result_;
  views::MessageBoxView* message_box_view_;

  // Set to false as soon as the user clicks a dialog button; this tells the
  // dispatcher we're done.
  bool should_show_dialog_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMessageBoxViews);
};

////////////////////////////////////////////////////////////////////////////////
// SimpleMessageBoxViews, public:

SimpleMessageBoxViews::SimpleMessageBoxViews(const string16& title,
                                             const string16& message,
                                             MessageBoxType type)
    : window_title_(title),
      type_(type),
      result_(MESSAGE_BOX_RESULT_NO),
      message_box_view_(new views::MessageBoxView(
          views::MessageBoxView::InitParams(message))),
      should_show_dialog_(true) {
}

////////////////////////////////////////////////////////////////////////////////
// SimpleMessageBoxViews, private:

SimpleMessageBoxViews::~SimpleMessageBoxViews() {
}

int SimpleMessageBoxViews::GetDialogButtons() const {
  if (type_ == MESSAGE_BOX_TYPE_QUESTION)
    return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
  return ui::DIALOG_BUTTON_OK;
}

string16 SimpleMessageBoxViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (type_ == MESSAGE_BOX_TYPE_QUESTION) {
    return l10n_util::GetStringUTF16((button == ui::DIALOG_BUTTON_OK) ?
        IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL :
        IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL);
  }
  return l10n_util::GetStringUTF16(IDS_OK);
}

bool SimpleMessageBoxViews::Cancel() {
  should_show_dialog_= false;
  result_ = MESSAGE_BOX_RESULT_NO;
  return true;
}

bool SimpleMessageBoxViews::Accept() {
  should_show_dialog_ = false;
  result_ = MESSAGE_BOX_RESULT_YES;
  return true;
}

string16 SimpleMessageBoxViews::GetWindowTitle() const {
  return window_title_;
}

void SimpleMessageBoxViews::DeleteDelegate() {
  delete this;
}

ui::ModalType SimpleMessageBoxViews::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

views::View* SimpleMessageBoxViews::GetContentsView() {
  return message_box_view_;
}

views::Widget* SimpleMessageBoxViews::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* SimpleMessageBoxViews::GetWidget() const {
  return message_box_view_->GetWidget();
}

bool SimpleMessageBoxViews::Dispatch(const base::NativeEvent& event) {
#if defined(OS_WIN)
  TranslateMessage(&event);
  DispatchMessage(&event);
#elif defined(USE_AURA)
  aura::Env::GetInstance()->GetDispatcher()->Dispatch(event);
#endif
  return should_show_dialog_;
}

}  // namespace

MessageBoxResult ShowMessageBox(gfx::NativeWindow parent,
                                const string16& title,
                                const string16& message,
                                MessageBoxType type) {
  SimpleMessageBoxViews* dialog =
      new SimpleMessageBoxViews(title, message, type);

  views::Widget::CreateWindowWithParent(dialog, parent)->Show();

#if defined(USE_AURA)
  aura::client::GetDispatcherClient(parent->GetRootWindow())->RunWithDispatcher(
      dialog, parent, true);
#else
  {
    MessageLoop::ScopedNestableTaskAllower allow(MessageLoopForUI::current());
    MessageLoopForUI::current()->RunWithDispatcher(dialog);
  }
#endif

  return dialog->result();
}

}  // namespace browser
