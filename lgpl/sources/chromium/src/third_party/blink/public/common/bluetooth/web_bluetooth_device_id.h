// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_BLUETOOTH_WEB_BLUETOOTH_DEVICE_ID_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_BLUETOOTH_WEB_BLUETOOTH_DEVICE_ID_H_

#include <stdint.h>

#include <array>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>

#include "third_party/blink/public/common/common_export.h"

namespace blink {

using WebBluetoothDeviceIdKey = std::array<uint8_t, 16>;

// Used to uniquely identify a Bluetooth Device for an Origin.
// A WebBluetoothDeviceId represents a 128bit key for bluetooth device id.
class BLINK_COMMON_EXPORT WebBluetoothDeviceId {
 public:
  // Default constructor that creates an invalid id. We implement it so that
  // instances of this class in a container, e.g. std::unordered_map, can be
  // accessed through the [] operator. Trying to call any function of the
  // resulting object will DCHECK-fail.
  WebBluetoothDeviceId();

  // CHECKS that |device_id| is valid.
  explicit WebBluetoothDeviceId(const WebBluetoothDeviceIdKey& device_id);

  // CHECKS that |encoded_device_id| is a valid base64-encoded string.
  explicit WebBluetoothDeviceId(const std::string& encoded_device_id);

  // Copyable.
  WebBluetoothDeviceId(const WebBluetoothDeviceId& other) = default;
  WebBluetoothDeviceId& operator=(const WebBluetoothDeviceId& other) = default;

  // Moveable.
  WebBluetoothDeviceId(WebBluetoothDeviceId&& other) = default;
  WebBluetoothDeviceId& operator=(WebBluetoothDeviceId&& other) = default;

  ~WebBluetoothDeviceId();

  // Returns the base64 encoded string of `device_id_`.
  std::string DeviceIdInBase64() const;

  // Returns the serialization of the object.
  std::string str() const;

  // `device_id_` getter.
  const WebBluetoothDeviceIdKey& DeviceId() const;

  // The returned WebBluetoothDeviceId is generated by creating a random 128bit
  // binary key.
  static WebBluetoothDeviceId Create();

  // This method will return true. if |encoded_device_id| results in a 128bit
  // base64-encoding string. Otherwise returns false.
  static bool IsValid(const std::string& encoded_device_id);

  bool IsValid() const;

  bool operator==(const WebBluetoothDeviceId& device_id) const;
  bool operator!=(const WebBluetoothDeviceId& device_id) const;
  bool operator<(const WebBluetoothDeviceId& device_id) const;

 private:
  WebBluetoothDeviceIdKey device_id_;
  bool is_initialized_ = false;
};

// This is required by gtest to print a readable output on test failures.
BLINK_COMMON_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const WebBluetoothDeviceId& device_id);

struct WebBluetoothDeviceIdHash {
  size_t operator()(const WebBluetoothDeviceId& device_id) const {
    return std::hash<std::string>()(device_id.str());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_BLUETOOTH_WEB_BLUETOOTH_DEVICE_ID_H_
