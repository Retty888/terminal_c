#include "cpr_http_client.h"
#include <cpr/cpr.h>
#include <exception>

namespace Core {

HttpResponse CprHttpClient::get(const std::string &url) {
  HttpResponse resp;
  try {
    auto r = cpr::Get(cpr::Url{url});
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

