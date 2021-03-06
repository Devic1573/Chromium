// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_MANAGER_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_icon_manager.h"
#include "chrome/common/extensions/url_pattern_set.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"


class Profile;
class SkBitmap;

namespace content {
class WebContents;
struct ContextMenuParams;
}

namespace extensions {
class Extension;
}

// Represents a menu item added by an extension.
class ExtensionMenuItem {
 public:
  // A list of ExtensionMenuItem's.
  typedef std::vector<ExtensionMenuItem*> List;

  // An Id uniquely identifies a context menu item registered by an extension.
  struct Id {
    Id();
    // Since the unique ID (uid or string_uid) is parsed from API arguments,
    // the normal usage is to set the uid or string_uid immediately after
    // construction.
    Id(bool incognito, const std::string& extension_id);
    ~Id();

    bool operator==(const Id& other) const;
    bool operator!=(const Id& other) const;
    bool operator<(const Id& other) const;

    bool incognito;
    std::string extension_id;
    // Only one of uid or string_uid will be defined.
    int uid;
    std::string string_uid;
  };

  // For context menus, these are the contexts where an item can appear.
  enum Context {
    ALL = 1,
    PAGE = 2,
    SELECTION = 4,
    LINK = 8,
    EDITABLE = 16,
    IMAGE = 32,
    VIDEO = 64,
    AUDIO = 128,
    FRAME = 256,
  };

  // An item can be only one of these types.
  enum Type {
    NORMAL,
    CHECKBOX,
    RADIO,
    SEPARATOR
  };

  // A list of Contexts for an item.
  class ContextList {
   public:
    ContextList() : value_(0) {}
    explicit ContextList(Context context) : value_(context) {}
    ContextList(const ContextList& other) : value_(other.value_) {}

    void operator=(const ContextList& other) {
      value_ = other.value_;
    }

    bool operator==(const ContextList& other) const {
      return value_ == other.value_;
    }

    bool operator!=(const ContextList& other) const {
      return !(*this == other);
    }

    bool Contains(Context context) const {
      return (value_ & context) > 0;
    }

    void Add(Context context) {
      value_ |= context;
    }

    scoped_ptr<Value> ToValue() const {
      return scoped_ptr<Value>(Value::CreateIntegerValue(value_));
    }

    bool Populate(const Value& value) {
      int int_value;
      if (!value.GetAsInteger(&int_value) || int_value < 0)
        return false;
      value_ = int_value;
      return true;
    }

   private:
    uint32 value_;  // A bitmask of Context values.
  };

  ExtensionMenuItem(const Id& id,
                    const std::string& title,
                    bool checked,
                    bool enabled,
                    Type type,
                    const ContextList& contexts);
  virtual ~ExtensionMenuItem();

  // Simple accessor methods.
  bool incognito() const { return id_.incognito; }
  const std::string& extension_id() const { return id_.extension_id; }
  const std::string& title() const { return title_; }
  const List& children() { return children_; }
  const Id& id() const { return id_; }
  Id* parent_id() const { return parent_id_.get(); }
  int child_count() const { return children_.size(); }
  ContextList contexts() const { return contexts_; }
  Type type() const { return type_; }
  bool checked() const { return checked_; }
  bool enabled() const { return enabled_; }
  const URLPatternSet& document_url_patterns() const {
    return document_url_patterns_;
  }
  const URLPatternSet& target_url_patterns() const {
    return target_url_patterns_;
  }

  // Simple mutator methods.
  void set_title(const std::string& new_title) { title_ = new_title; }
  void set_contexts(ContextList contexts) { contexts_ = contexts; }
  void set_type(Type type) { type_ = type; }
  void set_enabled(bool enabled) { enabled_ = enabled; }
  void set_document_url_patterns(const URLPatternSet& patterns) {
    document_url_patterns_ = patterns;
  }
  void set_target_url_patterns(const URLPatternSet& patterns) {
    target_url_patterns_ = patterns;
  }

  // Returns the title with any instances of %s replaced by |selection|. The
  // result will be no longer than |max_length|.
  string16 TitleWithReplacement(const string16& selection,
                                size_t max_length) const;

  // Sets the checked state to |checked|. Returns true if successful.
  bool SetChecked(bool checked);

  // Converts to Value for serialization to preferences.
  scoped_ptr<base::DictionaryValue> ToValue() const;

  // Returns a new ExtensionMenuItem created from |value|, or NULL if there is
  // an error. The caller takes ownership of the ExtensionMenuItem.
  static ExtensionMenuItem* Populate(const std::string& extension_id,
                                     const DictionaryValue& value,
                                     std::string* error);

  // Sets any document and target URL patterns from |properties|.
  bool PopulateURLPatterns(const base::DictionaryValue& properties,
                           const char* document_url_patterns_key,
                           const char* target_url_patterns_key,
                           std::string* error);

 protected:
  friend class ExtensionMenuManager;

  // Takes ownership of |item| and sets its parent_id_.
  void AddChild(ExtensionMenuItem* item);

  // Takes the child item from this parent. The item is returned and the caller
  // then owns the pointer.
  ExtensionMenuItem* ReleaseChild(const Id& child_id, bool recursive);

  // Recursively appends all descendant items (children, grandchildren, etc.)
  // to the output |list|.
  void GetFlattenedSubtree(ExtensionMenuItem::List* list);

  // Recursively removes all descendant items (children, grandchildren, etc.),
  // returning the ids of the removed items.
  std::set<Id> RemoveAllDescendants();

