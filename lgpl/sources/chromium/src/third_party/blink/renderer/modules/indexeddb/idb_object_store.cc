/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/indexeddb/idb_object_store.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key_range.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/modules/v8/to_v8_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/core/dom/dom_string_list.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_any.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_cursor_with_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_database.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_path.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_tracing.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "v8/include/v8.h"

using blink::WebBlobInfo;
using blink::WebIDBCallbacks;
using blink::WebIDBCursor;
using blink::WebIDBDatabase;

namespace blink {

IDBObjectStore::IDBObjectStore(scoped_refptr<IDBObjectStoreMetadata> metadata,
                               IDBTransaction* transaction)
    : metadata_(std::move(metadata)), transaction_(transaction) {
  DCHECK(transaction_);
  DCHECK(metadata_.get());
}

void IDBObjectStore::Trace(blink::Visitor* visitor) {
  visitor->Trace(transaction_);
  visitor->Trace(index_map_);
  ScriptWrappable::Trace(visitor);
}

void IDBObjectStore::setName(const String& name,
                             ExceptionState& exception_state) {
  IDB_TRACE("IDBObjectStore::setName");
  if (!transaction_->IsVersionChange()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kNotVersionChangeTransactionErrorMessage);
    return;
  }
  if (IsDeleted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kObjectStoreDeletedErrorMessage);
    return;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return;
  }

  if (this->name() == name)
    return;
  if (transaction_->db()->ContainsObjectStore(name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kConstraintError,
        IDBDatabase::kObjectStoreNameTakenErrorMessage);
    return;
  }
  if (!BackendDB()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return;
  }

  transaction_->db()->RenameObjectStore(Id(), name);
}

ScriptValue IDBObjectStore::keyPath(ScriptState* script_state) const {
  return ScriptValue::From(script_state, Metadata().key_path);
}

DOMStringList* IDBObjectStore::indexNames() const {
  IDB_TRACE1("IDBObjectStore::indexNames", "store_name",
             metadata_->name.Utf8());
  DOMStringList* index_names = DOMStringList::Create();
  for (const auto& it : Metadata().indexes)
    index_names->Append(it.value->name);
  index_names->Sort();
  return index_names;
}

IDBRequest* IDBObjectStore::get(ScriptState* script_state,
                                const ScriptValue& key,
                                ExceptionState& exception_state) {
  IDB_TRACE1("IDBObjectStore::getRequestSetup", "store_name",
             metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBObjectStore::get");
  if (IsDeleted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kObjectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }
  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), key, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!key_range) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        IDBDatabase::kNoKeyOrKeyRangeErrorMessage);
    return nullptr;
  }
  if (!BackendDB()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  BackendDB()->Get(transaction_->Id(), Id(), IDBIndexMetadata::kInvalidId,
                   key_range, /*key_only=*/false,
                   request->CreateWebCallbacks().release());
  return request;
}

IDBRequest* IDBObjectStore::getKey(ScriptState* script_state,
                                   const ScriptValue& key,
                                   ExceptionState& exception_state) {
  IDB_TRACE1("IDBObjectStore::getKeyRequestSetup", "store_name",
             metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBObjectStore::getKey");
  if (IsDeleted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kObjectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }
  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), key, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!key_range) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        IDBDatabase::kNoKeyOrKeyRangeErrorMessage);
    return nullptr;
  }
  if (!BackendDB()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  BackendDB()->Get(transaction_->Id(), Id(), IDBIndexMetadata::kInvalidId,
                   key_range, /*key_only=*/true,
                   request->CreateWebCallbacks().release());
  return request;
}

IDBRequest* IDBObjectStore::getAll(ScriptState* script_state,
                                   const ScriptValue& key_range,
                                   ExceptionState& exception_state) {
  return getAll(script_state, key_range, std::numeric_limits<uint32_t>::max(),
                exception_state);
}

