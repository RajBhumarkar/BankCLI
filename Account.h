#pragma once
#include "Money.h"

// These three classes answer one question: is this withdrawal allowed?
// They do not read input, run SQL, or change the saved balance.
class Account {
    Money balance_;

public:
    explicit Account(Money balance) : balance_(balance) {
        if (balance < 0) throw std::invalid_argument("Balance cannot be negative.");
    }
    virtual ~Account() = default;

    Money balance() const { return balance_; }
    virtual bool canWithdraw(Money amount) const = 0;
};

class SavingsAccount : public Account {
public:
    explicit SavingsAccount(Money balance) : Account(balance) {}

    bool canWithdraw(Money amount) const override {
        // Savings must keep at least Rs. 500 after a withdrawal.
        return amount > 0 && balance() >= SAVINGS_MINIMUM &&
               amount <= balance() - SAVINGS_MINIMUM;
    }
};

class CurrentAccount : public Account {
public:
    explicit CurrentAccount(Money balance) : Account(balance) {}

    bool canWithdraw(Money amount) const override {
        // Current accounts may reach zero, but cannot go below zero.
        return amount > 0 && amount <= balance();
    }
};
