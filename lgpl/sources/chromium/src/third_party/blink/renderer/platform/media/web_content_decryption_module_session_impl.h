// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_WEB_CONTENT_DECRYPTION_MODULE_SESSION_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_WEB_CONTENT_DECRYPTION_MODULE_SESSION_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/content_decryption_module.h"
#include "third_party/blink/public/platform/web_content_decryption_module_session.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/media/new_session_cdm_result_promise.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {
class CdmSessionAdapter;

class PLATFORM_EXPORT WebContentDecryptionModuleSessionImpl
    : public WebContentDecryptionModuleSession {
 public:
  WebContentDecryptionModuleSessionImpl(
      const scoped_refptr<CdmSessionAdapter>& adapter,
      WebEncryptedMediaSessionType session_type);
  WebContentDecryptionModuleSessionImpl(
      const WebContentDecryptionModuleSessionImpl&) = delete;
  WebContentDecryptionModuleSessionImpl& operator=(
      const WebContentDecryptionModuleSessionImpl&) = delete;
  ~WebContentDecryptionModuleSessionImpl() override;

  // WebContentDecryptionModuleSession implementation.
  void SetClientInterface(Client* client) override;
  WebString SessionId() const override;

  void InitializeNewSession(media::EmeInitDataType init_data_type,
                            const unsigned char* initData,
                            size_t initDataLength,
                            WebContentDecryptionModuleResult result) override;
  void Load(const WebString& session_id,
            WebContentDecryptionModuleResult result) override;
  void Update(const uint8_t* response,
              size_t response_length,
              WebContentDecryptionModuleResult result) override;
  void Close(WebContentDecryptionModuleResult result) override;
  void Remove(WebContentDecryptionModuleResult result) override;

  // Callbacks.
  void OnSessionMessage(media::CdmMessageType message_type,
                        const std::vector<uint8_t>& message);
  void OnSessionKeysChange(bool has_additional_usable_key,
                           media::CdmKeysInfo keys_info);
  void OnSessionExpirationUpdate(base::Time new_expiry_time);
  void OnSessionClosed(media::CdmSessionClosedReason reason);

 private:
  // Called when a new session is created or loaded. |status| is set as
  // appropriate, depending on whether the session already exists or not.
  void OnSessionInitialized(const std::string& session_id,
                            SessionInitStatus* status);

  scoped_refptr<CdmSessionAdapter> adapter_;

  // Keep track of the session type to be passed into InitializeNewSession() and
  // LoadSession().
  const media::CdmSessionType session_type_;

  // Non-owned pointer.
  Client* client_;

  // Session ID is the app visible ID for this session generated by the CDM.
  // This value is not set until the CDM resolves the initializeNewSession()
  // promise.
  std::string session_id_;

  // Keep track of whether the session has been closed or not. The session
  // may be closed as a result of an application calling close(), or the CDM
  // may close the session at any point.
  // https://w3c.github.io/encrypted-media/#session-closed
  // |has_close_been_called_| is used to keep track of whether close() has
  // been called or not. |is_closed_| is used to keep track of whether the
  // close event has been received or not.
  bool has_close_been_called_;
  bool is_closed_;

  THREAD_CHECKER(thread_checker_);

  // Since promises will live until they are fired, use a weak reference when
  // creating a promise in case this class disappears before the promise
  // actually fires.
  base::WeakPtrFactory<WebContentDecryptionModuleSessionImpl> weak_ptr_factory_{
      this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_WEB_CONTENT_DECRYPTION_MODULE_SESSION_IMPL_H_
