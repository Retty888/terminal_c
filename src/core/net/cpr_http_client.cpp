#include "cpr_http_client.h"

namespace Core {

HttpResponse CprHttpClient::get(const std::string &url) {
  // Network functionality disabled in minimal build.
  HttpResponse resp;
  resp.status_code = 0;
  resp.network_error = true;
  resp.error_message = "cpr unavailable";
  return resp;
}

} // namespace Core

