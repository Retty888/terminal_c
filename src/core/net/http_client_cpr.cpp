#include "http_client_cpr.h"

#include <cpr/cpr.h>

namespace Core::Net {
HttpResponse CprHttpClient::get(const std::string &url) {
  cpr::Response r = cpr::Get(cpr::Url{url});
  HttpResponse res;
  res.status_code = r.status_code;
  res.text = r.text;
  if (r.error.code != cpr::ErrorCode::OK) {
    res.network_error = true;
    res.error_message = r.error.message;
  } else {
    res.error_message = r.error.message;
  }
  return res;
}
} // namespace Core::Net
