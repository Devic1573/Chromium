// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_button.h"

#include <algorithm>

#include "ash/launcher/launcher_button_host.h"
#include "grit/ui_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/controls/image_view.h"

namespace {

// Size of the bar. This is along the opposite axis of the shelf. For example,
// if the shelf is aligned horizontally then this is the height of the bar.
const int kBarSize = 3;
const int kBarSpacing = 5;
const int kIconSize = 32;
const int kHopSpacing = 2;
const int kActiveBarColor = 0xe6ffffff;
const int kInactiveBarColor = 0x80ffffff;
const int kHopUpMS = 200;
const int kHopDownMS = 200;
const int kAttentionThrobDurationMS = 2000;

bool ShouldHop(int state) {
  return state & ash::internal::LauncherButton::STATE_HOVERED ||
      state & ash::internal::LauncherButton::STATE_ACTIVE;
}

}  // namespace

namespace ash {

namespace internal {

class LauncherButton::BarView : public views::ImageView,
                                public ui::AnimationDelegate {
 public:
  BarView() : ALLOW_THIS_IN_INITIALIZER_LIST(animation_(this)) {
    animation_.SetThrobDuration(kAttentionThrobDurationMS);
    animation_.SetTweenType(ui::Tween::SMOOTH_IN_OUT);
  }

  // View overrides.
  bool HitTest(const gfx::Point& l) const OVERRIDE {
    // Allow Mouse...() messages to go to the parent view.
    return false;
  }

  void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    if (animation_.is_animating()) {
      int alpha = animation_.CurrentValueBetween(0, 255);
      canvas->SaveLayerAlpha(alpha);
      views::ImageView::OnPaint(canvas);
      canvas->Restore();
    } else {
      views::ImageView::OnPaint(canvas);
    }
  }

  // ui::AnimationDelegate overrides.
  void AnimationProgressed(const ui::Animation* animation) OVERRIDE {
    SchedulePaint();
  }

  void ShowAttention(bool show) {
    if (show) {
      // It's less disruptive if we don't start the pulsing at 0.
      animation_.Reset(0.375);
      animation_.StartThrobbing(-1);
    } else {
      animation_.Reset(0.0);
    }
  }

 private:
  ui::ThrobAnimation animation_;

  DISALLOW_COPY_AND_ASSIGN(BarView);
};

LauncherButton::IconView::IconView() : icon_size_(kIconSize) {
}

LauncherButton::IconView::~IconView() {
}

bool LauncherButton::IconView::HitTest(const gfx::Point& l) const {
  // Return false so that LauncherButton gets all the mouse events.
  return false;
}

LauncherButton* LauncherButton::Create(views::ButtonListener* listener,
                                       LauncherButtonHost* host) {
  LauncherButton* button = new LauncherButton(listener, host);
  button->Init();
  return button;
}

LauncherButton::LauncherButton(views::ButtonListener* listener,
                               LauncherButtonHost* host)
    : CustomButton(listener),
      host_(host),
      icon_view_(NULL),
      bar_(new BarView),
      state_(STATE_NORMAL) {
  set_accessibility_focusable(true);
  AddChildView(bar_);
}

LauncherButton::~LauncherButton() {
}

void LauncherButton::SetShadowedImage(const SkBitmap& bitmap) {
  const gfx::ShadowValue kShadows[] = {
    gfx::ShadowValue(gfx::Point(0, 2), 0, SkColorSetARGB(0x1A, 0, 0, 0)),
    gfx::ShadowValue(gfx::Point(0, 3), 1, SkColorSetARGB(0x1A, 0, 0, 0)),
    gfx::ShadowValue(gfx::Point(0, 0), 1, SkColorSetARGB(0x54, 0, 0, 0)),
  };

  SkBitmap shadowed_bitmap = SkBitmapOperations::CreateDropShadow(
      bitmap, gfx::ShadowValues(kShadows, kShadows + arraysize(kShadows)));
  icon_view_->SetImage(shadowed_bitmap);
}

void LauncherButton::SetImage(const SkBitmap& image) {
  if (image.empty()) {
    // TODO: need an empty image.
    icon_view_->SetImage(image);
    return;
  }

  if (icon_view_->icon_size() == 0) {
    SetShadowedImage(image);
    return;
  }

  // Resize the image maintaining our aspect ratio.
  int pref = icon_view_->icon_size();
  float aspect_ratio =
      static_cast<float>(image.width()) / static_cast<float>(image.height());
  int height = pref;
  int width = static_cast<int>(aspect_ratio * height);
  if (width > pref) {
    width = pref;
    height = static_cast<int>(width / aspect_ratio);
  }

  if (width == image.width() && height == image.height()) {
    SetShadowedImage(image);
    return;
  }

  SkBitmap resized_image = skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_BEST, width, height);
  SetShadowedImage(resized_image);
}

void LauncherButton::AddState(State state) {
  if (!(state_ & state)) {
    if (ShouldHop(state) || !ShouldHop(state_)) {
      ui::ScopedLayerAnimationSettings scoped_setter(
          icon_view_->layer()->GetAnimator());
      scoped_setter.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kHopUpMS));
      state_ |= state;
      UpdateState();
    } else {
      state_ |= state;
      UpdateState();
    }
    if (state & STATE_ATTENTION)
      bar_->ShowAttention(true);
  }
}

void LauncherButton::ClearState(State state) {
  if (state_ & state) {
    if (!ShouldHop(state) || ShouldHop(state_)) {
      ui::ScopedLayerAnimationSettings scoped_setter(
          icon_view_->layer()->GetAnimator());
      scoped_setter.SetTweenType(ui::Tween::LINEAR);
      scoped_setter.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kHopDownMS));
      state_ &= ~state;
      UpdateState();
    } else {
      state_ &= ~state;
      UpdateState();
    }
    if (state & STATE_ATTENTION)
      bar_->ShowAttention(false);
  }
}