IDBRequest* IDBObjectStore::getAll(ScriptState* script_state,
                                   const ScriptValue& key_range,
                                   unsigned long max_count,
                                   ExceptionState& exception_state) {
  IDB_TRACE1("IDBObjectStore::getAllRequestSetup", "store_name",
             metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBObjectStore::getAll");
  if (!max_count)
    max_count = std::numeric_limits<uint32_t>::max();

  if (IsDeleted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kObjectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }
  IDBKeyRange* range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), key_range, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!BackendDB()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  BackendDB()->GetAll(transaction_->Id(), Id(), IDBIndexMetadata::kInvalidId,
                      range, max_count, false,
                      request->CreateWebCallbacks().release());
  return request;
}

IDBRequest* IDBObjectStore::getAllKeys(ScriptState* script_state,
                                       const ScriptValue& key_range,
                                       ExceptionState& exception_state) {
  return getAllKeys(script_state, key_range,
                    std::numeric_limits<uint32_t>::max(), exception_state);
}

IDBRequest* IDBObjectStore::getAllKeys(ScriptState* script_state,
                                       const ScriptValue& key_range,
                                       unsigned long max_count,
                                       ExceptionState& exception_state) {
  IDB_TRACE1("IDBObjectStore::getAllKeysRequestSetup", "store_name",
             metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBObjectStore::getAllKeys");
  if (!max_count)
    max_count = std::numeric_limits<uint32_t>::max();

  if (IsDeleted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kObjectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }
  IDBKeyRange* range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), key_range, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!BackendDB()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  BackendDB()->GetAll(transaction_->Id(), Id(), IDBIndexMetadata::kInvalidId,
                      range, max_count, true,
                      request->CreateWebCallbacks().release());
  return request;
}

static WebVector<WebIDBKey> GenerateIndexKeysForValue(
    v8::Isolate* isolate,
    const IDBIndexMetadata& index_metadata,
    const ScriptValue& object_value) {
  NonThrowableExceptionState exception_state;
  std::unique_ptr<IDBKey> index_key = ScriptValue::To<std::unique_ptr<IDBKey>>(
      isolate, object_value, exception_state, index_metadata.key_path);
  if (!index_key)
    return WebVector<WebIDBKey>();

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, key_type_histogram,
      ("WebCore.IndexedDB.ObjectStore.IndexEntry.KeyType",
       static_cast<int>(IDBKey::kTypeEnumMax)));

  if (!index_metadata.multi_entry ||
      index_key->GetType() != IDBKey::kArrayType) {
    if (!index_key->IsValid())
      return WebVector<WebIDBKey>();

    WebVector<WebIDBKey> index_keys;
    index_keys.reserve(1);
    index_keys.emplace_back(std::move(index_key));
    key_type_histogram.Count(static_cast<int>(index_keys[0].View().KeyType()));
    return WebVector<WebIDBKey>(std::move(index_keys));
  } else {
    DCHECK(index_metadata.multi_entry);
    DCHECK_EQ(index_key->GetType(), IDBKey::kArrayType);
    WebVector<WebIDBKey> index_keys =
        IDBKey::ToMultiEntryArray(std::move(index_key));
    for (const WebIDBKey& key : index_keys)
      key_type_histogram.Count(static_cast<int>(key.View().KeyType()));
    return index_keys;
  }
}

IDBRequest* IDBObjectStore::add(ScriptState* script_state,
                                const ScriptValue& value,
                                const ScriptValue& key,
                                ExceptionState& exception_state) {
  IDB_TRACE1("IDBObjectStore::addRequestSetup", "store_name",
             metadata_->name.Utf8());
  return DoPut(script_state, kWebIDBPutModeAddOnly, value, key,
               exception_state);
}

IDBRequest* IDBObjectStore::put(ScriptState* script_state,
                                const ScriptValue& value,
                                const ScriptValue& key,
                                ExceptionState& exception_state) {
  IDB_TRACE1("IDBObjectStore::putRequestSetup", "store_name",
             metadata_->name.Utf8());
  return DoPut(script_state, kWebIDBPutModeAddOrUpdate, value, key,
               exception_state);
}

