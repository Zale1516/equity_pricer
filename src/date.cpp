#include "ep/date.hpp"
#include <stdexcept>
#include <algorithm>

namespace ep {

long days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097L + static_cast<long>(doe) - 719468L;
}

Date::Date(int y, unsigned m, unsigned d) : serial(days_from_civil(y, m, d)) {}

Date parse_ymd_slash(const std::string& s) {
    size_t p = 0;
    auto num = [&](char stop) {
        int v = 0; bool any = false;
        while (p < s.size() && s[p] != stop) {
            if (s[p] < '0' || s[p] > '9') { p++; continue; }
            v = v * 10 + (s[p] - '0'); any = true; p++;
        }
        if (!any) throw std::runtime_error("bad date: " + s);
        return v;
    };
    int y = num('/'); if (p < s.size()) p++;
    int m = num('/'); if (p < s.size()) p++;
    int d = num('\0');
    return Date(y, m, d);
}

bool is_weekend(const Date& dt) {
    long w = ((dt.serial % 7) + 4) % 7;
    if (w < 0) w += 7;
    return w == 0 || w == 6;
}

static const std::vector<long>& hk_holidays() {
    static const std::vector<long> h = [] {
        const Date d[] = {
            {2024,12,25}, {2024,12,26},
            {2025, 1, 1},
            {2025, 1,29}, {2025, 1,30}, {2025, 1,31},
            {2025, 4, 4},
            {2025, 4,18}, {2025, 4,21},
            {2025, 5, 1}, {2025, 5, 5},
            {2025, 7, 1},
            {2025,10, 1}, {2025,10, 7}, {2025,10,29},
            {2025,12,25}, {2025,12,26},
        };
        std::vector<long> s; s.reserve(sizeof(d) / sizeof(d[0]));
        for (const Date& x : d) s.emplace_back(x.serial);
        std::sort(s.begin(), s.end());
        return s;
    }();
    return h;
}

bool is_hk_business_day(const Date& dt) {
    if (is_weekend(dt)) return false;
    const std::vector<long>& h = hk_holidays();
    return !std::binary_search(h.begin(), h.end(), dt.serial);
}

Date add_hk_business_days(const Date& dt, int n) {
    Date d = dt;
    while (n > 0) { d = Date(d.serial + 1); if (is_hk_business_day(d)) --n; }
    return d;
}

std::vector<Date> business_days(const Date& start, const Date& end) {
    std::vector<Date> out;
    for (long s = start.serial; s <= end.serial; ++s) {
        Date dt(s);
        if (is_hk_business_day(dt)) out.emplace_back(dt);
    }
    return out;
}

} // namespace ep
