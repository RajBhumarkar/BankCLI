#pragma once
#include "Database.h"

// The seven public methods correspond to the seven menu features.
class BankService {
    Database& database_; // Use the existing connection; do not copy it.

public:
    explicit BankService(Database& database) : database_(database) {}
    std::int64_t createCustomer(const std::string& name, const std::string& phone);
    std::int64_t createAccount(std::int64_t customerId, const std::string& type, Money opening);
    void deposit(std::int64_t id, Money amount);
    void withdraw(std::int64_t id, Money amount);
    void transfer(std::int64_t from, std::int64_t to, Money amount);
    AccountData balance(std::int64_t id);
    std::vector<TransactionData> history(std::int64_t id);
};
