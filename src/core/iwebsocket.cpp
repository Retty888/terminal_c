#include "iwebsocket.h"
#include "logger.h"

#if __has_include(<ixwebsocket/IXWebSocket.h>)
#define HAS_IXWEBSOCKET 1
#include <ixwebsocket/IXWebSocket.h>
#endif

namespace Core {

#ifdef HAS_IXWEBSOCKET
class IxWebSocket : public IWebSocket {
public:
  void setUrl(const std::string &url) override { ws_.setUrl(url); }
  void setOnMessage(MessageCallback cb) override { msg_cb_ = std::move(cb); }
  void setOnError(ErrorCallback cb) override { err_cb_ = std::move(cb); }
  void start() override {
    ws_.setOnMessage([this](const ix::WebSocketMessagePtr &msg) {
      if (msg->type == ix::WebSocketMessageType::Message) {
        if (msg_cb_) msg_cb_(msg->str);
      } else if (msg->type == ix::WebSocketMessageType::Error ||
                 msg->type == ix::WebSocketMessageType::Close) {
        if (err_cb_) err_cb_();
      }
    });
    ws_.start();
  }
  void stop() override { ws_.stop(); }

private:
  ix::WebSocket ws_;
  MessageCallback msg_cb_;
  ErrorCallback err_cb_;
};
#endif

WebSocketFactory default_websocket_factory() {
#ifdef HAS_IXWEBSOCKET
  return []() { return std::make_unique<IxWebSocket>(); };
#else
  return []() { return nullptr; };
#endif
}

} // namespace Core

