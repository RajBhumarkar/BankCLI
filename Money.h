#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

// Store money as whole paise: Rs. 125.50 = 12550 paise.
// A double can introduce rounding errors, so it is not used for money.
using Money = std::int64_t;
constexpr Money SAVINGS_MINIMUM = 50000; // Rs. 500.00

inline Money parseMoney(const std::string& text) {
    const auto dot = text.find('.');
    const std::string rupees = text.substr(0, dot);
    const std::string paise = dot == std::string::npos ? "" : text.substr(dot + 1);

    if (rupees.empty() || (dot != std::string::npos && (paise.empty() || paise.size() > 2))) {
        throw std::invalid_argument("Enter an amount like 125 or 125.50.");
    }

    // Turn 125, 125.5 and 125.50 into the same digit string: 12500, 12550, 12550.
    std::string digits = rupees + paise;
    if (paise.empty()) digits += "00";
    else if (paise.size() == 1) digits += '0';

    Money amount = 0;
    for (char digit : digits) {
        if (digit < '0' || digit > '9') {
            throw std::invalid_argument("Use digits and at most one decimal point.");
        }
        const int value = digit - '0';
        if (amount > (std::numeric_limits<Money>::max() - value) / 10) {
            throw std::invalid_argument("Amount is too large.");
        }
        amount = amount * 10 + value;
    }
    return amount;
}

inline std::string formatMoney(Money amount) {
    if (amount < 0) throw std::invalid_argument("Money cannot be negative.");
    const Money paise = amount % 100;
    return "Rs. " + std::to_string(amount / 100) + "." +
           (paise < 10 ? "0" : "") + std::to_string(paise);
}
