#include "cpr_http_client.h"
#include <cpr/cpr.h>

namespace Core {

HttpResponse CprHttpClient::get(const std::string &url) {
  cpr::Response r = cpr::Get(cpr::Url{url});
  HttpResponse resp;
  resp.status_code = r.status_code;
  resp.text = r.text;
  resp.network_error = r.error.code != cpr::ErrorCode::OK;
  resp.error_message = r.error.message;
  return resp;
}

} // namespace Core

