#pragma once
#include <cstdint>
#include <string>
using Money = std::int64_t;
Money parseMoney(const std::string& text);
std::string formatMoney(Money paise);

class Account {
    std::int64_t id_;
    Money balance_;
public:
    Account(std::int64_t id, Money balance);
    virtual ~Account() = default;
    std::int64_t id() const { return id_; }
    Money balance() const { return balance_; }
    virtual bool canWithdraw(Money amount) const = 0;
};
class SavingsAccount : public Account {
public:
    using Account::Account;
    bool canWithdraw(Money amount) const override;
};
class CurrentAccount : public Account {
public:
    using Account::Account;
    bool canWithdraw(Money amount) const override;
};
