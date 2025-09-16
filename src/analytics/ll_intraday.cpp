#include "analytics/ll_intraday.h"
#include "core/candle.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <vector>

namespace llintraday {

static std::vector<double> ema(const std::vector<double>& src, int length){
    if (length <= 1 || src.empty()) return src;
    std::vector<double> out(src.size(), std::numeric_limits<double>::quiet_NaN());
    double alpha = 2.0/(length+1.0);
    double m = src[0];
    out[0] = m;
    for(size_t i=1;i<src.size();++i){ m = alpha*src[i] + (1.0-alpha)*m; out[i]=m; }
    return out;
}

static void pivots(const std::vector<Core::Candle>& v, int left, int right, std::vector<uint8_t>& is_ph, std::vector<uint8_t>& is_pl){
    size_t n=v.size(); is_ph.assign(n,0); is_pl.assign(n,0);
    for(int i=left; i<(int)n-right; ++i){
        bool ph=true, pl=true;
        for(int k=0;k<=left;k++){ if(!(v[i].high >= v[i-k].high)) ph=false; if(!(v[i].low  <= v[i-k].low )) pl=false; }
        for(int k=1;k<=right;k++){ if(!(v[i].high > v[i+k].high)) ph=false; if(!(v[i].low  < v[i+k].low )) pl=false; }
        if(ph) is_ph[i]=1; if(pl) is_pl[i]=1;
    }
}

static std::vector<int> indices_of(const std::vector<uint8_t>& mask){ std::vector<int> idx; idx.reserve(mask.size()/4); for(size_t i=0;i<mask.size();++i) if(mask[i]) idx.push_back((int)i); return idx; }
static std::vector<int> lower_lows(const std::vector<int>& pl_idx, const std::vector<Core::Candle>& v){
    std::vector<int> out; out.reserve(pl_idx.size()/2); double prev_low = std::numeric_limits<double>::quiet_NaN(); bool has_prev=false;
    for(int i : pl_idx){ double L = v[i].low; if(!has_prev){ prev_low = L; has_prev=true; continue; } if(L < prev_low) out.push_back(i); prev_low = L; }
    return out;
}
static int mins_between_ms(int64_t a_ms, int64_t b_ms){ long long diff = (b_ms - a_ms)/1000LL; return (int)(diff/60LL); }

Result analyze_core_candles(const std::vector<Core::Candle>& v, const Params& P){
    Result res; res.summary.left=P.left; res.summary.right=P.right; res.summary.ema_fast=P.ema_fast; res.summary.ema_slow=P.ema_slow; res.summary.retest_eps=P.retest_eps; res.summary.lookahead_min=P.lookahead_min; res.summary.rows_used=v.size();
    if(v.size() < (size_t)(P.left+P.right+2)) return res;
    std::vector<double> closes; closes.reserve(v.size()); for(const auto& c: v) closes.push_back(c.close);
    auto ema_fast = llintraday::ema(closes, P.ema_fast);
    auto ema_slow = llintraday::ema(closes, P.ema_slow);
    std::vector<uint8_t> is_ph, is_pl; pivots(v, P.left, P.right, is_ph, is_pl);
    auto ph_idx = indices_of(is_ph); auto pl_idx = indices_of(is_pl);
    auto ll_idx = lower_lows(pl_idx, v);
    res.records.reserve(ll_idx.size());
    std::vector<std::optional<int>> t_hh, t_ema, t_retest; t_hh.reserve(ll_idx.size()); t_ema.reserve(ll_idx.size()); t_retest.reserve(ll_idx.size());
    auto last_ph_before = [&](int i) -> std::optional<int>{ int j=-1; for(int k : ph_idx){ if(k<i) j=k; else break; } return j>=0?std::optional<int>(j):std::nullopt; };
    auto first_hh_after = [&](int idx_ll, std::optional<int> last_ph_before_ll, int lookahead_last) -> std::optional<int>{ if(!last_ph_before_ll) return std::nullopt; double ref_high = v[*last_ph_before_ll].high; for(int j : ph_idx){ if(j<=idx_ll) continue; if(j>lookahead_last) break; if(v[j].high > ref_high) return j; } return std::nullopt; };
    auto first_close_above = [&](int idx_ll, const std::vector<double>& ema_vec, int lookahead_last)->std::optional<int>{ for(int i=idx_ll+1; i<=lookahead_last && i<(int)v.size(); ++i){ if(v[i].close > ema_vec[i]) return i; } return std::nullopt; };
    auto first_retest = [&](int idx_ll, double eps, int lookahead_last)->std::optional<int>{ double ll_price=v[idx_ll].low; double thr=ll_price*(1.0+eps); for(int i=idx_ll+1; i<=lookahead_last && i<(int)v.size(); ++i){ if(v[i].low<=thr) return i; } return std::nullopt; };
    for(int idx_ll : ll_idx){
        Record rec{}; rec.ll_time_ms = v[idx_ll].open_time; rec.ll_price = v[idx_ll].low;
        auto last_ph = last_ph_before(idx_ll); auto lookahead_last = std::min<int>((int)v.size()-1, idx_ll + P.lookahead_min);
        auto hh_i   = first_hh_after(idx_ll, last_ph, lookahead_last);
        auto ema_i  = first_close_above(idx_ll, ema_slow, lookahead_last);
        auto ret_i  = first_retest(idx_ll, P.retest_eps, lookahead_last);
        if(last_ph){ rec.prev_ph_time_ms = v[*last_ph].open_time; rec.prev_ph_price = v[*last_ph].high; }
        if(hh_i){ rec.hh_time_ms = v[*hh_i].open_time; rec.mins_ll_to_hh = mins_between_ms(rec.ll_time_ms, *rec.hh_time_ms); }
        if(ema_i){ rec.ema200_cross_time_ms = v[*ema_i].open_time; rec.mins_ll_to_ema200 = mins_between_ms(rec.ll_time_ms, *rec.ema200_cross_time_ms); }
        if(ret_i){ rec.retest_time_ms = v[*ret_i].open_time; rec.mins_ll_to_retest = mins_between_ms(rec.ll_time_ms, *rec.retest_time_ms); }
        t_hh.push_back(rec.mins_ll_to_hh); t_ema.push_back(rec.mins_ll_to_ema200); t_retest.push_back(rec.mins_ll_to_retest);
        res.records.push_back(std::move(rec));
    }
    auto make_stats = [](const std::vector<std::optional<int>>& data){ SeriesStats st; std::vector<int> x; for(const auto& o: data) if(o) x.push_back(*o); st.count=(int)x.size(); if(x.empty()) return st; std::sort(x.begin(), x.end()); auto perc=[&](double p){ double idx=p*(x.size()-1); size_t i0=(size_t)std::floor(idx), i1=(size_t)std::ceil(idx); double w=idx-i0; return (1.0-w)*x[i0]+w*x[i1]; }; double sum=0; for(int v: x) sum+=v; st.median_min=perc(0.5); st.p25_min=perc(0.25); st.p75_min=perc(0.75); st.mean_min=sum/x.size(); st.max_min=(double)x.back(); return st; };
    res.summary.mins_ll_to_hh     = make_stats(t_hh);
    res.summary.mins_ll_to_ema200 = make_stats(t_ema);
    res.summary.mins_ll_to_retest = make_stats(t_retest);
    return res;
}

bool write_records_csv(const std::string& path, const std::vector<Record>& R){
    std::ofstream f(path); if(!f.is_open()) return false;
    f << "ll_time_ms,ll_price,prev_ph_time_ms,prev_ph_price,hh_time_ms,mins_ll_to_hh,ema200_cross_time_ms,mins_ll_to_ema200,retest_time_ms,mins_ll_to_retest\n";
    auto optll=[&](const std::optional<int64_t>& v){ return v? std::to_string(*v) : std::string(); };
    auto optd=[&](const std::optional<double>& v){ if(!v) return std::string(); std::ostringstream ss; ss<<std::setprecision(12)<<*v; return ss.str(); };
    auto opti=[&](const std::optional<int>& v){ return v? std::to_string(*v) : std::string(); };
    for(const auto& r: R){
        f << r.ll_time_ms << "," << r.ll_price << ","
          << optll(r.prev_ph_time_ms) << "," << optd(r.prev_ph_price) << ","
          << optll(r.hh_time_ms) << "," << opti(r.mins_ll_to_hh) << ","
          << optll(r.ema200_cross_time_ms) << "," << opti(r.mins_ll_to_ema200) << ","
          << optll(r.retest_time_ms) << "," << opti(r.mins_ll_to_retest) << "\n";
    }
    return true;
}

bool write_summary_json(const std::string& path, const Summary& S){
    std::ofstream f(path); if(!f.is_open()) return false; auto od=[&](const std::optional<double>& v){ if(!v) return std::string("null"); std::ostringstream ss; ss<<std::setprecision(10)<<*v; return ss.str(); };
    f << "{\n";
    f << "  \"mins_ll_to_hh\": {\"count\": "<<S.mins_ll_to_hh.count<<", \"median_min\": "<<od(S.mins_ll_to_hh.median_min)
      << ", \"p25_min\": "<<od(S.mins_ll_to_hh.p25_min)<<", \"p75_min\": "<<od(S.mins_ll_to_hh.p75_min)<<", \"mean_min\": "<<od(S.mins_ll_to_hh.mean_min)<<", \"max_min\": "<<od(S.mins_ll_to_hh.max_min)<<"},\n";
    f << "  \"mins_ll_to_ema200\": {\"count\": "<<S.mins_ll_to_ema200.count<<", \"median_min\": "<<od(S.mins_ll_to_ema200.median_min)
      << ", \"p25_min\": "<<od(S.mins_ll_to_ema200.p25_min)<<", \"p75_min\": "<<od(S.mins_ll_to_ema200.p75_min)<<", \"mean_min\": "<<od(S.mins_ll_to_ema200.mean_min)<<", \"max_min\": "<<od(S.mins_ll_to_ema200.max_min)<<"},\n";
    f << "  \"mins_ll_to_retest\": {\"count\": "<<S.mins_ll_to_retest.count<<", \"median_min\": "<<od(S.mins_ll_to_retest.median_min)
      << ", \"p25_min\": "<<od(S.mins_ll_to_retest.p25_min)<<", \"p75_min\": "<<od(S.mins_ll_to_retest.p75_min)<<", \"mean_min\": "<<od(S.mins_ll_to_retest.mean_min)<<", \"max_min\": "<<od(S.mins_ll_to_retest.max_min)<<"},\n";
    f << "  \"params\": {\"left\": "<<S.left<<", \"right\": "<<S.right<<", \"ema_fast\": "<<S.ema_fast<<", \"ema_slow\": "<<S.ema_slow
      << ", \"retest_eps\": "<<S.retest_eps<<", \"lookahead_min\": "<<S.lookahead_min<<", \"rows_used\": "<<S.rows_used<<"}\n";
    f << "}\n";
    return true;
}

} // namespace llintraday
