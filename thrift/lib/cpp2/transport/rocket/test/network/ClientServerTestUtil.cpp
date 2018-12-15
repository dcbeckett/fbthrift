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

#include <thrift/lib/cpp2/transport/rocket/test/network/ClientServerTestUtil.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <utility>

#include <gtest/gtest.h>

#include <folly/Conv.h>
#include <folly/Optional.h>
#include <folly/SocketAddress.h>
#include <folly/fibers/Baton.h>
#include <folly/fibers/FiberManager.h>
#include <folly/fibers/FiberManagerMap.h>
#include <folly/futures/helpers.h>
#include <folly/io/IOBuf.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/EventBase.h>

#include <rsocket/RSocket.h>
#include <rsocket/transports/tcp/TcpConnectionAcceptor.h>
#include <wangle/acceptor/Acceptor.h>
#include <wangle/acceptor/ServerSocketConfig.h>
#include <yarpl/Flowable.h>
#include <yarpl/Single.h>

#include <thrift/lib/cpp2/async/Stream.h>
#include <thrift/lib/cpp2/transport/rocket/Types.h>
#include <thrift/lib/cpp2/transport/rocket/client/RocketClient.h>
#include <thrift/lib/cpp2/transport/rocket/client/RocketStreamImpl.h>
#include <thrift/lib/cpp2/transport/rocket/server/RocketServerConnection.h>
#include <thrift/lib/cpp2/transport/rocket/server/RocketServerFrameContext.h>
#include <thrift/lib/cpp2/transport/rocket/server/RocketServerHandler.h>
#include <thrift/lib/cpp2/transport/rocket/server/RocketServerStreamSubscriber.h>

using namespace rsocket;
using namespace yarpl::flowable;
using namespace yarpl::single;

namespace apache {
namespace thrift {
namespace rocket {
namespace test {

namespace {
class RsocketTestServerResponder : public rsocket::RSocketResponder {
 public:
  std::shared_ptr<Single<rsocket::Payload>> handleRequestResponse(
      rsocket::Payload request,
      uint32_t) final;

