#include "Account.h"
#include <limits>
#include <stdexcept>

Account::Account(std::int64_t id, Money balance) : id_(id), balance_(balance) {
    if (id <= 0 || balance < 0) throw std::invalid_argument("Invalid account data.");
}
bool SavingsAccount::canWithdraw(Money amount) const {
    return amount > 0 && balance() >= 50000 && amount <= balance() - 50000;
}
bool CurrentAccount::canWithdraw(Money amount) const {
    return amount > 0 && amount <= balance();
}
Money parseMoney(const std::string& text) {
    // Parse digits directly, never through floating point.
    const auto dot = text.find('.');
    const auto whole = text.substr(0, dot);
    const auto fraction = dot == std::string::npos ? "" : text.substr(dot + 1);
    if (whole.empty() || (dot != std::string::npos &&
        (fraction.empty() || fraction.size() > 2)))
        throw std::invalid_argument("Use an amount like 125 or 125.50.");
    std::string digits = whole + fraction;
    if (fraction.empty()) digits += "00";
    else if (fraction.size() == 1) digits += '0';
    Money value = 0;
    for (char ch : digits) {
        if (ch < '0' || ch > '9') throw std::invalid_argument("Use digits and an optional decimal point.");
        const int digit = ch - '0';
        if (value > (std::numeric_limits<Money>::max() - digit) / 10)
            throw std::invalid_argument("Amount is too large.");
        value = value * 10 + digit;
    }
    return value;
}
std::string formatMoney(Money paise) {
    if (paise < 0) throw std::invalid_argument("Negative balance.");
    const auto cents = paise % 100;
    return "Rs. " + std::to_string(paise / 100) + "." +
           (cents < 10 ? "0" : "") + std::to_string(cents);
}