 private:
  // The unique id for this item.
  Id id_;

  // What gets shown in the menu for this item.
  std::string title_;

  Type type_;

  // This should only be true for items of type CHECKBOX or RADIO.
  bool checked_;

  // If the item is enabled or not.
  bool enabled_;

  // In what contexts should the item be shown?
  ContextList contexts_;

  // If this item is a child of another item, the unique id of its parent. If
  // this is a top-level item with no parent, this will be NULL.
  scoped_ptr<Id> parent_id_;

  // Patterns for restricting what documents this item will appear for. This
  // applies to the frame where the click took place.
  URLPatternSet document_url_patterns_;

  // Patterns for restricting where items appear based on the src/href
  // attribute of IMAGE/AUDIO/VIDEO/LINK tags.
  URLPatternSet target_url_patterns_;

  // Any children this item may have.
  List children_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMenuItem);
};

// This class keeps track of menu items added by extensions.
class ExtensionMenuManager : public content::NotificationObserver {
 public:
  explicit ExtensionMenuManager(Profile* profile);
  virtual ~ExtensionMenuManager();

  // Returns the ids of extensions which have menu items registered.
  std::set<std::string> ExtensionIds();

  // Returns a list of all the *top-level* menu items (added via AddContextItem)
  // for the given extension id, *not* including child items (added via
  // AddChildItem); although those can be reached via the top-level items'
  // children. A view can then decide how to display these, including whether to
  // put them into a submenu if there are more than 1.
  const ExtensionMenuItem::List* MenuItems(const std::string& extension_id);

  // Adds a top-level menu item for an extension, requiring the |extension|
  // pointer so it can load the icon for the extension. Takes ownership of
  // |item|. Returns a boolean indicating success or failure.
  bool AddContextItem(const extensions::Extension* extension,
                      ExtensionMenuItem* item);

  // Add an item as a child of another item which has been previously added, and
  // takes ownership of |item|. Returns a boolean indicating success or failure.
  bool AddChildItem(const ExtensionMenuItem::Id& parent_id,
                    ExtensionMenuItem* child);

  // Makes existing item with |child_id| a child of the item with |parent_id|.
  // If the child item was already a child of another parent, this will remove
  // it from that parent first. It is an error to try and move an item to be a
  // child of one of its own descendants. It is legal to pass NULL for
  // |parent_id|, which means the item should be moved to the top-level.
  bool ChangeParent(const ExtensionMenuItem::Id& child_id,
                    const ExtensionMenuItem::Id* parent_id);

  // Removes a context menu item with the given id (whether it is a top-level
  // item or a child of some other item), returning true if the item was found
  // and removed or false otherwise.
  bool RemoveContextMenuItem(const ExtensionMenuItem::Id& id);

  // Removes all items for the given extension id.
  void RemoveAllContextItems(const std::string& extension_id);

  // Returns the item with the given |id| or NULL.
  ExtensionMenuItem* GetItemById(const ExtensionMenuItem::Id& id) const;

  // Notify the ExtensionMenuManager that an item has been updated not through
  // an explicit call into ExtensionMenuManager. For example, if an item is
  // acquired by a call to GetItemById and changed, then this should be called.
  // Returns true if the item was found or false otherwise.
  bool ItemUpdated(const ExtensionMenuItem::Id& id);

  // Called when a menu item is clicked on by the user.
  void ExecuteCommand(Profile* profile, content::WebContents* web_contents,
                      const content::ContextMenuParams& params,
                      const ExtensionMenuItem::Id& menuItemId);

  // This returns a bitmap of width/height kFaviconSize, loaded either from an
  // entry specified in the extension's 'icon' section of the manifest, or a
  // default extension icon.
  const SkBitmap& GetIconForExtension(const std::string& extension_id);

  // Implements the content::NotificationObserver interface.
  virtual void Observe(int type, const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Stores the menu items for the extension in the preferences.
  void WriteToPrefs(const extensions::Extension* extension);

  // Reads menu items for the extension from the preferences. Any invalid
  // items are ignored.
  void ReadFromPrefs(const extensions::Extension* extension);

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionMenuManagerTest, DeleteParent);
  FRIEND_TEST_ALL_PREFIXES(ExtensionMenuManagerTest, RemoveOneByOne);

  // This is a helper function which takes care of de-selecting any other radio
  // items in the same group (i.e. that are adjacent in the list).
  void RadioItemSelected(ExtensionMenuItem* item);

  // Make sure that there is only one radio item selected at once in any run.
  // If there are no radio items selected, then the first item in the run
  // will get selected. If there are multiple radio items selected, then only
  // the last one will get selcted.
  void SanitizeRadioList(const ExtensionMenuItem::List& item_list);

  // Returns true if item is a descendant of an item with id |ancestor_id|.
  bool DescendantOf(ExtensionMenuItem* item,
                    const ExtensionMenuItem::Id& ancestor_id);

  // We keep items organized by mapping an extension id to a list of items.
  typedef std::map<std::string, ExtensionMenuItem::List> MenuItemMap;
  MenuItemMap context_items_;

  // This lets us make lookup by id fast. It maps id to ExtensionMenuItem* for
  // all items the menu manager knows about, including all children of top-level
  // items.
  std::map<ExtensionMenuItem::Id, ExtensionMenuItem*> items_by_id_;

  content::NotificationRegistrar registrar_;

  ExtensionIconManager icon_manager_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMenuManager);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_MANAGER_H_