IDBRequest* IDBObjectStore::DoPut(ScriptState* script_state,
                                  WebIDBPutMode put_mode,
                                  const ScriptValue& value,
                                  const ScriptValue& key_value,
                                  ExceptionState& exception_state) {
  std::unique_ptr<IDBKey> key =
      key_value.IsUndefined()
          ? nullptr
          : ScriptValue::To<std::unique_ptr<IDBKey>>(
                script_state->GetIsolate(), key_value, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return DoPut(script_state, put_mode,
               IDBRequest::Source::FromIDBObjectStore(this), value, key.get(),
               exception_state);
}

IDBRequest* IDBObjectStore::DoPut(ScriptState* script_state,
                                  WebIDBPutMode put_mode,
                                  const IDBRequest::Source& source,
                                  const ScriptValue& value,
                                  const IDBKey* key,
                                  ExceptionState& exception_state) {
  const char* tracing_name = nullptr;
  switch (put_mode) {
    case kWebIDBPutModeAddOrUpdate:
      tracing_name = "IDBObjectStore::put";
      break;
    case kWebIDBPutModeAddOnly:
      tracing_name = "IDBObjectStore::add";
      break;
    case kWebIDBPutModeCursorUpdate:
      tracing_name = "IDBCursor::update";
      break;
  }
  IDBRequest::AsyncTraceState metrics(tracing_name);
  if (IsDeleted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kObjectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }
  if (transaction_->IsReadOnly()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kReadOnlyError,
        IDBDatabase::kTransactionReadOnlyErrorMessage);
    return nullptr;
  }

  v8::Isolate* isolate = script_state->GetIsolate();
  DCHECK(isolate->InContext());
  // TODO(crbug.com/719053): This wasm behavior differs from other browsers.
  SerializedScriptValue::SerializeOptions::WasmSerializationPolicy wasm_policy =
      ExecutionContext::From(script_state)->IsSecureContext()
          ? SerializedScriptValue::SerializeOptions::kSerialize
          : SerializedScriptValue::SerializeOptions::kBlockedInNonSecureContext;
  IDBValueWrapper value_wrapper(isolate, value.V8Value(), wasm_policy,
                                exception_state);
  if (exception_state.HadException())
    return nullptr;

  // Keys that need to be extracted must be taken from a clone so that
  // side effects (i.e. getters) are not triggered. Construct the
  // clone lazily since the operation may be expensive.
  ScriptValue clone;

  const IDBKeyPath& key_path = IdbKeyPath();
  const bool uses_in_line_keys = !key_path.IsNull();
  const bool has_key_generator = autoIncrement();

  if (put_mode != kWebIDBPutModeCursorUpdate && uses_in_line_keys && key) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The object store uses in-line keys and "
                                      "the key parameter was provided.");
    return nullptr;
  }

  // If the primary key is extracted from the value using a key path, this holds
  // onto the extracted key for the duration of the method.
  std::unique_ptr<IDBKey> key_path_key;

  // This test logically belongs in IDBCursor, but must operate on the cloned
  // value.
  if (put_mode == kWebIDBPutModeCursorUpdate && uses_in_line_keys) {
    DCHECK(key);
    DCHECK(clone.IsEmpty());
    value_wrapper.Clone(script_state, &clone);

    DCHECK(!key_path_key);
    key_path_key = ScriptValue::To<std::unique_ptr<IDBKey>>(
        script_state->GetIsolate(), clone, exception_state, key_path);
    if (exception_state.HadException())
      return nullptr;
    if (!key_path_key || !key_path_key->IsEqual(key)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "The effective object store of this cursor uses in-line keys and "
          "evaluating the key path of the value parameter results in a "
          "different value than the cursor's effective key.");
      return nullptr;
    }
  }

  if (!uses_in_line_keys && !has_key_generator && !key) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      "The object store uses out-of-line keys "
                                      "and has no key generator and the key "
                                      "parameter was not provided.");
    return nullptr;
  }

  if (uses_in_line_keys) {
    if (clone.IsEmpty()) {
      // For an IDBCursor.update(), the value should have been cloned above.
      DCHECK(put_mode != kWebIDBPutModeCursorUpdate);
      value_wrapper.Clone(script_state, &clone);

      DCHECK(!key_path_key);
      key_path_key = ScriptValue::To<std::unique_ptr<IDBKey>>(
          script_state->GetIsolate(), clone, exception_state, key_path);
      if (exception_state.HadException())
        return nullptr;
      if (key_path_key && !key_path_key->IsValid()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kDataError,
            "Evaluating the object store's key path yielded a value that is "
            "not a valid key.");
        return nullptr;
      }
    } else {
      // The clone was created in the large if block above. The block should
      // have thrown if key_path_key is not valid.
      DCHECK(put_mode == kWebIDBPutModeCursorUpdate);
      DCHECK(key_path_key && key_path_key->IsValid());
    }

    // The clone should have either been created in the if block right above,
    // or in the large if block further above above. Both if blocks throw if
    // key_path_key is populated with an invalid key. However, the latter block,
    // which handles IDBObjectStore.put() and IDBObjectStore.add(), may end up
    // with a null key_path_key. This is acceptable for object stores with key
    // generators (autoIncrement is true).
    DCHECK(!key_path_key || key_path_key->IsValid());

    if (!key_path_key) {
      if (!has_key_generator) {
        exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                          "Evaluating the object store's key "
                                          "path did not yield a value.");
        return nullptr;
      }

      // Auto-incremented keys must be generated by the backing store, to
      // ensure uniqueness when the same IndexedDB database is concurrently
      // accessed by multiple render processes. This check ensures that we'll be
      // able to inject the generated key into the supplied value when we read
      // them from the backing store.
      if (!CanInjectIDBKeyIntoScriptValue(script_state->GetIsolate(), clone,
                                          key_path)) {
        exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                          "A generated key could not be "
                                          "inserted into the value.");
        return nullptr;
      }
    }

    if (key_path_key)
      key = key_path_key.get();
  }

  if (key && !key->IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return nullptr;
  }

  if (!BackendDB()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  if (key && uses_in_line_keys) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        EnumerationHistogram, key_type_histogram,
        ("WebCore.IndexedDB.ObjectStore.Record.KeyType",
         static_cast<int>(IDBKey::kTypeEnumMax)));
    key_type_histogram.Count(static_cast<int>(key->GetType()));
  }

  WebVector<WebIDBIndexKeys> index_keys;
  index_keys.reserve(Metadata().indexes.size());
  for (const auto& it : Metadata().indexes) {
    if (clone.IsEmpty())
      value_wrapper.Clone(script_state, &clone);
    index_keys.emplace_back(
        it.key, GenerateIndexKeysForValue(script_state->GetIsolate(), *it.value,
                                          clone));
  }
  // Records 1KB to 1GB.
  UMA_HISTOGRAM_COUNTS_1M(
      "WebCore.IndexedDB.PutValueSize2",
      base::saturated_cast<base::HistogramBase::Sample>(
          value_wrapper.DataLengthBeforeWrapInBytes() / 1024));

  IDBRequest* request = IDBRequest::Create(
      script_state, source, transaction_.Get(), std::move(metrics));

  value_wrapper.DoneCloning();

  if (base::FeatureList::IsEnabled(kIndexedDBLargeValueWrapping))
    value_wrapper.WrapIfBiggerThan(IDBValueWrapper::kWrapThreshold);

  request->transit_blob_handles() = value_wrapper.TakeBlobDataHandles();
  BackendDB()->Put(
      transaction_->Id(), Id(), WebData(value_wrapper.TakeWireBytes()),
      value_wrapper.TakeBlobInfo(), WebIDBKeyView(key),
      static_cast<WebIDBPutMode>(put_mode),
      request->CreateWebCallbacks().release(), std::move(index_keys));

  return request;
}

