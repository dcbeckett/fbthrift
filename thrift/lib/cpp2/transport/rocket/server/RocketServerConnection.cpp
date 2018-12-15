/*
 * Copyright 2018-present Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <thrift/lib/cpp2/transport/rocket/server/RocketServerConnection.h>

#include <memory>
#include <utility>

#include <folly/ExceptionWrapper.h>
#include <folly/Format.h>
#include <folly/Likely.h>
#include <folly/MapUtil.h>
#include <folly/ScopeGuard.h>
#include <folly/io/Cursor.h>
#include <folly/io/IOBuf.h>
#include <folly/io/async/AsyncTransport.h>
#include <folly/io/async/DelayedDestruction.h>

#include <wangle/acceptor/ConnectionManager.h>

#include <thrift/lib/cpp2/transport/rocket/RocketException.h>
#include <thrift/lib/cpp2/transport/rocket/framing/Frames.h>
#include <thrift/lib/cpp2/transport/rocket/framing/Util.h>
#include <thrift/lib/cpp2/transport/rocket/server/RocketServerFrameContext.h>
#include <thrift/lib/cpp2/transport/rocket/server/RocketServerHandler.h>
#include <thrift/lib/cpp2/transport/rocket/server/RocketServerStreamSubscriber.h>

namespace apache {
namespace thrift {
namespace rocket {

RocketServerConnection::RocketServerConnection(
    folly::AsyncTransportWrapper::UniquePtr socket,
    std::shared_ptr<RocketServerHandler> frameHandler)
    : evb_(*socket->getEventBase()),
      socket_(std::move(socket)),
      frameHandler_(std::move(frameHandler)) {
  CHECK(socket_);
  CHECK(frameHandler_);
  socket_->setReadCB(&parser_);
}

std::shared_ptr<RocketServerStreamSubscriber>
RocketServerConnection::createStreamSubscriber(
    RocketServerFrameContext&& context,
    uint32_t initialRequestN) {
  const auto streamId = context.streamId();
  auto& connection = context.connection();
  auto subscriber = std::make_shared<RocketServerStreamSubscriber>(
      std::move(context), initialRequestN);
  connection.streams_.emplace(streamId, subscriber);
  return subscriber;
}

void RocketServerConnection::send(std::unique_ptr<folly::IOBuf> data) {
  evb_.dcheckIsInEventBaseThread();

  if (state_ != ConnectionState::ALIVE) {
    return;
  }

  batchWriteLoopCallback_.enqueueWrite(std::move(data));
  if (!batchWriteLoopCallback_.isLoopCallbackScheduled()) {
    evb_.runInLoop(&batchWriteLoopCallback_, true /* thisIteration */);
  }
}

RocketServerConnection::~RocketServerConnection() {
  DCHECK(inflight_ == 0);
  DCHECK(batchWriteLoopCallback_.empty());
}

void RocketServerConnection::closeIfNeeded() {
  if (state_ != ConnectionState::CLOSING || inflight_ != streams_.size()) {
    return;
  }

  for (auto it = streams_.begin(); it != streams_.end();) {
    auto& subscriber = *it->second;
    subscriber.cancel();
    it = streams_.erase(it);
  }

  if (batchWriteLoopCallback_.isLoopCallbackScheduled()) {
    batchWriteLoopCallback_.cancelLoopCallback();
    flushPendingWrites();
  }
  state_ = ConnectionState::CLOSED;
  if (auto* manager = getConnectionManager()) {
    manager->removeConnection(this);
  }
  socket_.reset();
  destroy();
}