  std::shared_ptr<Flowable<rsocket::Payload>> handleRequestStream(
      rsocket::Payload request,
      uint32_t) final;
};

std::pair<std::unique_ptr<folly::IOBuf>, std::unique_ptr<folly::IOBuf>>
makeTestResponse(
    std::unique_ptr<folly::IOBuf> requestMetadata,
    std::unique_ptr<folly::IOBuf> requestData) {
  std::pair<std::unique_ptr<folly::IOBuf>, std::unique_ptr<folly::IOBuf>>
      response;

  folly::StringPiece data(requestData->coalesce());
  constexpr folly::StringPiece kMetadataEchoPrefix{"metadata_echo:"};
  constexpr folly::StringPiece kDataEchoPrefix{"data_echo:"};

  folly::Optional<rsocket::Payload> responsePayload;
  if (data.removePrefix("sleep_ms:")) {
    // Sleep, then echo back request.
    std::chrono::milliseconds sleepFor(folly::to<uint32_t>(data));
    std::this_thread::sleep_for(sleepFor); // sleep override
  } else if (data.removePrefix("error:")) {
    // Reply with a specific kind of error.
  } else if (data.startsWith(kMetadataEchoPrefix)) {
    // Reply with echoed metadata in the response payload.
    auto responseMetadata = requestData->clone();
    responseMetadata->trimStart(kMetadataEchoPrefix.size());
    response =
        std::make_pair(std::move(responseMetadata), std::move(requestData));
  } else if (data.startsWith(kDataEchoPrefix)) {
    // Reply with echoed data in the response payload.
    auto responseData = requestData->clone();
    responseData->trimStart(kDataEchoPrefix.size());
    response =
        std::make_pair(std::move(requestMetadata), std::move(responseData));
  }

  // If response payload is not set at this point, simply echo back what client
  // sent.
  if (!response.first && !response.second) {
    response =
        std::make_pair(std::move(requestMetadata), std::move(requestData));
  }

  return response;
}

template <class P>
P makePayload(folly::StringPiece metadata, folly::StringPiece data);

template <>
rsocket::Payload makePayload<rsocket::Payload>(
    folly::StringPiece metadata,
    folly::StringPiece data) {
  return rsocket::Payload(data, metadata);
}

template <>
apache::thrift::rocket::Payload makePayload<apache::thrift::rocket::Payload>(
    folly::StringPiece metadata,
    folly::StringPiece data) {
  return apache::thrift::rocket::Payload::makeFromMetadataAndData(
      metadata, data);
}

template <class P>
std::shared_ptr<yarpl::flowable::Flowable<P>> makeTestFlowable(
    folly::StringPiece data) {
  size_t n = 500;
  if (data.removePrefix("generate:")) {
    n = folly::to<size_t>(data);
  }

  auto gen = [n, i = static_cast<size_t>(0)](
                 auto& subscriber, int64_t requested) mutable {
    while (requested-- > 0 && i < n) {
      subscriber.onNext(makePayload<P>(
          folly::to<std::string>("metadata:", i), folly::to<std::string>(i)));
      ++i;
    }
    if (i == n) {
      subscriber.onComplete();
    }
  };
  return yarpl::flowable::Flowable<P>::create(std::move(gen));
}

std::shared_ptr<Single<rsocket::Payload>>
RsocketTestServerResponder::handleRequestResponse(
    rsocket::Payload request,
    uint32_t /* streamId */) {
  DCHECK(request.data);
  auto data = folly::StringPiece(request.data->coalesce());

  if (data.removePrefix("error:application")) {
    return Single<rsocket::Payload>::create([](auto&& subscriber) mutable {
      subscriber->onSubscribe(SingleSubscriptions::empty());
      subscriber->onError(folly::make_exception_wrapper<std::runtime_error>(
          "Application error occurred"));
    });
  }

  auto response =
      makeTestResponse(std::move(request.metadata), std::move(request.data));
  rsocket::Payload responsePayload(
      std::move(response.second), std::move(response.first));

  return Single<rsocket::Payload>::create(
      [responsePayload =
           std::move(responsePayload)](auto&& subscriber) mutable {
        subscriber->onSubscribe(SingleSubscriptions::empty());
        subscriber->onSuccess(std::move(responsePayload));
      });
}

std::shared_ptr<Flowable<rsocket::Payload>>
RsocketTestServerResponder::handleRequestStream(
    rsocket::Payload request,
    uint32_t /* streamId */) {
  DCHECK(request.data);
  auto data = folly::StringPiece(request.data->coalesce());

  if (data.removePrefix("error:application")) {
    return Flowable<rsocket::Payload>::create(
        [](Subscriber<rsocket::Payload>& subscriber,
           int64_t /* requested */) mutable {
          subscriber.onError(folly::make_exception_wrapper<std::runtime_error>(
              "Application error occurred"));
        });
  }
  return makeTestFlowable<rsocket::Payload>(data);
}
} // namespace

RsocketTestServer::RsocketTestServer() {
  TcpConnectionAcceptor::Options opts;
  opts.address = folly::SocketAddress("::1", 0 /* bind to any port */);
  opts.threads = 2;

  rsocketServer_ = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));
  // Start accepting connections
  rsocketServer_->start([](const rsocket::SetupParameters&) {
    return std::make_shared<RsocketTestServerResponder>();
  });
}

RsocketTestServer::~RsocketTestServer() {
  shutdown();
}

uint16_t RsocketTestServer::getListeningPort() const {
  auto oport = rsocketServer_->listeningPort();
  DCHECK(oport);
  return *oport;
}

void RsocketTestServer::shutdown() {
  rsocketServer_.reset();
}

RocketTestClient::RocketTestClient(const folly::SocketAddress& serverAddr)
    : evb_(*evbThread_.getEventBase()),
      fm_(folly::fibers::getFiberManager(evb_)) {
  evb_.runInEventBaseThread([this, serverAddr] {
    folly::AsyncSocket::UniquePtr socket(
        new folly::AsyncSocket(&evb_, serverAddr));
    client_ = RocketClient::create(evb_, std::move(socket));
  });
}

RocketTestClient::~RocketTestClient() {
  evb_.runInEventBaseThreadAndWait([this] { client_.reset(); });
}

folly::Try<Payload> RocketTestClient::sendRequestResponseSync(
    Payload request,
    std::chrono::milliseconds timeout) {
  folly::Try<Payload> response;
  folly::fibers::Baton baton;

  evb_.runInEventBaseThread([&] {
    fm_.addTaskFinally(
        [&] {
          return client_->sendRequestResponseSync(std::move(request), timeout);
        },
        [&](folly::Try<Payload>&& r) {
          response = std::move(r);
          baton.post();
        });
  });

  baton.wait();
  return response;
}

folly::Try<void> RocketTestClient::sendRequestFnfSync(Payload request) {
  folly::Try<void> response;
  folly::fibers::Baton baton;

  evb_.runInEventBaseThread([&] {
    fm_.addTaskFinally(
        [&] { return client_->sendRequestFnfSync(std::move(request)); },
        [&](folly::Try<void>&& r) {
          response = std::move(r);
          baton.post();
        });
  });

  baton.wait();
  return response;
}

folly::Try<SemiStream<Payload>> RocketTestClient::sendRequestStreamSync(
    Payload request) {
  folly::Try<SemiStream<Payload>> stream;

  evb_.runInEventBaseThreadAndWait([&] {
    stream = folly::makeTryWith([&] {
      return SemiStream<Payload>(Stream<Payload>::create(
          std::make_unique<apache::thrift::detail::RocketStreamImpl>(
              client_->createStream(std::move(request))),
          &evb_));
    });
  });

  return stream;
}

namespace {
class RocketTestServerAcceptor final : public wangle::Acceptor {
 public:
  explicit RocketTestServerAcceptor(
      std::shared_ptr<RocketServerHandler> frameHandler)
      : Acceptor(wangle::ServerSocketConfig{}),
        frameHandler_(std::move(frameHandler)) {}