IDBRequest* IDBObjectStore::Delete(ScriptState* script_state,
                                   const ScriptValue& key,
                                   ExceptionState& exception_state) {
  IDB_TRACE1("IDBObjectStore::deleteRequestSetup", "store_name",
             metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBObjectStore::delete");
  if (IsDeleted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kObjectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }
  if (transaction_->IsReadOnly()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kReadOnlyError,
        IDBDatabase::kTransactionReadOnlyErrorMessage);
    return nullptr;
  }

  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), key, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!key_range) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kDataError,
        IDBDatabase::kNoKeyOrKeyRangeErrorMessage);
    return nullptr;
  }
  if (!BackendDB()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  return deleteFunction(script_state, key_range, std::move(metrics));
}

IDBRequest* IDBObjectStore::deleteFunction(
    ScriptState* script_state,
    IDBKeyRange* key_range,
    IDBRequest::AsyncTraceState metrics) {
  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));

  BackendDB()->DeleteRange(transaction_->Id(), Id(), key_range,
                           request->CreateWebCallbacks().release());
  return request;
}

IDBRequest* IDBObjectStore::clear(ScriptState* script_state,
                                  ExceptionState& exception_state) {
  IDB_TRACE("IDBObjectStore::clearRequestSetup");
  IDBRequest::AsyncTraceState metrics("IDBObjectStore::clear");
  if (IsDeleted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kObjectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }
  if (transaction_->IsReadOnly()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kReadOnlyError,
        IDBDatabase::kTransactionReadOnlyErrorMessage);
    return nullptr;
  }
  if (!BackendDB()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  BackendDB()->Clear(transaction_->Id(), Id(),
                     request->CreateWebCallbacks().release());
  return request;
}

