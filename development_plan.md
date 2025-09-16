## Development Plan (Consolidated)

This plan merges the overall development roadmap with Hyperliquid integration and chart improvements. Each section lists scope, current status, issues, and next actions. Tags: [OK], [IN PROGRESS], [PENDING], [BLOCKER].

---

### 0) Goals & Principles

- Bold: fast, stable terminal for market data, analysis, and execution.
- Simple: minimal UI friction; predictable, testable behaviors.
- Safe: clear logs, resilient networking, and no silent failures.

---

### 1) Platform & Build [OK]

- Build system: CMake + vcpkg; VS 2022 generator; C++20.
- Artifacts: `TradingTerminal.exe`, unit tests under `tests/`.
- Status: builds in Debug successfully after recent fixes.
- Issues: none blocking.
- Next: keep presets in `CMakePresets.json` in sync with CI.

---

### 2) Core Data Layer [OK]

- Storage: `CandleManager` with CSV/JSON, append/clear, size, list.
- Fetch: `IDataProvider`, Binance provider implemented; HTTP via `cpr` with token-bucket limiter.
- New: `IDataProvider::fetch_range` added for backfill; wired in `DataService`.
- Issues: none blocking.
- Next: add unit tests for range merges and gap filling.

---

### 3) Hyperliquid Integration [IN PROGRESS]

- Scope: market data (Phase 1), trading API (Phase 2), strategies (Phase 3).
- Done: `HyperliquidDataProvider` with `fetch_klines` and `fetch_range`; provider registered; basic symbol normalization (strip `USDT`/`USD`).
- Pending: `fetch_all_symbols`, `fetch_intervals`; interval mapping validation; richer symbol normalization.
- Risks: API response schema variance; rate limits; symbol aliases.
- Next Actions:
  - Implement `fetch_all_symbols`/`fetch_intervals` (start with minimal lists if API lacks endpoints).
  - Finalize symbol/interval mapping helpers with tests and edge cases.
  - Add config options for default Hyperliquid coin list.

---

### 4) UI & Workflow [OK]

- Provider switcher: works (`DataService::register_provider`, selector in Control Panel).
- Pair management: load/reload/clear per interval; file size shown.
- Issues: none blocking after adding `DataService::remove_candles`.
- Next: persist last provider/pair/interval; quick actions for backfill.

---

### 5) Chart Improvements [IN PROGRESS]

- Theme: grayscale applied (HTML complete; native ImPlot candles grayscale).
- Time axis: HTML uses RU locale without seconds; native ImPlot still shows “unclear” labels.
- Tools: crosshair, basic drawing; positions overlay; markers.
- Issues [BLOCKER - Time Axis]:
  - ImPlot time flags not available in current version; need custom ticks to fully control labels.
  - Crosshair/axis should support UTC/local toggle; 24h clock; hide seconds for >1m intervals.
- Plan (Native ImPlot):
  - Add time tick generator (by interval): compute tick times across visible X range; call `ImPlot::SetupAxisTicks` with `dd.mm HH:MM` formatter.
  - Add settings: `chart_time_zone` (utc|local), `chart_locale`, `show_seconds` for sub-minute intervals only.
  - Extend grayscale to lines/areas and grids; add high-contrast preset.
- Plan (HTML Lightweight): disabled in build; assets moved to `docs/` as reference.

---

### 6) Analysis & Tools [PLANNED]

- Indicators: SMA/EMA, VWAP, Bollinger, RSI, MACD (separate pane), volume + MA.
- Drawing tools: trend, hline/vline, rectangle, fibo; edit/move/delete; snap; undo/redo.
- Persistence: drawings stored per pair+interval in JSON next to candles.
- Next: define minimal indicator set (SMA, EMA, Volume), then iterate.

---

### 7) Performance & Reliability [PLANNED]

- Decimation for dense plots; incremental updates; frame throttling.
- Diagnostics: FPS/CPU overlay; logging around fetch/retry; visible progress for loads.
- Next: add decimation in renderer; throttle UI updates consistently.

