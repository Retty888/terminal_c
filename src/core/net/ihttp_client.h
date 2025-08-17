#pragma once

#include <string>

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
  virtual HttpResponse get(const std::string &url) = 0;
};

} // namespace Core

