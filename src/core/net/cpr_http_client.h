#pragma once

#include "ihttp_client.h"

namespace Core {

class CprHttpClient : public IHttpClient {
public:
  HttpResponse get(const std::string &url,
                   std::chrono::milliseconds timeout,
                   const std::map<std::string, std::string> &headers) override;
  HttpResponse post(const std::string &url, const std::string &body,
                    std::chrono::milliseconds timeout,
                    const std::map<std::string, std::string> &headers) override;
};

} // namespace Core