---

### 8) Configuration & Docs [PLANNED]

- Config: add chart-related settings; provider defaults; log level.
- Docs: README usage, CHANGELOG, FAQ, and troubleshooting.
- Next: document Hyperliquid setup and risks; add chart settings section.

---

### 9) Open Problems & Decisions

- Time Axis (Native) [BLOCKER]: unclear format persists.
  - Decision: implement custom tick generation + formatter; add UTC/local toggle.
- Hyperliquid Symbols [PENDING]: mapping robustness.
  - Decision: add mapping table and tests; allow overrides in config.
- Intervals Support [PENDING]: list of supported intervals differs per provider.
  - Decision: provider-specific `fetch_intervals`; UI should adapt.
- Rate Limiting [PENDING]: 1 rps may be conservative.
  - Decision: expose in config; adopt backoff strategy on errors.

---

### 10) Next Actions (Short List)

- Implement native time ticks + formatter; add UTC/local toggle and hide seconds beyond 1m.
- Implement `fetch_all_symbols`/`fetch_intervals` in Hyperliquid provider.
- Extend grayscale to all series/grids in native chart; add high-contrast preset.
- Persist last provider/pair/interval and expose quick backfill action.
- Add unit tests for symbol/interval mapping and time formatting.
- Disable Binance as data source (temporarily) and pivot UI flows to Hyperliquid only.
- Align UI + chart visuals with Hyperliquid design language (spacing, colors, typography).

---

- Disable Binance as data source (temporarily) and pivot UI flows to Hyperliquid only.
- Align UI + chart visuals with Hyperliquid design language (spacing, colors, typography).

### 11) Changelog

- 2025-09-13
  - Build: Clean Debug build passes; fixed provider logic; linked tests to providers.
  - Integration: added `fetch_range` end-to-end; Hyperliquid symbol normalization.
  - UI: added `clear_interval`, `get_file_size`, `reload_candles` helpers.
  - Chart: grayscale theme (HTML + native candles); HTML axis localized; native axis formatter planned; WebView disabled and resources moved to `docs/`.
  - Cleanup: updated `.gitignore` (logs, data, build dirs); moved logs to `logs/`; removed stray file `CON`.
  - Chart Axis: implemented native time tick generation and labels (dd.mm HH:MM, seconds for sub-minute) via `SetupAxisTicks`; added crosshair readout (time + price) overlay near top-right of plot.
  - Layout: introduced default docking layout (left Control Panel, center Chart, bottom tabbed Journal/Backtest/Analytics/Signals); manual window placement disabled when docking is on.
  - Drawing Tools: expanded native tools (Cross/Trend/HLine/VLine/Rect/Ruler/Fibo) с предпросмотром, выбором/перетаскиванием, удалением (Del), привязкой по времени к свечам (Snap), и кнопками Save/Load (JSON на пару+интервал). Троттлинг лайв‑апдейта 500мс. Улучшен монохромный стиль.
  - Build & Run: cleaned build dirs, configured Debug preset, rebuilt, and launched TradingTerminal for UI verification.
- 2025-09-13 (later)
  - Fresh Candles: enabled lightweight HTTP polling each frame (schedules aligned updates per interval); fixed stream provider mapping from active provider; recent candles now appear without manual refresh.
  - Persistence: merged config pairs with stored data so user-added pairs (e.g., ETH) persist across runs; saving pairs normalizes symbols for Hyperliquid.
  - Provider: default provider switched to Hyperliquid; task added to disable Binance source end-to-end.
 новый код для встраивания - 
 // ll_intraday_analyzer.hpp
// C++17, header-only. Без внешних зависимостей.
// Формат CSV: timestamp,open,high,low,close,volume
// timestamp — предпочтительно epoch миллисекунды. ISO с 'Z' тоже поддержан (UTC).

#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace llintraday {

struct Candle {
    int64_t ts_ms = 0; // UTC, миллисекунды от эпохи
    double open=0, high=0, low=0, close=0, volume=0;
};

