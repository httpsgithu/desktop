/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FILEAPI_FILE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FILEAPI_FILE_H_

#include "base/dcheck_is_on.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class FilePropertyBag;
class FileMetadata;
class FormControlState;
class KURL;
class ExecutionContext;

class CORE_EXPORT File final : public Blob {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // AllContentTypes should only be used when the full path/name are trusted;
  // otherwise, it could allow arbitrary pages to determine what applications an
  // user has installed.
  enum ContentTypeLookupPolicy {
    kWellKnownContentTypes,
    kAllContentTypes,
  };

  // The user should not be able to browse to some files, such as the ones
  // generated by the Filesystem API.
  enum UserVisibility { kIsUserVisible, kIsNotUserVisible };

  // Constructor in File.idl
  static File* Create(ExecutionContext*,
                      const HeapVector<Member<V8BlobPart>>& file_bits,
                      const String& file_name,
                      const FilePropertyBag* options);

  // For deserialization.
  static File* CreateFromSerialization(
      const String& path,
      const String& name,
      const String& relative_path,
      UserVisibility user_visibility,
      bool has_snapshot_data,
      uint64_t size,
      const absl::optional<base::Time>& last_modified,
      scoped_refptr<BlobDataHandle> blob_data_handle) {
    return MakeGarbageCollected<File>(
        path, name, relative_path, user_visibility, has_snapshot_data, size,
        last_modified, std::move(blob_data_handle));
  }
  static File* CreateFromIndexedSerialization(
      const String& name,
      uint64_t size,
      const absl::optional<base::Time>& last_modified,
      scoped_refptr<BlobDataHandle> blob_data_handle) {
    return MakeGarbageCollected<File>(
        String(), name, String(), kIsNotUserVisible, true, size, last_modified,
        std::move(blob_data_handle));
  }

  // For session restore feature.
  // See also AppendToControlState().
  static File* CreateFromControlState(ExecutionContext* context,
                                      const FormControlState& state,
                                      wtf_size_t& index);
  static String PathFromControlState(const FormControlState& state,
                                     wtf_size_t& index);

  static File* CreateWithRelativePath(ExecutionContext* context,
                                      const String& path,
                                      const String& relative_path);

  // If filesystem files live in the remote filesystem, the port might pass the
  // valid metadata (whose length field is non-negative) and cache in the File
  // object.
  //
  // Otherwise calling size(), lastModifiedTime() and slice() will synchronously
  // query the file metadata.
  static File* CreateForFileSystemFile(const String& name,
                                       const FileMetadata& metadata,
                                       UserVisibility user_visibility) {
    return MakeGarbageCollected<File>(name, metadata, user_visibility);
  }

  // KURL has a String() operator, so if this signature is called and not
  // deleted it will overload to the signature above
  // `CreateForFileSystemFile(String, FileMetadata, user_visibility)`.
  static File* CreateForFileSystemFile(const KURL& url,
                                       const FileMetadata& metadata,
                                       UserVisibility user_visibility) = delete;

  static File* CreateForFileSystemFile(
      const KURL& url,
      const FileMetadata& metadata,
      UserVisibility user_visibility,
      scoped_refptr<BlobDataHandle> blob_data_handle) {
    return MakeGarbageCollected<File>(url, metadata, user_visibility,
                                      std::move(blob_data_handle));
  }

  // Calls RegisterBlob through the relevant FileSystemManager, then constructs
  // a File with the resulting BlobDataHandle.
  static File* CreateForFileSystemFile(ExecutionContext& context,
                                       const KURL& url,
                                       const FileMetadata& metadata,
                                       UserVisibility user_visibility);

  File(ExecutionContext* context,
       const String& path,
       ContentTypeLookupPolicy = kWellKnownContentTypes,
       UserVisibility = File::kIsUserVisible);
  File(ExecutionContext* context,
       const String& path,
       const String& name,
       ContentTypeLookupPolicy,
       UserVisibility);
  File(const String& path,
       const String& name,
       const String& relative_path,
       UserVisibility,
       bool has_snapshot_data,
       uint64_t size,
       const absl::optional<base::Time>& last_modified,
       scoped_refptr<BlobDataHandle>);
  File(const String& name,
       const absl::optional<base::Time>& modification_time,
       scoped_refptr<BlobDataHandle>);
  File(const String& name, const FileMetadata&, UserVisibility);
  File(const KURL& file_system_url,
       const FileMetadata& metadata,
       UserVisibility user_visibility,
       scoped_refptr<BlobDataHandle> blob_data_handle);

  File(const File&);