namespace {
// This class creates the index keys for a given index by extracting
// them from the SerializedScriptValue, for all the existing values in
// the object store. It only needs to be kept alive by virtue of being
// a listener on an IDBRequest object, in the same way that JavaScript
// cursor success handlers are kept alive.
class IndexPopulator final : public EventListener {
 public:
  static IndexPopulator* Create(
      ScriptState* script_state,
      IDBDatabase* database,
      int64_t transaction_id,
      int64_t object_store_id,
      scoped_refptr<const IDBIndexMetadata> index_metadata) {
    return new IndexPopulator(script_state, database, transaction_id,
                              object_store_id, std::move(index_metadata));
  }

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(script_state_);
    visitor->Trace(database_);
    EventListener::Trace(visitor);
  }

 private:
  IndexPopulator(ScriptState* script_state,
                 IDBDatabase* database,
                 int64_t transaction_id,
                 int64_t object_store_id,
                 scoped_refptr<const IDBIndexMetadata> index_metadata)
      : EventListener(kCPPEventListenerType),
        script_state_(script_state),
        database_(database),
        transaction_id_(transaction_id),
        object_store_id_(object_store_id),
        index_metadata_(std::move(index_metadata)) {
    DCHECK(index_metadata_.get());
  }

  const IDBIndexMetadata& IndexMetadata() const { return *index_metadata_; }

  void handleEvent(ExecutionContext* execution_context, Event* event) override {
    if (!script_state_->ContextIsValid())
      return;
    IDB_TRACE("IDBObjectStore::IndexPopulator::handleEvent");

    DCHECK_EQ(ExecutionContext::From(script_state_), execution_context);
    DCHECK_EQ(event->type(), EventTypeNames::success);
    EventTarget* target = event->target();
    IDBRequest* request = static_cast<IDBRequest*>(target);

    if (!database_->Backend())  // If database is stopped?
      return;

    ScriptState::Scope scope(script_state_);

    IDBAny* cursor_any = request->ResultAsAny();
    IDBCursorWithValue* cursor = nullptr;
    if (cursor_any->GetType() == IDBAny::kIDBCursorWithValueType)
      cursor = cursor_any->IdbCursorWithValue();

    if (cursor && !cursor->IsDeleted()) {
      cursor->Continue(nullptr, nullptr, IDBRequest::AsyncTraceState(),
                       ASSERT_NO_EXCEPTION);

      const IDBKey* primary_key = cursor->IdbPrimaryKey();
      ScriptValue value = cursor->value(script_state_);

      WebVector<WebIDBIndexKeys> index_keys;
      index_keys.reserve(1);
      index_keys.emplace_back(
          IndexMetadata().id,
          GenerateIndexKeysForValue(script_state_->GetIsolate(),
                                    IndexMetadata(), value));

      database_->Backend()->SetIndexKeys(transaction_id_, object_store_id_,
                                         WebIDBKeyView(primary_key),
                                         std::move(index_keys));
    } else {
      // Now that we are done indexing, tell the backend to go
      // back to processing tasks of type NormalTask.
      Vector<int64_t> index_ids;
      index_ids.push_back(IndexMetadata().id);
      database_->Backend()->SetIndexesReady(transaction_id_, object_store_id_,
                                            index_ids);
      database_.Clear();
    }
  }