struct Record {
    int64_t ll_time_ms = 0;
    double  ll_price = 0;
    std::optional<int64_t> prev_ph_time_ms;
    std::optional<double>  prev_ph_price;

    std::optional<int64_t> hh_time_ms;
    std::optional<int>     mins_ll_to_hh;

    std::optional<int64_t> ema200_cross_time_ms;
    std::optional<int>     mins_ll_to_ema200;

    std::optional<int64_t> retest_time_ms;
    std::optional<int>     mins_ll_to_retest;
};

struct SeriesStats {
    int count = 0;
    std::optional<double> median_min, p25_min, p75_min, mean_min, max_min;
};

struct Summary {
    SeriesStats mins_ll_to_hh;
    SeriesStats mins_ll_to_ema200;
    SeriesStats mins_ll_to_retest;
    // фактические параметры
    int left = 3, right = 3;
    int ema_fast = 50, ema_slow = 200;
    double retest_eps = 0.001;
    int lookahead_min = 720;
    size_t rows_used = 0;
};

struct Params {
    int left = 3;
    int right = 3;
    int ema_fast = 50;
    int ema_slow = 200;
    double retest_eps = 0.001;  // 0.1%
    int lookahead_min = 720;    // 12 часов
};

struct Result {
    std::vector<Record> records;
    Summary summary;
};

// -------------------- утилиты --------------------
inline bool _ieq(char a, char b){ return (a==b) || (std::tolower(a)==std::tolower(b)); }

