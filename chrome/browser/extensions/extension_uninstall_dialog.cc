// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_uninstall_dialog.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

// Size of extension icon in top left of dialog.
static const int kIconSize = 69;

ExtensionUninstallDialog::ExtensionUninstallDialog(
    Profile* profile,
    ExtensionUninstallDialog::Delegate* delegate)
    : profile_(profile),
      delegate_(delegate),
      extension_(NULL),
      ui_loop_(MessageLoop::current()),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)) {}

ExtensionUninstallDialog::~ExtensionUninstallDialog() {}

void ExtensionUninstallDialog::ConfirmUninstall(
    const extensions::Extension* extension) {
  DCHECK(ui_loop_ == MessageLoop::current());
  extension_ = extension;

  ExtensionResource image =
      extension_->GetIconResource(ExtensionIconSet::EXTENSION_ICON_LARGE,
                                  ExtensionIconSet::MATCH_BIGGER);
  // Load the image asynchronously. The response will be sent to OnImageLoaded.
  tracker_.LoadImage(extension_, image,
                     gfx::Size(kIconSize, kIconSize),
                     ImageLoadingTracker::DONT_CACHE);
}

void ExtensionUninstallDialog::SetIcon(const gfx::Image& image) {
  if (image.IsEmpty()) {
    if (extension_->is_app()) {
      icon_ = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
          IDR_APP_DEFAULT_ICON);
    } else {
      icon_ = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
          IDR_EXTENSION_DEFAULT_ICON);
    }
  } else {
    icon_ = *image.ToSkBitmap();
  }
}

void ExtensionUninstallDialog::OnImageLoaded(const gfx::Image& image,
                                             const std::string& extension_id,
                                             int index) {
  SetIcon(image);

  Show();
}
