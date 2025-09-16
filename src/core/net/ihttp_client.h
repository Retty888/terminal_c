#pragma once

#include <string>
#include <chrono>
#include <map>

namespace Core {

struct HttpResponse {
  int status_code{0};
  std::string text;
  std::string error_message;
  bool network_error{false};
};

class IHttpClient {
public:
  virtual ~IHttpClient() = default;
  virtual HttpResponse get(const std::string &url,
                           std::chrono::milliseconds timeout,
                           const std::map<std::string, std::string> &headers) = 0;
  virtual HttpResponse post(const std::string &url, const std::string &body,
                            std::chrono::milliseconds timeout,
                            const std::map<std::string, std::string> &headers) = 0;
};

} // namespace Core

