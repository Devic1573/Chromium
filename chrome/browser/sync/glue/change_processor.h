// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_GLUE_CHANGE_PROCESSOR_H_
#pragma once

#include "chrome/browser/sync/glue/data_type_error_handler.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "sync/internal_api/change_record.h"

class Profile;

namespace browser_sync {

class ModelAssociator;
class UnrecoverableErrorHandler;

// An interface used to apply changes from the sync model to the browser's
// native model.  This does not currently distinguish between model data types.
class ChangeProcessor {
 public:
  explicit ChangeProcessor(DataTypeErrorHandler* error_handler);
  virtual ~ChangeProcessor();

  // Call when the processor should accept changes from either provided model
  // and apply them to the other.  Both the chrome model and sync_api are
  // expected to be initialized and loaded.  You must have set a valid
  // ModelAssociator and UnrecoverableErrorHandler before using this method,
  // and the two models should be associated w.r.t the ModelAssociator provided.
  // It is safe to call Start again after previously Stop()ing the processor.
  // Subclasses can extract their associated chrome model from |profile| in
  // |StartImpl|.
  void Start(Profile* profile, sync_api::UserShare* share_handle);
  void Stop();
  virtual bool IsRunning() const;

  // Changes have been applied to the backend model and are ready to be
  // applied to the frontend model. See syncapi.h for detailed instructions on
  // how to interpret and process |changes|.
  virtual void ApplyChangesFromSyncModel(
      const sync_api::BaseTransaction* trans,
      const sync_api::ImmutableChangeRecordList& changes) = 0;

  // The changes found in ApplyChangesFromSyncModel may be too slow to be
  // performed while holding a [Read/Write]Transaction lock or may interact
  // with another thread, which might itself be waiting on the transaction lock,
  // putting us at risk of deadlock.
  // This function is called once the transactional lock is released and it is
  // safe to perform inter-thread or slow I/O operations. Note that not all
  // datatypes need this, so we provide an empty default version.
  virtual void CommitChangesFromSyncModel();

  // This ensures that startobserving gets called after stopobserving even
  // if there is an early return in the function.
  template <class T>
  class ScopedStopObserving {
   public:
    explicit ScopedStopObserving(T* processor)
        : processor_(processor) {
      processor_->StopObserving();
    }
    ~ScopedStopObserving() {
      processor_->StartObserving();
    }

   private:
    ScopedStopObserving() {}
    T* processor_;
  };

 protected:
  // These methods are invoked by Start() and Stop() to do
  // implementation-specific work.
  virtual void StartImpl(Profile* profile) = 0;
  virtual void StopImpl() = 0;

  bool running() const { return running_; }
  DataTypeErrorHandler* error_handler() const;
  virtual sync_api::UserShare* share_handle() const;

 private:
  bool running_;  // True if we have been told it is safe to process changes.
  DataTypeErrorHandler* error_handler_;  // Guaranteed to outlive us.

  // The sync model we are processing changes from. Non-NULL when |running_| is
  // true.
  sync_api::UserShare* share_handle_;

  DISALLOW_COPY_AND_ASSIGN(ChangeProcessor);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_CHANGE_PROCESSOR_H_
