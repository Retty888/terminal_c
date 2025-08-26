#include "cpr_http_client.h"
#include <cpr/cpr.h>
#include <exception>

namespace Core {

HttpResponse CprHttpClient::get(const std::string &url,
                                std::chrono::milliseconds timeout,
                                const std::map<std::string, std::string> &headers) {
  HttpResponse resp;
  try {
    cpr::Timeout to{static_cast<int32_t>(timeout.count())};
    cpr::Header hdr{headers.begin(), headers.end()};
    auto r = cpr::Get(cpr::Url{url}, to, hdr);
    resp.status_code = static_cast<int>(r.status_code);
    resp.text = std::move(r.text);
    if (r.error.code != cpr::ErrorCode::OK) {
      resp.network_error = true;
      resp.error_message = r.error.message;
    }
  } catch (const std::exception &e) {
    resp.network_error = true;
    resp.error_message = e.what();
  }
  return resp;
}

} // namespace Core