  Member<ScriptState> script_state_;
  Member<IDBDatabase> database_;
  const int64_t transaction_id_;
  const int64_t object_store_id_;
  scoped_refptr<const IDBIndexMetadata> index_metadata_;
};
}  // namespace

IDBIndex* IDBObjectStore::createIndex(ScriptState* script_state,
                                      const String& name,
                                      const IDBKeyPath& key_path,
                                      const IDBIndexParameters& options,
                                      ExceptionState& exception_state) {
  IDB_TRACE1("IDBObjectStore::createIndexRequestSetup", "store_name",
             metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBObjectStore::createIndex");
  if (!transaction_->IsVersionChange()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kNotVersionChangeTransactionErrorMessage);
    return nullptr;
  }
  if (IsDeleted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kObjectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }
  if (ContainsIndex(name)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      IDBDatabase::kIndexNameTakenErrorMessage);
    return nullptr;
  }
  if (!key_path.IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The keyPath argument contains an invalid key path.");
    return nullptr;
  }
  if (key_path.GetType() == IDBKeyPath::kArrayType && options.multiEntry()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The keyPath argument was an array and the multiEntry option is true.");
    return nullptr;
  }
  if (!BackendDB()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  int64_t index_id = metadata_->max_index_id + 1;
  DCHECK_NE(index_id, IDBIndexMetadata::kInvalidId);
  BackendDB()->CreateIndex(transaction_->Id(), Id(), index_id, name, key_path,
                           options.unique(), options.multiEntry());

  ++metadata_->max_index_id;

  scoped_refptr<IDBIndexMetadata> index_metadata =
      base::AdoptRef(new IDBIndexMetadata(
          name, index_id, key_path, options.unique(), options.multiEntry()));
  IDBIndex* index = IDBIndex::Create(index_metadata, this, transaction_.Get());
  index_map_.Set(name, index);
  metadata_->indexes.Set(index_id, index_metadata);

  DCHECK(!exception_state.HadException());
  if (exception_state.HadException())
    return nullptr;

  IDBRequest* index_request =
      openCursor(script_state, nullptr, kWebIDBCursorDirectionNext,
                 kWebIDBTaskTypePreemptive, std::move(metrics));
  index_request->PreventPropagation();

  // This is kept alive by being the success handler of the request, which is in
  // turn kept alive by the owning transaction.
  IndexPopulator* index_populator = IndexPopulator::Create(
      script_state, transaction()->db(), transaction_->Id(), Id(),
      std::move(index_metadata));
  index_request->setOnsuccess(index_populator);
  return index;
}

IDBIndex* IDBObjectStore::index(const String& name,
                                ExceptionState& exception_state) {
  IDB_TRACE1("IDBObjectStore::index", "store_name", metadata_->name.Utf8());
  if (IsDeleted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kObjectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (transaction_->IsFinished() || transaction_->IsFinishing()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kTransactionFinishedErrorMessage);
    return nullptr;
  }

  IDBIndexMap::iterator it = index_map_.find(name);
  if (it != index_map_.end())
    return it->value;

  int64_t index_id = FindIndexId(name);
  if (index_id == IDBIndexMetadata::kInvalidId) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      IDBDatabase::kNoSuchIndexErrorMessage);
    return nullptr;
  }

  DCHECK(Metadata().indexes.Contains(index_id));
  scoped_refptr<IDBIndexMetadata> index_metadata =
      Metadata().indexes.at(index_id);
  DCHECK(index_metadata.get());
  IDBIndex* index =
      IDBIndex::Create(std::move(index_metadata), this, transaction_.Get());
  index_map_.Set(name, index);
  return index;
}

