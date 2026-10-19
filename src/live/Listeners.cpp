#include <cmath>
#include <cstdio>
#include <fstream>
#include <ostream>

#include "quant/core/Errors.hpp"
#include "quant/core/TimeUtil.hpp"
#include "quant/live/ISignalListener.hpp"

namespace quant {

std::string describeTargets(const std::vector<TargetWeight>& w) {
    char buf[64];
    bool anyNonZero = false;
    for (const auto& t : w) anyNonZero = anyNonZero || std::fabs(t.weight) > 1e-9;
    if (!anyNonZero) return "FLAT";
    if (w.size() == 1) {
        std::snprintf(buf, sizeof buf, "%s(%.2f)", w[0].weight > 0 ? "LONG" : "SHORT", std::fabs(w[0].weight));
        return buf;
    }
    std::string s;
    for (const auto& t : w) {
        std::snprintf(buf, sizeof buf, "%s%s:%+.2f", s.empty() ? "" : " ", t.symbol.c_str(), t.weight);
        s += buf;
    }
    return s;
}

void ConsolePrinter::onSignal(const SignalEvent& e) {
    char close[48];
    std::snprintf(close, sizeof close, "%.4f", e.close);
    os_ << formatUtc(e.barOpenTime) << " " << e.universe << " close=" << close << " strategy=" << e.strategy
        << " target=" << describeTargets(e.weights);
    if (e.changed) os_ << " [changed from " << (e.previous.empty() ? "FLAT" : e.previous) << "]";
    if (!e.recommended) os_ << " [NOT RECOMMENDED: shadow mode]";
    os_ << std::endl;  // flush so a live display updates immediately
}

void FileLogger::onSignal(const SignalEvent& e) {
    bool needHeader = !std::ifstream(path_).good();
    std::ofstream out(path_, std::ios::app);
    if (!out) throw DataError("cannot open signal log " + path_);
    if (needHeader) out << "timestamp,universe,strategy,close,target_weights,changed\n";
    char close[48];
    std::snprintf(close, sizeof close, "%.8f", e.close);
    std::string targets;
    for (const auto& t : e.weights) {
        char b[64];
        std::snprintf(b, sizeof b, "%s%s=%.4f", targets.empty() ? "" : ";", t.symbol.c_str(), t.weight);
        targets += b;
    }
    out << formatUtc(e.barOpenTime) << "," << e.universe << "," << e.strategy << "," << close << "," << targets
        << "," << (e.changed ? 1 : 0) << "\n";
}

}  // namespace quant
