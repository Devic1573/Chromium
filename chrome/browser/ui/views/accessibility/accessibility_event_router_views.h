// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_EVENT_ROUTER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_EVENT_ROUTER_VIEWS_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "chrome/browser/accessibility/accessibility_events.h"
#include "ui/base/accessibility/accessibility_types.h"

class Profile;

template <typename T> struct DefaultSingletonTraits;

namespace views {
class View;
}

// NOTE: This class is part of the Accessibility Extension API, which lets
// extensions receive accessibility events. It's distinct from code that
// implements platform accessibility APIs like MSAA or ATK.
//
// Singleton class that adds listeners to many views, then sends an
// accessibility notification whenever a relevant event occurs in an
// accessible view.
//
// Views are not accessible by default. When you register a root widget,
// that widget and all of its descendants will start sending accessibility
// event notifications. You can then override the default behavior for
// specific descendants using other methods.
//
// You can use Profile::PauseAccessibilityEvents to prevent a flurry
// of accessibility events when a window is being created or initialized.
class AccessibilityEventRouterViews {
 public:
  // Internal information about a particular view to override the
  // information we get directly from the view.
  struct ViewInfo {
    ViewInfo() : ignore(false) {}

    // If nonempty, will use this name instead of the view's label.
    std::string name;

    // If true, will ignore this widget and not send accessibility events.
    bool ignore;
  };

  // Get the single instance of this class.
  static AccessibilityEventRouterViews* GetInstance();

  // Handle an accessibility event generated by a view.
  void HandleAccessibilityEvent(
      views::View* view, ui::AccessibilityTypes::Event event_type);

  // Handle a menu item being focused (separate because a menu item is
  // not necessarily its own view).
  void HandleMenuItemFocused(const string16& menu_name,
                             const string16& menu_item_name,
                             int item_index,
                             int item_count,
                             bool has_submenu);

 private:
  friend struct DefaultSingletonTraits<AccessibilityEventRouterViews>;

  FRIEND_TEST_ALL_PREFIXES(AccessibilityEventRouterViewsTest,
                           TestFocusNotification);

  AccessibilityEventRouterViews();
  virtual ~AccessibilityEventRouterViews();

  // Checks the type of the view and calls one of the more specific
  // Send*Notification methods, below.
  void DispatchAccessibilityNotification(
      views::View* view,
      int type);

  // Each of these methods constructs an AccessibilityControlInfo object
  // and sends a notification of a specific accessibility event.
  static void SendButtonNotification(
      views::View* view,
      int type,
      Profile* profile);
  static void SendLinkNotification(
      views::View* view,
      int type,
      Profile* profile);
  static void SendMenuNotification(
      views::View* view,
      int type,
      Profile* profile);
  static void SendMenuItemNotification(
      views::View* view,
      int type,
      Profile* profile);
  static void SendTextfieldNotification(
      views::View* view,
      int type,
      Profile* profile);
  static void SendComboboxNotification(
      views::View* view,
      int type,
      Profile* profile);
  static void SendCheckboxNotification(
      views::View* view,
      int type,
      Profile* profile);
  static void SendWindowNotification(
      views::View* view,
      int type,
      Profile* profile);
  static void SendSliderNotification(
      views::View* view,
      int type,
      Profile* profile);

  // Return the name of a view.
  static std::string GetViewName(views::View* view);

  // Get the context of a view - the name of the enclosing group, toolbar, etc.
  static std::string GetViewContext(views::View* view);

  // Return a descendant of this view with a given accessible role, if found.
  static views::View* FindDescendantWithAccessibleRole(
      views::View* view,
      ui::AccessibilityTypes::Role role);

  // Return true if it's an event on a menu.
  static bool IsMenuEvent(views::View* view,
                          int type);

  // Recursively explore all menu items of |menu| and return in |count|
  // the total number of items, and in |index| the 0-based index of
  // |item|, if found. Initialize |count| to zero before calling this
  // method. |index| will be unchanged if the item is not found, so
  // initialize it to -1 to detect this case.
  static void RecursiveGetMenuItemIndexAndCount(views::View* menu,
                                                views::View* item,
                                                int* index,
                                                int* count);

  // Recursively explore the subviews and return the text from the first
  // subview with a role of STATIC_TEXT.
  static std::string RecursiveGetStaticText(views::View* view);

  // The profile associated with the most recent window event  - used to
  // figure out where to route a few events that can't be directly traced
  // to a window with a profile (like menu events).
  Profile* most_recent_profile_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_EVENT_ROUTER_VIEWS_H_
