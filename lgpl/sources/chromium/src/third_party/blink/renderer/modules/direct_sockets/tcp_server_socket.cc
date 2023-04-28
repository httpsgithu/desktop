// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_server_socket.h"

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_tcp_server_socket_open_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_tcp_server_socket_options.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/direct_sockets/tcp_server_readable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

namespace {

mojom::blink::DirectTCPServerSocketOptionsPtr CreateTCPServerSocketOptions(
    const String& local_address,
    const TCPServerSocketOptions* options,
    ExceptionState& exception_state) {
  auto socket_options = mojom::blink::DirectTCPServerSocketOptions::New();

  net::IPAddress address;
  if (!address.AssignFromIPLiteral(local_address.Utf8())) {
    exception_state.ThrowTypeError("localAddress must be a valid IP address.");
    return {};
  }

  if (options->hasLocalPort() && options->localPort() == 0) {
    exception_state.ThrowTypeError(
        "localPort must be greater than zero. Leave this field unassigned to "
        "allow the OS to pick a port on its own.");
    return {};
  }

  // Port 0 allows the OS to pick an available port on its own.
  net::IPEndPoint local_addr = net::IPEndPoint(
      std::move(address), options->hasLocalPort() ? options->localPort() : 0U);

  if (options->hasBacklog()) {
    if (options->backlog() == 0) {
      exception_state.ThrowTypeError("backlog must be greater than zero.");
      return {};
    }
    socket_options->backlog = options->backlog();
  }

  if (options->hasIpv6Only()) {
    if (local_addr.address() != net::IPAddress::IPv6AllZeros()) {
      exception_state.ThrowTypeError(
          "ipv6Only can only be specified when localAddress is [::] or "
          "equivalent.");
      return {};
    }
    // TODO(crbug.com/1413161): Implement ipv6_only support.
  }

  socket_options->local_addr = std::move(local_addr);
  return socket_options;
}

}  // namespace

TCPServerSocket::TCPServerSocket(ScriptState* script_state)
    : Socket(script_state) {}

TCPServerSocket::~TCPServerSocket() = default;

// static
TCPServerSocket* TCPServerSocket::Create(ScriptState* script_state,
                                         const String& local_address,
                                         const TCPServerSocketOptions* options,
                                         ExceptionState& exception_state) {
  if (!Socket::CheckContextAndPermissions(script_state, exception_state)) {
    return nullptr;
  }

  auto* socket = MakeGarbageCollected<TCPServerSocket>(script_state);
  if (!socket->Open(local_address, options, exception_state)) {
    return nullptr;
  }
  return socket;
}

ScriptPromise TCPServerSocket::close(ScriptState*, ExceptionState&) {
  NOTIMPLEMENTED();
  return closed(GetScriptState());
}

bool TCPServerSocket::Open(const String& local_addr,
                           const TCPServerSocketOptions* options,
                           ExceptionState& exception_state) {
  auto open_tcp_server_socket_options =
      CreateTCPServerSocketOptions(local_addr, options, exception_state);

  if (exception_state.HadException()) {
    return false;
  }

  mojo::PendingRemote<network::mojom::blink::TCPServerSocket> tcp_server_remote;
  mojo::PendingReceiver<network::mojom::blink::TCPServerSocket>
      tcp_server_receiver = tcp_server_remote.InitWithNewPipeAndPassReceiver();

  GetServiceRemote()->OpenTCPServerSocket(
      std::move(open_tcp_server_socket_options), std::move(tcp_server_receiver),
      WTF::BindOnce(&TCPServerSocket::OnTCPServerSocketOpened,
                    WrapPersistent(this), std::move(tcp_server_remote)));
  return true;
}

void TCPServerSocket::OnTCPServerSocketOpened(
    mojo::PendingRemote<network::mojom::blink::TCPServerSocket>
        tcp_server_remote,
    int32_t result,
    const absl::optional<net::IPEndPoint>& local_addr) {
  if (result == net::OK) {
    DCHECK(local_addr);
    readable_stream_wrapper_ =
        MakeGarbageCollected<TCPServerReadableStreamWrapper>(
            GetScriptState(), base::DoNothing(), std::move(tcp_server_remote));

    auto* open_info = TCPServerSocketOpenInfo::Create();
    open_info->setReadable(readable_stream_wrapper_->Readable());
    open_info->setLocalAddress(String{local_addr->ToStringWithoutPort()});
    open_info->setLocalPort(local_addr->port());

    GetOpenedPromiseResolver()->Resolve(open_info);

    SetState(State::kOpen);
  } else {
    NOTIMPLEMENTED();
  }

  DCHECK_NE(GetState(), State::kOpening);
}

void TCPServerSocket::Trace(Visitor* visitor) const {
  visitor->Trace(readable_stream_wrapper_);

  ScriptWrappable::Trace(visitor);
  Socket::Trace(visitor);
}

}  // namespace blink
