// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/application_launch.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/default_apps_trial.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/platform_app_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/renderer_preferences.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_metrics.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#endif

#if defined(USE_ASH)
#include "ash/ash_switches.h"
#include "chrome/browser/ui/views/ash/panel_view_aura.h"
#endif

using content::WebContents;
using extensions::Extension;

namespace {

// Get the launch URL for a given extension, with optional override/fallback.
// |override_url|, if non-empty, will be preferred over the extension's
// launch url.
GURL UrlForExtension(const Extension* extension,
                     const GURL& override_url) {
  if (!extension)
    return override_url;

  GURL url;
  if (!override_url.is_empty()) {
    DCHECK(extension->web_extent().MatchesURL(override_url));
    url = override_url;
  } else {
    url = extension->GetFullLaunchURL();
  }

  // For extensions lacking launch urls, determine a reasonable fallback.
  if (!url.is_valid()) {
    url = extension->options_url();
    if (!url.is_valid())
      url = GURL(chrome::kChromeUIExtensionsURL);
  }

  return url;
}

bool AllowPanels(const std::string& app_name) {
  return PanelManager::ShouldUsePanels(
      web_app::GetExtensionIdFromApplicationName(app_name));
}

}  // namespace

namespace application_launch {

WebContents* OpenApplication(Profile* profile,
                             const Extension* extension,
                             extension_misc::LaunchContainer container,
                             const GURL& override_url,
                             WindowOpenDisposition disposition,
                             const CommandLine* command_line) {
  WebContents* tab = NULL;
  ExtensionPrefs* prefs = profile->GetExtensionService()->extension_prefs();
  prefs->SetActiveBit(extension->id(), true);

  UMA_HISTOGRAM_ENUMERATION("Extensions.AppLaunchContainer", container, 100);
#if defined(OS_CHROMEOS)
  if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled())
    chromeos::KioskModeMetrics::Get()->UserOpenedApp();
#endif

  if (extension->is_platform_app()) {
    extensions::LaunchPlatformApp(profile, extension, command_line);
    return NULL;
  }

  switch (container) {
    case extension_misc::LAUNCH_NONE: {
      NOTREACHED();
      break;
    }
    case extension_misc::LAUNCH_PANEL:
#if defined(USE_ASH)
      if (extension &&
          CommandLine::ForCurrentProcess()->HasSwitch(
              ash::switches::kAuraPanelManager)) {
        tab = OpenApplicationPanel(profile, extension, override_url);
        break;
      }
      // else fall through to LAUNCH_WINDOW
#endif
    case extension_misc::LAUNCH_WINDOW:
      tab = OpenApplicationWindow(profile, extension, container,
                                  override_url, NULL);
      break;
    case extension_misc::LAUNCH_TAB: {
      tab = OpenApplicationTab(profile, extension, override_url,
                               disposition);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
  return tab;
}

#if defined(USE_ASH)
WebContents* OpenApplicationPanel(
    Profile* profile,
    const Extension* extension,
    const GURL& url_input) {
  GURL url = UrlForExtension(extension, url_input);
  std::string app_name =
      web_app::GenerateApplicationNameFromExtensionId(extension->id());
  gfx::Rect panel_bounds;
  panel_bounds.set_width(extension->launch_width());
  panel_bounds.set_height(extension->launch_height());
  PanelViewAura* panel_view = new PanelViewAura(app_name);
  panel_view->Init(profile, url, panel_bounds);
  return panel_view->WebContents();
}
#endif

WebContents* OpenApplicationWindow(
    Profile* profile,
    const Extension* extension,
    extension_misc::LaunchContainer container,
    const GURL& url_input,
    Browser** app_browser) {
  DCHECK(!url_input.is_empty() || extension);
  GURL url = UrlForExtension(extension, url_input);

  std::string app_name;
  app_name = extension ?
      web_app::GenerateApplicationNameFromExtensionId(extension->id()) :
      web_app::GenerateApplicationNameFromURL(url);

  Browser::Type type = Browser::TYPE_POPUP;
  if (extension &&
      container == extension_misc::LAUNCH_PANEL &&
      AllowPanels(app_name)) {
    type = Browser::TYPE_PANEL;
  }

  gfx::Rect window_bounds;
  if (extension) {
    window_bounds.set_width(extension->launch_width());
    window_bounds.set_height(extension->launch_height());
  }

  Browser::CreateParams params(type, profile);
  params.app_name = app_name;
  params.initial_bounds = window_bounds;

#if defined(USE_ASH)
  if (extension &&
      container == extension_misc::LAUNCH_WINDOW) {
    // In ash, LAUNCH_FULLSCREEN launches in a maximized app window and
    // LAUNCH_WINDOW launches in a normal app window.
    ExtensionPrefs::LaunchType launch_type =
        profile->GetExtensionService()->extension_prefs()->GetLaunchType(
            extension->id(), ExtensionPrefs::LAUNCH_DEFAULT);
    if (launch_type == ExtensionPrefs::LAUNCH_FULLSCREEN)
      params.initial_show_state = ui::SHOW_STATE_MAXIMIZED;
    else if (launch_type == ExtensionPrefs::LAUNCH_WINDOW)
      params.initial_show_state = ui::SHOW_STATE_NORMAL;
  }
#endif

  Browser* browser = Browser::CreateWithParams(params);

  if (app_browser)
    *app_browser = browser;

  TabContentsWrapper* wrapper =
      browser->AddSelectedTabWithURL(url, content::PAGE_TRANSITION_START_PAGE);
  WebContents* contents = wrapper->web_contents();
  contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  contents->GetRenderViewHost()->SyncRendererPrefs();
  // TODO(stevenjb): Find the right centralized place to do this. Currently it
  // is only done for app tabs in normal browsers through SetExtensionAppById.
  if (extension && type == Browser::TYPE_PANEL)
    wrapper->extension_tab_helper()->SetExtensionAppIconById(extension->id());

  browser->window()->Show();

  // TODO(jcampan): http://crbug.com/8123 we should not need to set the initial
  //                focus explicitly.
  contents->GetView()->SetInitialFocus();
  return contents;
}

WebContents* OpenAppShortcutWindow(Profile* profile,
                                   const GURL& url,
                                   bool update_shortcut) {
  Browser* app_browser;
  WebContents* tab = OpenApplicationWindow(
      profile,
      NULL,  // this is a URL app.  No extension.
      extension_misc::LAUNCH_WINDOW,
      url,
      &app_browser);

  if (!tab)
    return NULL;

  if (update_shortcut) {
    // Set UPDATE_SHORTCUT as the pending web app action. This action is picked
    // up in LoadingStateChanged to schedule a GetApplicationInfo. And when
    // the web app info is available, ExtensionTabHelper notifies Browser via
    // OnDidGetApplicationInfo, which calls
    // web_app::UpdateShortcutForTabContents when it sees UPDATE_SHORTCUT as
    // pending web app action.
    app_browser->set_pending_web_app_action(Browser::UPDATE_SHORTCUT);
  }
  return tab;
}

WebContents* OpenApplicationTab(Profile* profile,
                                const Extension* extension,
                                const GURL& override_url,
                                WindowOpenDisposition disposition) {
  Browser* browser = browser::FindTabbedBrowser(profile, false);
  WebContents* contents = NULL;
  if (!browser) {
    // No browser for this profile, need to open a new one.
    browser = Browser::Create(profile);
    browser->window()->Show();
    // There's no current tab in this browser window, so add a new one.
    disposition = NEW_FOREGROUND_TAB;
  } else {
    // For existing browser, ensure its window is activated.
    browser->window()->Activate();
  }

  // Check the prefs for overridden mode.
  ExtensionService* extension_service = profile->GetExtensionService();
  DCHECK(extension_service);

  ExtensionPrefs::LaunchType launch_type =
      extension_service->extension_prefs()->GetLaunchType(
          extension->id(), ExtensionPrefs::LAUNCH_DEFAULT);
  UMA_HISTOGRAM_ENUMERATION("Extensions.AppTabLaunchType", launch_type, 100);

  static bool default_apps_trial_exists =
      base::FieldTrialList::TrialExists(kDefaultAppsTrialName);
  if (default_apps_trial_exists) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("Extensions.AppTabLaunchType",
                                   kDefaultAppsTrialName),
        launch_type, 100);
  }

