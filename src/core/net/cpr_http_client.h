#pragma once

#include "ihttp_client.h"

namespace Core {

class CprHttpClient : public IHttpClient {
public:
  HttpResponse get(const std::string &url) override;
};

} // namespace Core