void RocketServerConnection::handleFrame(std::unique_ptr<folly::IOBuf> frame) {
  DestructorGuard dg(this);

  // Entire payloads may be chained, but the parser ensures each fragment is
  // coalesced.
  DCHECK(!frame->isChained());
  folly::io::Cursor cursor(frame.get());

  const auto streamId = readStreamId(cursor);
  FrameType frameType;
  std::tie(frameType, std::ignore) = readFrameTypeAndFlags(cursor);

  if (UNLIKELY(!setupFrameReceived_)) {
    if (frameType != FrameType::SETUP) {
      return close(folly::make_exception_wrapper<RocketException>(
          ErrorCode::INVALID_SETUP, "First frame must be SETUP frame"));
    }
    setupFrameReceived_ = true;
  } else {
    if (UNLIKELY(frameType == FrameType::SETUP)) {
      return close(folly::make_exception_wrapper<RocketException>(
          ErrorCode::INVALID_SETUP, "More than one SETUP frame received"));
    }
  }

  switch (frameType) {
    case FrameType::SETUP: {
      RocketServerFrameContext frameContext(*this, streamId);
      return frameHandler_->handleSetupFrame(
          SetupFrame(std::move(frame)), std::move(frameContext));
    }

    case FrameType::REQUEST_RESPONSE: {
      RocketServerFrameContext frameContext(*this, streamId);
      return std::move(frameContext)
          .onRequestFrame(RequestResponseFrame(std::move(frame)));
    }

    case FrameType::REQUEST_FNF: {
      RocketServerFrameContext frameContext(*this, streamId);
      return std::move(frameContext)
          .onRequestFrame(RequestFnfFrame(std::move(frame)));
    }

    case FrameType::REQUEST_STREAM: {
      RocketServerFrameContext frameContext(*this, streamId);
      return std::move(frameContext)
          .onRequestFrame(RequestStreamFrame(std::move(frame)));
    }

    case FrameType::REQUEST_N: {
      RequestNFrame requestNFrame(std::move(frame));
      if (auto* stream = folly::get_ptr(streams_, requestNFrame.streamId())) {
        (*stream)->request(requestNFrame.requestN());
      }
      return;
    }

    case FrameType::CANCEL: {
      auto streamIt = streams_.find(streamId);
      if (streamIt != streams_.end()) {
        streamIt->second->cancel();
        streams_.erase(streamIt);
      }
      return;
    }

    case FrameType::PAYLOAD: {
      auto it = partialFrames_.find(streamId);
      if (it == partialFrames_.end()) {
        return close(folly::make_exception_wrapper<RocketException>(
            ErrorCode::INVALID,
            folly::sformat(
                "Unexpected PAYLOAD frame received on stream {}",
                static_cast<uint32_t>(streamId))));
      }

      auto& frameContext = it->second;
      PayloadFrame payloadFrame(std::move(frame));

      const bool hasFollows = payloadFrame.hasFollows();
      SCOPE_EXIT {
        if (!hasFollows) {
          partialFrames_.erase(streamId);
        }
      };

      // Note that frameContext is only moved out of if we're handling the
      // final fragment.
      return std::move(frameContext).onPayloadFrame(std::move(payloadFrame));
    }

    default:
      close(folly::make_exception_wrapper<RocketException>(
          ErrorCode::INVALID,
          folly::sformat(
              "Received unhandleable frame type ({})",
              static_cast<uint8_t>(frameType))));
  }
}

void RocketServerConnection::close(folly::exception_wrapper ew) {
  DestructorGuard dg(this);

  // Immediately stop processing new requests
  socket_->setReadCB(nullptr);

  auto rex = ew
      ? RocketException(ErrorCode::CONNECTION_ERROR, ew.what())
      : RocketException(ErrorCode::CONNECTION_CLOSE, "Closing connection");
  RocketServerFrameContext(*this, StreamId{0}).sendError(std::move(rex));

  state_ = ConnectionState::CLOSING;
  closeIfNeeded();
}

void RocketServerConnection::timeoutExpired() noexcept {
  DestructorGuard dg(this);

  if (!isBusy()) {
    closeWhenIdle();
  }
}

bool RocketServerConnection::isBusy() const {
  return inflight_ > 0 || batchWriteLoopCallback_.isLoopCallbackScheduled();
}

// On graceful shutdown, ConnectionManager will first fire the
// notifyPendingShutdown() callback for each connection. Then, after the drain
// period has elapsed, closeWhenIdle() will be called for each connection. Note
// that ConnectionManager waits for a connection to become un-busy before
// calling closeWhenIdle().
void RocketServerConnection::notifyPendingShutdown() {}

void RocketServerConnection::dropConnection() {
  close(folly::make_exception_wrapper<transport::TTransportException>(
      transport::TTransportException::TTransportExceptionType::INTERRUPTED,
      "Dropping connection"));
}

void RocketServerConnection::closeWhenIdle() {
  DCHECK(!isBusy());
  close(folly::make_exception_wrapper<transport::TTransportException>(
      transport::TTransportException::TTransportExceptionType::TIMED_OUT,
      "Closing idle connection"));
}

void RocketServerConnection::writeSuccess() noexcept {}

void RocketServerConnection::writeErr(
    size_t bytesWritten,
    const folly::AsyncSocketException& ex) noexcept {
  DestructorGuard dg(this);
  close(folly::make_exception_wrapper<std::runtime_error>(folly::sformat(
      "Failed to write to remote endpoint. Wrote {} bytes."
      " AsyncSocketException: {}",
      bytesWritten,
      ex.what())));
}

} // namespace rocket
} // namespace thrift
} // namespace apache
