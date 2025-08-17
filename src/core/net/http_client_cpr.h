#pragma once

#include "i_http_client.h"

namespace Core::Net {
class CprHttpClient : public IHttpClient {
public:
  HttpResponse get(const std::string &url) override;
};
}
