#pragma once

#include <functional>
#include <memory>
#include <string>

namespace Core {

class IWebSocket {
public:
  using MessageCallback = std::function<void(const std::string&)>;
  using ErrorCallback = std::function<void()>;
  using CloseCallback = std::function<void()>;
  using OpenCallback = std::function<void()>;
  virtual ~IWebSocket() = default;
  virtual void setUrl(const std::string &url) = 0;
  virtual void setOnMessage(MessageCallback cb) = 0;
  virtual void setOnError(ErrorCallback cb) = 0;
  virtual void setOnClose(CloseCallback cb) = 0;
  virtual void setOnOpen(OpenCallback cb) = 0;
  virtual void sendText(const std::string &text) = 0;
  virtual void start() = 0;
  virtual void stop() = 0;
};

using WebSocketFactory = std::function<std::unique_ptr<IWebSocket>()>;
WebSocketFactory default_websocket_factory();

} // namespace Core