void IDBObjectStore::deleteIndex(const String& name,
                                 ExceptionState& exception_state) {
  IDB_TRACE1("IDBObjectStore::deleteIndex", "store_name",
             metadata_->name.Utf8());
  if (!transaction_->IsVersionChange()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kNotVersionChangeTransactionErrorMessage);
    return;
  }
  if (IsDeleted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kObjectStoreDeletedErrorMessage);
    return;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return;
  }
  int64_t index_id = FindIndexId(name);
  if (index_id == IDBIndexMetadata::kInvalidId) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      IDBDatabase::kNoSuchIndexErrorMessage);
    return;
  }
  if (!BackendDB()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return;
  }

  BackendDB()->DeleteIndex(transaction_->Id(), Id(), index_id);

  metadata_->indexes.erase(index_id);
  IDBIndexMap::iterator it = index_map_.find(name);
  if (it != index_map_.end()) {
    transaction_->IndexDeleted(it->value);
    it->value->MarkDeleted();
    index_map_.erase(name);
  }
}

IDBRequest* IDBObjectStore::openCursor(ScriptState* script_state,
                                       const ScriptValue& range,
                                       const String& direction_string,
                                       ExceptionState& exception_state) {
  IDB_TRACE1("IDBObjectStore::openCursorRequestSetup", "store_name",
             metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBObjectStore::openCursor");
  if (IsDeleted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kObjectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }

  WebIDBCursorDirection direction =
      IDBCursor::StringToDirection(direction_string);
  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), range, exception_state);
  if (exception_state.HadException())
    return nullptr;

  if (!BackendDB()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  return openCursor(script_state, key_range, direction, kWebIDBTaskTypeNormal,
                    std::move(metrics));
}

IDBRequest* IDBObjectStore::openCursor(ScriptState* script_state,
                                       IDBKeyRange* range,
                                       WebIDBCursorDirection direction,
                                       WebIDBTaskType task_type,
                                       IDBRequest::AsyncTraceState metrics) {
  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  request->SetCursorDetails(IndexedDB::kCursorKeyAndValue, direction);

  BackendDB()->OpenCursor(transaction_->Id(), Id(),
                          IDBIndexMetadata::kInvalidId, range, direction, false,
                          task_type, request->CreateWebCallbacks().release());
  return request;
}

IDBRequest* IDBObjectStore::openKeyCursor(ScriptState* script_state,
                                          const ScriptValue& range,
                                          const String& direction_string,
                                          ExceptionState& exception_state) {
  IDB_TRACE1("IDBObjectStore::openKeyCursorRequestSetup", "store_name",
             metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBObjectStore::openKeyCursor");
  if (IsDeleted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kObjectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }

  WebIDBCursorDirection direction =
      IDBCursor::StringToDirection(direction_string);
  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), range, exception_state);
  if (exception_state.HadException())
    return nullptr;

  if (!BackendDB()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  request->SetCursorDetails(IndexedDB::kCursorKeyOnly, direction);

  BackendDB()->OpenCursor(transaction_->Id(), Id(),
                          IDBIndexMetadata::kInvalidId, key_range, direction,
                          true, kWebIDBTaskTypeNormal,
                          request->CreateWebCallbacks().release());
  return request;
}

IDBRequest* IDBObjectStore::count(ScriptState* script_state,
                                  const ScriptValue& range,
                                  ExceptionState& exception_state) {
  IDB_TRACE1("IDBObjectStore::countRequestSetup", "store_name",
             metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBObjectStore::count");
  if (IsDeleted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        IDBDatabase::kObjectStoreDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kTransactionInactiveError,
        transaction_->InactiveErrorMessage());
    return nullptr;
  }

  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), range, exception_state);
  if (exception_state.HadException())
    return nullptr;

  if (!BackendDB()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  BackendDB()->Count(transaction_->Id(), Id(), IDBIndexMetadata::kInvalidId,
                     key_range, request->CreateWebCallbacks().release());
  return request;
}