inline std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> out;
    std::string cur; cur.reserve(line.size());
    bool in_quotes=false;
    for(size_t i=0;i<line.size();++i){
        char c=line[i];
        if(c=='"'){ in_quotes = !in_quotes; }
        else if(c==',' && !in_quotes){ out.emplace_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.emplace_back(cur);
    return out;
}

// Простейший парсер ISO8601 "YYYY-MM-DDTHH:MM:SS(.sss)Z" -> epoch ms
inline std::optional<int64_t> parse_iso_utc_ms(const std::string& s){
    // требуем 'Z' в конце
    if(s.empty() || (s.back()!='Z' && s.back()!='z')) return std::nullopt;
    // отделим дробные миллисекунды, если есть
    std::string main = s;
    std::string frac;
    auto pos_dot = s.find('.');
    if(pos_dot!=std::string::npos){
        auto pos_z = s.find_last_of("Zz");
        if(pos_z!=std::string::npos && pos_z>pos_dot) {
            main = s.substr(0,pos_dot) + "Z";
            frac = s.substr(pos_dot+1, pos_z-pos_dot-1);
        }
    }
    std::tm tm{}; tm.tm_isdst = 0;
    std::istringstream iss(main);
    // формат без миллисекунд
    // пример: 2024-01-01T12:34:56Z
    iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if(iss.fail()) return std::nullopt;

    // преобразуем как timegm (UTC). На Windows нет timegm — делаем вручную.
    // Используем алгоритм перевода даты в секунды от эпохи (UTC).
    auto y = tm.tm_year + 1900;
    auto m = tm.tm_mon + 1;
    auto d = tm.tm_mday;
    auto H = tm.tm_hour;
    auto M = tm.tm_min;
    auto S = tm.tm_sec;

    // преобразование по алгоритму "Days since epoch"
    auto is_leap = [](int Y){ return (Y%4==0 && Y%100!=0) || (Y%400==0); };
    static int mdays_norm[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

    int64_t days = 0;
    for(int year=1970; year<y; ++year){
        days += is_leap(year)?366:365;
    }
    for(int month=1; month<m; ++month){
        days += mdays_norm[month-1] + ((month==2 && is_leap(y))?1:0);
    }
    days += (d-1);

    int64_t secs = days*86400LL + H*3600 + M*60 + S;
    int64_t ms = secs*1000LL;

    // миллисекунды из frac
    if(!frac.empty()){
        // взять до 3 цифр
        int ms_part = 0;
        int base = 1;
        for(size_t i=0;i<frac.size() && i<3;i++){
            ms_part = ms_part*10 + (frac[i]>='0'&&frac[i]<='9' ? frac[i]-'0' : 0);
            base*=10;
        }
        // нормализуем до миллисекунд
        while(base<1000){ ms_part*=10; base*=10; }
        ms += ms_part;
    }
    return ms;
}

inline std::optional<int64_t> parse_timestamp_ms(const std::string& s){
    // 1) epoch миллисекунды/секунды
    bool numeric = !s.empty() && (std::isdigit(s[0]) || s[0]=='-' || s[0]=='+');
    if(numeric){
        try{
            long double v = std::stold(s);
            if(std::fabsl(v) > 1e12L) { // похоже на миллисекунды
                return static_cast<int64_t>(v);
            }
            // секунды -> мс
            return static_cast<int64_t>(v*1000.0L);
        }catch(...){}
    }
    // 2) ISO8601 с 'Z'
    return parse_iso_utc_ms(s);
}

inline bool read_csv(const std::string& path, std::vector<Candle>& out){
    std::ifstream f(path);
    if(!f.is_open()) return false;
    std::string header;
    if(!std::getline(f, header)) return false;
    // нормализуем заголовки
    auto H = split_csv_line(header);
    auto find_col = [&](std::initializer_list<const char*> names)->int{
        for(size_t i=0;i<H.size();++i){
            std::string hh = H[i];
            std::transform(hh.begin(), hh.end(), hh.begin(), ::tolower);
            for(const char* name: names){
                std::string nn = name; std::transform(nn.begin(), nn.end(), nn.begin(), ::tolower);
                if(hh==nn) return int(i);
            }
        }
        return -1;
    };
    int c_ts = find_col({"timestamp","time","date","ts"});
    int c_o  = find_col({"open"});
    int c_h  = find_col({"high"});
    int c_l  = find_col({"low"});
    int c_c  = find_col({"close"});
    int c_v  = find_col({"volume","vol"});
    if(c_ts<0 || c_o<0 || c_h<0 || c_l<0 || c_c<0){
        return false;
    }
    std::string line;
    out.clear(); out.reserve(1<<20);
    while(std::getline(f,line)){
        if(line.empty()) continue;
        auto cols = split_csv_line(line);
        if((int)cols.size() <= std::max({c_ts,c_o,c_h,c_l,c_c,c_v})) continue;
        auto tsOpt = parse_timestamp_ms(cols[c_ts]);
        if(!tsOpt) continue;
        auto to_d = [](const std::string& s)->std::optional<double>{
            try{ return std::stod(s); } catch(...){ return std::nullopt; }
        };
        auto o = to_d(cols[c_o]); auto h = to_d(cols[c_h]); auto l=to_d(cols[c_l]); auto c=to_d(cols[c_c]);
        if(!o||!h||!l||!c) continue;
        Candle cd;
        cd.ts_ms = *tsOpt;
        cd.open=*o; cd.high=*h; cd.low=*l; cd.close=*c;
        if(c_v>=0){
            auto v = to_d(cols[c_v]); cd.volume = v?*v:0.0;
        }
        out.push_back(cd);
    }
    // по времени
    std::sort(out.begin(), out.end(), [](const Candle& a, const Candle& b){ return a.ts_ms < b.ts_ms; });
    return !out.empty();
}

inline std::vector<double> ema(const std::vector<double>& src, int length){
    std::vector<double> out(src.size(), std::numeric_limits<double>::quiet_NaN());
    if(length<=1 || src.empty()) return src;
    double alpha = 2.0/(length+1.0);
    double m = src[0];
    out[0]=m;
    for(size_t i=1;i<src.size();++i){
        m = alpha*src[i] + (1.0-alpha)*m;
        out[i]=m;
    }
    return out;
}

inline void pivots(const std::vector<Candle>& v, int left, int right, std::vector<uint8_t>& is_ph, std::vector<uint8_t>& is_pl){
    size_t n=v.size();
    is_ph.assign(n,0); is_pl.assign(n,0);
    for(int i=left; i<(int)n-right; ++i){
        bool ph=true, pl=true;
        for(int k=0;k<=left;k++){
            if(!(v[i].high >= v[i-k].high)) ph=false;
            if(!(v[i].low  <= v[i-k].low )) pl=false;
        }
        for(int k=1;k<=right;k++){
            if(!(v[i].high > v[i+k].high)) ph=false;
            if(!(v[i].low  < v[i+k].low )) pl=false;
        }
        if(ph) is_ph[i]=1;
        if(pl) is_pl[i]=1;
    }
}

inline std::vector<int> indices_of(const std::vector<uint8_t>& mask){
    std::vector<int> idx; idx.reserve(mask.size()/4);
    for(size_t i=0;i<mask.size();++i) if(mask[i]) idx.push_back((int)i);
    return idx;
}

inline std::vector<int> lower_lows(const std::vector<int>& pl_idx, const std::vector<Candle>& v){
    std::vector<int> out; out.reserve(pl_idx.size()/2);
    double prev_low = std::numeric_limits<double>::quiet_NaN();
    bool has_prev=false;
    for(int i : pl_idx){
        double L = v[i].low;
        if(!has_prev){ prev_low = L; has_prev=true; continue; }
        if(L < prev_low) out.push_back(i);
        prev_low = L;
    }
    return out;
}

inline std::optional<int> first_hh_after_ll(int idx_ll, const std::vector<int>& ph_idx, const std::vector<Candle>& v, std::optional<int> last_ph_before_ll, int lookahead_last){
    if(!last_ph_before_ll) return std::nullopt;
    double ref_high = v[*last_ph_before_ll].high;
    for(int j : ph_idx){
        if(j <= idx_ll) continue;
        if(j > lookahead_last) break;
        if(v[j].high > ref_high) return j;
    }
    return std::nullopt;
}

inline std::optional<int> first_close_above_ema(int idx_ll, const std::vector<Candle>& v, const std::vector<double>& ema_vec, int lookahead_last){
    for(int i=idx_ll+1; i<=lookahead_last && i<(int)v.size(); ++i){
        if(v[i].close > ema_vec[i]) return i;
    }
    return std::nullopt;
}

inline std::optional<int> first_retest_after_ll(int idx_ll, const std::vector<Candle>& v, double eps, int lookahead_last){
    double ll_price = v[idx_ll].low;
    double thr = ll_price * (1.0 + eps);
    for(int i=idx_ll+1; i<=lookahead_last && i<(int)v.size(); ++i){
        if(v[i].low <= thr) return i;
    }
    return std::nullopt;
}

inline int mins_between_ms(int64_t a_ms, int64_t b_ms){
    long long diff = (b_ms - a_ms)/1000LL; // сек
    return (int)(diff/60LL);
}

inline SeriesStats make_stats(const std::vector<std::optional<int>>& data){
    SeriesStats st;
    std::vector<int> x; x.reserve(data.size());
    for(const auto& o: data) if(o) x.push_back(*o);
    st.count = (int)x.size();
    if(x.empty()) return st;
    std::sort(x.begin(), x.end());
    auto percentile = [&](double p)->double{
        if(x.empty()) return std::numeric_limits<double>::quiet_NaN();
        double idx = p*(x.size()-1);
        size_t i0 = (size_t)std::floor(idx);
        size_t i1 = (size_t)std::ceil(idx);
        double w = idx - i0;
        return (1.0-w)*x[i0] + w*x[i1];
    };
    double sum=0; for(int v: x) sum+=v;
    st.median_min = percentile(0.5);
    st.p25_min    = percentile(0.25);
    st.p75_min    = percentile(0.75);
    st.mean_min   = sum / x.size();
    st.max_min    = (double)x.back();
    return st;
}

inline Result analyze(const std::vector<Candle>& v, const Params& P){
    Result res;
    res.summary.left = P.left; res.summary.right = P.right;
    res.summary.ema_fast = P.ema_fast; res.summary.ema_slow = P.ema_slow;
    res.summary.retest_eps = P.retest_eps; res.summary.lookahead_min = P.lookahead_min;
    res.summary.rows_used = v.size();

    if(v.size() < (size_t)(P.left+P.right+2)) return res;

    // EMA
    std::vector<double> closes; closes.reserve(v.size());
    for(const auto& c: v) closes.push_back(c.close);
    auto ema_fast = llintraday::ema(closes, P.ema_fast);
    auto ema_slow = llintraday::ema(closes, P.ema_slow);

    // pivots
    std::vector<uint8_t> is_ph, is_pl;
    pivots(v, P.left, P.right, is_ph, is_pl);
    auto ph_idx = indices_of(is_ph);
    auto pl_idx = indices_of(is_pl);

    // LL
    auto ll_idx = lower_lows(pl_idx, v);

    res.records.reserve(ll_idx.size());
    std::vector<std::optional<int>> t_hh, t_ema, t_retest;
    t_hh.reserve(ll_idx.size()); t_ema.reserve(ll_idx.size()); t_retest.reserve(ll_idx.size());

    // карта pivot-high < idx
    for(int idx : ll_idx){
        // последний pivot-high до LL
        std::optional<int> last_ph_before_ll;
        {
            auto it = std::lower_bound(ph_idx.begin(), ph_idx.end(), idx);
            if(it!=ph_idx.begin()){
                --it;
                if(*it < idx) last_ph_before_ll = *it;
            }
        }
        // горизонт поиска
        // P.lookahead_min минут после LL
        int lookahead_last = idx + P.lookahead_min; // 1 свеча = 1 минута
        if(lookahead_last >= (int)v.size()) lookahead_last = (int)v.size()-1;

        auto hh_i    = first_hh_after_ll(idx, ph_idx, v, last_ph_before_ll, lookahead_last);
        auto ema_i   = first_close_above_ema(idx, v, ema_slow, lookahead_last);
        auto ret_i   = first_retest_after_ll(idx, v, P.retest_eps, lookahead_last);

        Record rec;
        rec.ll_time_ms = v[idx].ts_ms;
        rec.ll_price   = v[idx].low;
        if(last_ph_before_ll){
            rec.prev_ph_time_ms = v[*last_ph_before_ll].ts_ms;
            rec.prev_ph_price   = v[*last_ph_before_ll].high;
        }

        if(hh_i){
            rec.hh_time_ms = v[*hh_i].ts_ms;
            rec.mins_ll_to_hh = mins_between_ms(rec.ll_time_ms, *rec.hh_time_ms);
        }
        if(ema_i){
            rec.ema200_cross_time_ms = v[*ema_i].ts_ms;
            rec.mins_ll_to_ema200 = mins_between_ms(rec.ll_time_ms, *rec.ema200_cross_time_ms);
        }
        if(ret_i){
            rec.retest_time_ms = v[*ret_i].ts_ms;
            rec.mins_ll_to_retest = mins_between_ms(rec.ll_time_ms, *rec.retest_time_ms);
        }

        t_hh.push_back(rec.mins_ll_to_hh);
        t_ema.push_back(rec.mins_ll_to_ema200);
        t_retest.push_back(rec.mins_ll_to_retest);

        res.records.push_back(std::move(rec));
    }

    res.summary.mins_ll_to_hh      = make_stats(t_hh);
    res.summary.mins_ll_to_ema200  = make_stats(t_ema);
    res.summary.mins_ll_to_retest  = make_stats(t_retest);
    return res;
}

// -------------------- вывод --------------------
inline bool write_records_csv(const std::string& path, const std::vector<Record>& R){
    std::ofstream f(path);
    if(!f.is_open()) return false;
    f << "ll_time_ms,ll_price,prev_ph_time_ms,prev_ph_price,hh_time_ms,mins_ll_to_hh,ema200_cross_time_ms,mins_ll_to_ema200,retest_time_ms,mins_ll_to_retest\n";
    for(const auto& r: R){
        auto optll = [&](const std::optional<int64_t>& v){ return v? std::to_string(*v) : ""; };
        auto optd  = [&](const std::optional<double>& v){ if(!v) return std::string(""); std::ostringstream ss; ss<<std::setprecision(12)<<*v; return ss.str(); };
        auto opti  = [&](const std::optional<int>& v){ return v? std::to_string(*v) : ""; };
        f << r.ll_time_ms << "," << r.ll_price << ","
          << optll(r.prev_ph_time_ms) << "," << optd(r.prev_ph_price) << ","
          << optll(r.hh_time_ms) << "," << opti(r.mins_ll_to_hh) << ","
          << optll(r.ema200_cross_time_ms) << "," << opti(r.mins_ll_to_ema200) << ","
          << optll(r.retest_time_ms) << "," << opti(r.mins_ll_to_retest) << "\n";
    }
    return true;
}

inline bool write_summary_json(const std::string& path, const Summary& S){
    auto emit_stats = [](std::ostream& os, const char* key, const SeriesStats& st){
        auto od = [&](const std::optional<double>& v){ if(!v) return std::string("null"); std::ostringstream ss; ss<<std::setprecision(10)<<*v; return ss.str(); };
        os << "  \""<<key<<"\": {\"count\": "<<st.count
           << ", \"median_min\": "<<od(st.median_min)
           << ", \"p25_min\": "<<od(st.p25_min)
           << ", \"p75_min\": "<<od(st.p75_min)
           << ", \"mean_min\": "<<od(st.mean_min)
           << ", \"max_min\": "<<od(st.max_min) << "}";
    };
    std::ofstream f(path);
    if(!f.is_open()) return false;
    f << "{\n";
    emit_stats(f, "mins_ll_to_hh", S.mins_ll_to_hh); f << ",\n";
    emit_stats(f, "mins_ll_to_ema200", S.mins_ll_to_ema200); f << ",\n";
    emit_stats(f, "mins_ll_to_retest", S.mins_ll_to_retest); f << ",\n";
    f << "  \"params\": {\"left\": "<<S.left<<", \"right\": "<<S.right
      << ", \"ema_fast\": "<<S.ema_fast<<", \"ema_slow\": "<<S.ema_slow
      << ", \"retest_eps\": "<<S.retest_eps<<", \"lookahead_min\": "<<S.lookahead_min
      << ", \"rows_used\": "<<S.rows_used<<"}\n";
    f << "}\n";
    return true;
}

// -------------------- пример использования --------------------
// Пример main — можно удалить и вызывать analyze(...) из вашего кода.
// Компиляция: g++ -std=c++17 -O3 test.cpp -o test
#ifdef LL_INTRADAY_ANALYZER_EXAMPLE_MAIN
int main(int argc, char** argv){
    if(argc<2){ std::cerr<<"Usage: "<<argv[0]<<" path_to_BTCUSDT_1m.csv\n"; return 1; }
    std::vector<Candle> v;
    if(!read_csv(argv[1], v)){ std::cerr<<"CSV read failed\n"; return 2; }
    Params P; P.left=3; P.right=3; P.ema_slow=200; P.lookahead_min=720; P.retest_eps=0.001;
    auto R = analyze(v, P);
    write_records_csv("btc_ll_intraday_results.csv", R.records);
    write_summary_json("btc_ll_intraday_summary.json", R.summary);
    std::cout<<"Done. Records: "<<R.records.size()<<"\n";
    return 0;
}
#endif

} // namespace llintraday


- 2025-09-13 (final)
  - Provider: removed Binance registration and hid provider dropdown when only one source is available; default provider now Hyperliquid.
  - Theme: applied Hyperliquid-inspired dark style to ImGui/ImPlot; updated candlestick palette (up=#30E0A1, down=#FF5A5A), dark plot background, subtle grids.
  - Fresh Candles: enabled HTTP polling in render loop; fixed stream provider mapping from active provider.
  - Persistence: merged pairs from config with stored data; normalized symbols for Hyperliquid so ETH persists across runs.