  KURL FileSystemURL() const {
#if DCHECK_IS_ON()
    DCHECK(HasValidFileSystemURL());
#endif
    return file_system_url_;
  }

  // Create a file with a name exposed to the author (via File.name and
  // associated DOM properties) that differs from the one provided in the path.
  static File* CreateForUserProvidedFile(ExecutionContext* context,
                                         const String& path,
                                         const String& display_name) {
    if (display_name.empty()) {
      return MakeGarbageCollected<File>(context, path, File::kAllContentTypes,
                                        File::kIsUserVisible);
    }
    return MakeGarbageCollected<File>(context, path, display_name,
                                      File::kAllContentTypes,
                                      File::kIsUserVisible);
  }

  static File* CreateForFileSystemFile(
      const String& path,
      const String& name,
      ContentTypeLookupPolicy policy = kWellKnownContentTypes) {
    if (name.empty()) {
      return MakeGarbageCollected<File>(/*context=*/nullptr, path, policy,
                                        File::kIsNotUserVisible);
    }
    return MakeGarbageCollected<File>(/*context=*/nullptr, path, name, policy,
                                      File::kIsNotUserVisible);
  }

  File* Clone(const String& name = String()) const;

  // This method calls CaptureSnapshotIfNeeded, and thus can involve synchronous
  // IPC and file operations.
  uint64_t size() const override;

  bool IsFile() const override { return true; }
  bool HasBackingFile() const override { return has_backing_file_; }

  const String& GetPath() const {
#if DCHECK_IS_ON()
    DCHECK(HasValidFilePath());
#endif
    return path_;
  }
  const String& name() const { return name_; }

  // Getter for the lastModified IDL attribute,
  // http://dev.w3.org/2006/webapi/FileAPI/#file-attrs
  // This method calls CaptureSnapshotIfNeeded, and thus can involve synchronous
  // IPC and file operations.
  int64_t lastModified() const;

  // Getter for the lastModifiedDate IDL attribute,
  // http://www.w3.org/TR/FileAPI/#dfn-lastModifiedDate
  // This method calls CaptureSnapshotIfNeeded, and thus can involve synchronous
  // IPC and file operations.
  ScriptValue lastModifiedDate(ScriptState* script_state) const;

  // Returns File's last modified time.
  // If the modification time isn't known, the current time is returned.
  // This method calls CaptureSnapshotIfNeeded, and thus can involve synchronous
  // IPC and file operations.
  base::Time LastModifiedTime() const;

  // Similar to |LastModifiedTime()|, except this returns absl::nullopt rather
  // than the current time if the modified time is unknown.
  // This is used by SerializedScriptValue to serialize the last modified time
  // of a File object.
  // This method calls CaptureSnapshotIfNeeded, and thus can involve synchronous
  // IPC and file operations.
  absl::optional<base::Time> LastModifiedTimeForSerialization() const;

  UserVisibility GetUserVisibility() const { return user_visibility_; }

  // Returns the relative path of this file in the context of a directory
  // selection.
  const String& webkitRelativePath() const { return relative_path_; }

  // Returns true if this has a valid snapshot metadata
  // (i.e. snapshot_size_.has_value()).
  bool HasValidSnapshotMetadata() const { return snapshot_size_.has_value(); }

  // Returns true if the sources (file path, file system URL, or blob handler)
  // of the file objects are same or not.
  bool HasSameSource(const File& other) const;

  // Return false if this File instance is not serializable to FormControlState.
  bool AppendToControlState(FormControlState& state);

 private:
  // Note that this involves synchronous file operation. Think twice before
  // calling this function.
  void CaptureSnapshotIfNeeded() const;

#if DCHECK_IS_ON()
  // Instances backed by a file must have an empty file system URL.
  bool HasValidFileSystemURL() const {
    return !HasBackingFile() || file_system_url_.IsEmpty();
  }
  // Instances not backed by a file must have an empty path set.
  bool HasValidFilePath() const { return HasBackingFile() || path_.empty(); }
#endif

  bool has_backing_file_;
  UserVisibility user_visibility_;
  String path_;
  String name_;

  KURL file_system_url_;

  // If snapshot_size_ has no value, the snapshot metadata is invalid and
  // we retrieve the latest metadata synchronously in size(),
  // LastModifiedTime() and slice().
  // Otherwise, the snapshot metadata are used directly in those methods.
  mutable absl::optional<uint64_t> snapshot_size_;
  mutable absl::optional<base::Time> snapshot_modification_time_;

  String relative_path_;
};

template <>
struct DowncastTraits<File> {
  static bool AllowFrom(const Blob& blob) { return blob.IsFile(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FILEAPI_FILE_H_