gfx::Rect LauncherButton::GetIconBounds() const {
  return icon_view_->bounds();
}

bool LauncherButton::OnMousePressed(const views::MouseEvent& event) {
  CustomButton::OnMousePressed(event);
  host_->MousePressedOnButton(this, event);
  return true;
}

void LauncherButton::OnMouseReleased(const views::MouseEvent& event) {
  CustomButton::OnMouseReleased(event);
  host_->MouseReleasedOnButton(this, false);
}

void LauncherButton::OnMouseCaptureLost() {
  ClearState(STATE_HOVERED);
  host_->MouseReleasedOnButton(this, true);
  CustomButton::OnMouseCaptureLost();
}

bool LauncherButton::OnMouseDragged(const views::MouseEvent& event) {
  CustomButton::OnMouseDragged(event);
  host_->MouseDraggedOnButton(this, event);
  return true;
}

void LauncherButton::OnMouseEntered(const views::MouseEvent& event) {
  AddState(STATE_HOVERED);
  CustomButton::OnMouseEntered(event);
  host_->MouseExitedButton(this);
}

void LauncherButton::OnMouseExited(const views::MouseEvent& event) {
  ClearState(STATE_HOVERED);
  CustomButton::OnMouseExited(event);
  host_->MouseExitedButton(this);
}

void LauncherButton::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = host_->GetAccessibleName(this);
}

void LauncherButton::Layout() {
  gfx::Rect rect(GetContentsBounds());
  int image_x, image_y;

  if (IsShelfHorizontal()) {
    image_x = rect.x() + (rect.width() - icon_view_->width()) / 2;
    image_y = rect.bottom() - (icon_view_->height() + kBarSize + kBarSpacing);
    if (ShouldHop(state_))
      image_y -= kHopSpacing;
  } else {
    image_y = rect.y() + (rect.height() - icon_view_->height()) / 2;
    if (host_->GetShelfAlignment() == SHELF_ALIGNMENT_LEFT) {
      image_x = rect.x() + kBarSize + kBarSpacing;
      if (ShouldHop(state_))
        image_x += kHopSpacing;
    } else {
      image_x = rect.right() - (icon_view_->width() + kBarSize + kBarSpacing);
      if (ShouldHop(state_))
        image_x -= kHopSpacing;
    }
  }

  icon_view_->SetPosition(gfx::Point(image_x, image_y));
  bar_->SetBoundsRect(rect);
}

bool LauncherButton::GetTooltipText(
    const gfx::Point& p, string16* tooltip) const {
  DCHECK(tooltip);
  tooltip->assign(host_->GetAccessibleName(this));
  return true;
}

void LauncherButton::Init() {
  icon_view_ = CreateIconView();
  // TODO: refactor the layers so each button doesn't require 2.
  icon_view_->SetPaintToLayer(true);
  icon_view_->SetFillsBoundsOpaquely(false);
  icon_view_->SetSize(gfx::Size(kIconSize, kIconSize));
  icon_view_->SetHorizontalAlignment(views::ImageView::CENTER);
  icon_view_->SetVerticalAlignment(views::ImageView::CENTER);
  AddChildView(icon_view_);
}

LauncherButton::IconView* LauncherButton::CreateIconView() {
  return new IconView;
}

bool LauncherButton::IsShelfHorizontal() const {
  return host_->GetShelfAlignment() == SHELF_ALIGNMENT_BOTTOM;
}

void LauncherButton::UpdateState() {
  if (state_ == STATE_NORMAL) {
    bar_->SetVisible(false);
  } else {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    int bar_id;
    bar_->SetVisible(true);

    if (state_ & STATE_ACTIVE || state_ & STATE_ATTENTION) {
      bar_id = IsShelfHorizontal() ? IDR_AURA_LAUNCHER_UNDERLINE_ACTIVE :
          IDR_AURA_LAUNCHER_UNDERLINE_VERTICAL_ACTIVE;
    } else if (state_ & STATE_HOVERED) {
      bar_id = IsShelfHorizontal() ? IDR_AURA_LAUNCHER_UNDERLINE_HOVER :
          IDR_AURA_LAUNCHER_UNDERLINE_VERTICAL_HOVER;
    } else {
      bar_id = IsShelfHorizontal() ? IDR_AURA_LAUNCHER_UNDERLINE_RUNNING :
          IDR_AURA_LAUNCHER_UNDERLINE_VERTICAL_RUNNING;
    }

    bar_->SetImage(rb.GetImageNamed(bar_id).ToImageSkia());
  }

  switch (host_->GetShelfAlignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
      bar_->SetHorizontalAlignment(views::ImageView::CENTER);
      bar_->SetVerticalAlignment(views::ImageView::TRAILING);
      break;
    case SHELF_ALIGNMENT_LEFT:
      bar_->SetHorizontalAlignment(
          base::i18n::IsRTL() ? views::ImageView::TRAILING :
                                views::ImageView::LEADING);
      bar_->SetVerticalAlignment(views::ImageView::CENTER);
      break;
    case SHELF_ALIGNMENT_RIGHT:
      bar_->SetHorizontalAlignment(
          base::i18n::IsRTL() ? views::ImageView::LEADING :
                                views::ImageView::TRAILING);
      bar_->SetVerticalAlignment(views::ImageView::CENTER);
      break;
  }

  Layout();
  SchedulePaint();
}
}  // namespace internal
}  // namespace ash
