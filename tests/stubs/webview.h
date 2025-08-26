#pragma once
using webview_t = void*;
inline webview_t webview_create(int, void*) { return nullptr; }
inline int webview_destroy(webview_t) { return 0; }
inline int webview_run(webview_t) { return 0; }
inline int webview_terminate(webview_t) { return 0; }
inline int webview_dispatch(webview_t, void (*)(webview_t, void*), void*) { return 0; }
inline int webview_navigate(webview_t, const char*) { return 0; }
inline int webview_bind(webview_t, const char*, void (*)(webview_t, const char*, const char*, void*), void*) { return 0; }
inline int webview_return(webview_t, const char*, int, const char*) { return 0; }
inline int webview_eval(webview_t, const char*) { return 0; }