  int add_type = TabStripModel::ADD_ACTIVE;
  if (launch_type == ExtensionPrefs::LAUNCH_PINNED)
    add_type |= TabStripModel::ADD_PINNED;

  GURL extension_url = UrlForExtension(extension, override_url);
  // TODO(erikkay): START_PAGE doesn't seem like the right transition in all
  // cases.
  browser::NavigateParams params(browser, extension_url,
                                 content::PAGE_TRANSITION_START_PAGE);
  params.tabstrip_add_types = add_type;
  params.disposition = disposition;

  if (disposition == CURRENT_TAB) {
    WebContents* existing_tab = browser->GetSelectedWebContents();
    TabStripModel* model = browser->tab_strip_model();
    int tab_index = model->GetWrapperIndex(existing_tab);

    existing_tab->OpenURL(content::OpenURLParams(
          extension_url,
          content::Referrer(existing_tab->GetURL(),
                            WebKit::WebReferrerPolicyDefault),
          disposition, content::PAGE_TRANSITION_LINK, false));
    // Reset existing_tab as OpenURL() may have clobbered it.
    existing_tab = browser->GetSelectedWebContents();
    if (params.tabstrip_add_types & TabStripModel::ADD_PINNED) {
      model->SetTabPinned(tab_index, true);
      // Pinning may have moved the tab.
      tab_index = model->GetWrapperIndex(existing_tab);
    }
    if (params.tabstrip_add_types & TabStripModel::ADD_ACTIVE)
      model->ActivateTabAt(tab_index, true);

    contents = existing_tab;
  } else {
    browser::Navigate(&params);
    contents = params.target_contents->web_contents();
  }

#if defined(USE_ASH)
  // In ash, LAUNCH_FULLSCREEN launches in a maximized app window and it should
  // not reach here.
  DCHECK(launch_type != ExtensionPrefs::LAUNCH_FULLSCREEN);
#else
  // TODO(skerner):  If we are already in full screen mode, and the user
  // set the app to open as a regular or pinned tab, what should happen?
  // Today we open the tab, but stay in full screen mode.  Should we leave
  // full screen mode in this case?
  if (launch_type == ExtensionPrefs::LAUNCH_FULLSCREEN &&
      !browser->window()->IsFullscreen()) {
    browser->ToggleFullscreenMode();
  }
#endif

  return contents;
}

}  // namespace application_launch
