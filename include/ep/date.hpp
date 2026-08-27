#pragma once
#include <string>
#include <vector>

namespace ep {

long days_from_civil(int y, unsigned m, unsigned d);   // days since 1970-01-01 (Hinnant)

class Date {
public:
    long serial = 0;                 // days since 1970-01-01
    Date() = default;
    explicit Date(long s) : serial(s) {}
    Date(int y, unsigned m, unsigned d);
    bool operator<(const Date& o) const { return serial < o.serial; }
    bool operator<=(const Date& o) const { return serial <= o.serial; }
    bool operator==(const Date& o) const { return serial == o.serial; }
};

Date   parse_ymd_slash(const std::string& s);          // parse "YYYY/M/D"
bool   is_weekend(const Date& d);                      // Saturday or Sunday
bool   is_hk_business_day(const Date& d);              // not a weekend and not an HKEX holiday
Date   add_hk_business_days(const Date& d, int n);     // add n HK business days (T+n clearance)
std::vector<Date> business_days(const Date& start, const Date& end);   // scheduled trading days, inclusive

inline double yearfrac(const Date& from, const Date& to) {   // ACT/365 Fixed
    return static_cast<double>(to.serial - from.serial) / 365.0;
}

} // namespace ep