void IDBObjectStore::MarkDeleted() {
  DCHECK(transaction_->IsVersionChange())
      << "An object store got deleted outside a versionchange transaction.";

  deleted_ = true;
  metadata_->indexes.clear();

  for (auto& it : index_map_) {
    IDBIndex* index = it.value;
    index->MarkDeleted();
  }
}

void IDBObjectStore::ClearIndexCache() {
  DCHECK(!transaction_->IsActive() || (IsDeleted() && IsNewlyCreated()));

#if DCHECK_IS_ON()
  // There is no harm in having ClearIndexCache() happen multiple times for
  // the same object. We assert that it is called once to uncover potential
  // object store accounting bugs.
  DCHECK(!clear_index_cache_called_);
  clear_index_cache_called_ = true;
#endif  // DCHECK_IS_ON()

  index_map_.clear();
}

void IDBObjectStore::RevertMetadata(
    scoped_refptr<IDBObjectStoreMetadata> old_metadata) {
  DCHECK(transaction_->IsVersionChange());
  DCHECK(!transaction_->IsActive());
  DCHECK(old_metadata.get());
  DCHECK(Id() == old_metadata->id);

  for (auto& index : index_map_.Values()) {
    const int64_t index_id = index->Id();

    if (index->IsNewlyCreated(*old_metadata)) {
      // The index was created by this transaction. According to the spec,
      // its metadata will remain as-is.
      DCHECK(!old_metadata->indexes.Contains(index_id));
      index->MarkDeleted();
      continue;
    }

    // The index was created in a previous transaction. We need to revert
    // its metadata. The index might have been deleted, so we
    // unconditionally reset the deletion marker.
    DCHECK(old_metadata->indexes.Contains(index_id));
    scoped_refptr<IDBIndexMetadata> old_index_metadata =
        old_metadata->indexes.at(index_id);
    index->RevertMetadata(std::move(old_index_metadata));
  }
  metadata_ = std::move(old_metadata);

  // An object store's metadata will only get reverted if the index was in the
  // database when the versionchange transaction started.
  deleted_ = false;
}

void IDBObjectStore::RevertDeletedIndexMetadata(IDBIndex& deleted_index) {
  DCHECK(transaction_->IsVersionChange());
  DCHECK(!transaction_->IsActive());
  DCHECK(deleted_index.objectStore() == this);
  DCHECK(deleted_index.IsDeleted());

  const int64_t index_id = deleted_index.Id();
  DCHECK(metadata_->indexes.Contains(index_id))
      << "The object store's metadata was not correctly reverted";
  scoped_refptr<IDBIndexMetadata> old_index_metadata =
      metadata_->indexes.at(index_id);
  deleted_index.RevertMetadata(std::move(old_index_metadata));
}

void IDBObjectStore::RenameIndex(int64_t index_id, const String& new_name) {
  DCHECK(transaction_->IsVersionChange());
  DCHECK(transaction_->IsActive());

  BackendDB()->RenameIndex(transaction_->Id(), Id(), index_id, new_name);

  auto metadata_iterator = metadata_->indexes.find(index_id);
  DCHECK_NE(metadata_iterator, metadata_->indexes.end()) << "Invalid index_id";
  const String& old_name = metadata_iterator->value->name;

  DCHECK(index_map_.Contains(old_name))
      << "The index had to be accessed in order to be renamed.";
  DCHECK(!index_map_.Contains(new_name));
  index_map_.Set(new_name, index_map_.Take(old_name));

  metadata_iterator->value->name = new_name;
}

int64_t IDBObjectStore::FindIndexId(const String& name) const {
  for (const auto& it : Metadata().indexes) {
    if (it.value->name == name) {
      DCHECK_NE(it.key, IDBIndexMetadata::kInvalidId);
      return it.key;
    }
  }
  return IDBIndexMetadata::kInvalidId;
}

WebIDBDatabase* IDBObjectStore::BackendDB() const {
  return transaction_->BackendDB();
}

}  // namespace blink