  void onNewConnection(
      folly::AsyncTransportWrapper::UniquePtr socket,
      const folly::SocketAddress*,
      const std::string&,
      wangle::SecureTransportType,
      const wangle::TransportInfo&) override {
    auto* connection =
        new RocketServerConnection(std::move(socket), frameHandler_);
    getConnectionManager()->addConnection(connection);
  }

 private:
  const std::shared_ptr<RocketServerHandler> frameHandler_;
};

class RocketTestServerHandler : public RocketServerHandler {
 public:
  void handleSetupFrame(SetupFrame&&, RocketServerFrameContext&&) final {}

  void handleRequestResponseFrame(
      RequestResponseFrame&& frame,
      RocketServerFrameContext&& context) final {
    auto payload = std::move(frame.payload());
    folly::StringPiece dataPiece(payload.data()->coalesce());

    if (dataPiece.removePrefix("error:application")) {
      return context.sendError(RocketException(
          ErrorCode::APPLICATION_ERROR, "Application error occurred"));
    }

    auto md = std::move(payload).metadata();
    auto data = std::move(payload).data();
    auto response = makeTestResponse(std::move(md), std::move(data));
    auto responsePayload = Payload::makeFromMetadataAndData(
        std::move(response.first), std::move(response.second));
    return context.sendPayload(
        std::move(responsePayload), Flags::none().next(true).complete(true));
  }

  void handleRequestFnfFrame(RequestFnfFrame&&, RocketServerFrameContext&&)
      final {}

  void handleRequestStreamFrame(
      RequestStreamFrame&& frame,
      std::shared_ptr<RocketServerStreamSubscriber> subscriber) final {
    auto payload = std::move(frame.payload());
    folly::StringPiece dataPiece(payload.data()->coalesce());

    if (dataPiece.removePrefix("error:application")) {
      return Flowable<Payload>::error(
                 folly::make_exception_wrapper<RocketException>(
                     ErrorCode::APPLICATION_ERROR,
                     "Application error occurred"))
          ->subscribe(std::move(subscriber));
    }
    makeTestFlowable<rocket::Payload>(dataPiece)->subscribe(
        std::move(subscriber));
  }
};
} // namespace

RocketTestServer::RocketTestServer()
    : evb_(*ioThread_.getEventBase()),
      listeningSocket_(new folly::AsyncServerSocket(&evb_)),
      acceptor_(std::make_unique<RocketTestServerAcceptor>(
          std::make_shared<RocketTestServerHandler>())) {
  start();
}

RocketTestServer::~RocketTestServer() {
  stop();
}

void RocketTestServer::start() {
  folly::via(
      &evb_,
      [this] {
        acceptor_->init(listeningSocket_.get(), &evb_);
        listeningSocket_->bind(0 /* bind to any port */);
        listeningSocket_->listen(128 /* tcpBacklog */);
        listeningSocket_->startAccepting();
      })
      .wait();
}

void RocketTestServer::stop() {
  folly::via(&evb_, [this] { listeningSocket_.reset(); }).wait();
  // Ensure that asynchronous shutdown work enqueued by ~AsyncServerSocket()
  // has a chance to run before acceptor_ is reset.
  folly::via(&evb_, [] {}).wait();
  folly::via(&evb_, [this] { acceptor_.reset(); }).wait();
}

uint16_t RocketTestServer::getListeningPort() const {
  return listeningSocket_->getAddress().getPort();
}

} // namespace test
} // namespace rocket
} // namespace thrift
} // namespace apache
