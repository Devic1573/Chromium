// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/file_based_policy_loader.h"

#include "base/files/file_path_watcher.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "content/public/browser/browser_thread.h"

using ::base::files::FilePathWatcher;
using content::BrowserThread;

namespace {

// Amount of time we wait for the files on disk to settle before trying to load
// them. This alleviates the problem of reading partially written files and
// makes it possible to batch quasi-simultaneous changes.
const int kSettleIntervalSeconds = 5;

// The time interval for rechecking policy. This is our fallback in case the
// delegate never reports a change to the ReloadObserver.
const int kReloadIntervalMinutes = 15;

}  // namespace

namespace policy {

FileBasedPolicyLoader::FileBasedPolicyLoader(
    FileBasedPolicyProvider::ProviderDelegate* provider_delegate)
    : AsynchronousPolicyLoader(provider_delegate,
                               kReloadIntervalMinutes),
      config_file_path_(provider_delegate->config_file_path()),
      settle_interval_(base::TimeDelta::FromSeconds(kSettleIntervalSeconds)) {
}

FileBasedPolicyLoader::~FileBasedPolicyLoader() {}

class FileBasedPolicyWatcherDelegate : public FilePathWatcher::Delegate {
 public:
  explicit FileBasedPolicyWatcherDelegate(
      scoped_refptr<FileBasedPolicyLoader> loader)
      : loader_(loader) {}

  // FilePathWatcher::Delegate implementation:
  virtual void OnFilePathChanged(const FilePath& path) OVERRIDE {
    loader_->OnFilePathChanged(path);
  }

  virtual void OnFilePathError(const FilePath& path) OVERRIDE {
    loader_->OnFilePathError(path);
  }

 private:
  virtual ~FileBasedPolicyWatcherDelegate() {}

  scoped_refptr<FileBasedPolicyLoader> loader_;
  DISALLOW_COPY_AND_ASSIGN(FileBasedPolicyWatcherDelegate);
};

void FileBasedPolicyLoader::OnFilePathChanged(
    const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Reload(false);
}

void FileBasedPolicyLoader::OnFilePathError(const FilePath& path) {
  LOG(ERROR) << "FilePathWatcher on " << path.value()
             << " failed.";
}

void FileBasedPolicyLoader::Reload(bool force) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!delegate()) {
    scoped_ptr<PolicyBundle> empty_bundle;
    PostUpdatePolicyTask(empty_bundle.Pass());
    return;
  }

  // Check the directory time in order to see whether a reload is required.
  base::TimeDelta delay;
  base::Time now = base::Time::Now();
  if (!force && !IsSafeToReloadPolicy(now, &delay)) {
    ScheduleReloadTask(delay);
    return;
  }

  // Load the policy definitions.
  scoped_ptr<PolicyBundle> bundle(delegate()->Load());

  // Check again in case the directory has changed while reading it.
  if (!force && !IsSafeToReloadPolicy(now, &delay)) {
    ScheduleReloadTask(delay);
    return;
  }

  PostUpdatePolicyTask(bundle.Pass());

  ScheduleFallbackReloadTask();
}

void FileBasedPolicyLoader::InitOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  watcher_.reset(new FilePathWatcher);
  const FilePath& path = config_file_path();
  if (!path.empty() &&
      !watcher_->Watch(path, new FileBasedPolicyWatcherDelegate(this))) {
    OnFilePathError(path);
  }

  // There might have been changes to the directory in the time between
  // construction of the loader and initialization of the watcher. Call reload
  // to detect if that is the case.
  Reload(false);

  ScheduleFallbackReloadTask();
}

void FileBasedPolicyLoader::StopOnFileThread() {
  watcher_.reset();
  AsynchronousPolicyLoader::StopOnFileThread();
}

bool FileBasedPolicyLoader::IsSafeToReloadPolicy(
    const base::Time& now,
    base::TimeDelta* delay) {
  DCHECK(delay);

  // A null modification time indicates there's no data.
  FileBasedPolicyProvider::ProviderDelegate* provider_delegate =
      static_cast<FileBasedPolicyProvider::ProviderDelegate*>(delegate());
  base::Time last_modification(provider_delegate->GetLastModification());
  if (last_modification.is_null())
    return true;

  // If there was a change since the last recorded modification, wait some more.
  if (last_modification != last_modification_file_) {
    last_modification_file_ = last_modification;
    last_modification_clock_ = now;
    *delay = settle_interval_;
    return false;
  }

  // Check whether the settle interval has elapsed.
  base::TimeDelta age = now - last_modification_clock_;
  if (age < settle_interval_) {
    *delay = settle_interval_ - age;
    return false;
  }

  return true;
}

}  // namespace policy
