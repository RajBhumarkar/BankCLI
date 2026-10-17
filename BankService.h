#pragma once
#include "Database.h"

class BankService {
    Database& db_;
public:
    explicit BankService(Database& database) : db_(database) {}
    std::int64_t createCustomer(const std::string& name, const std::string& phone);
    std::int64_t createAccount(std::int64_t customerId, const std::string& type, Money opening);
    void deposit(std::int64_t id, Money amount);
    void withdraw(std::int64_t id, Money amount);
    void transfer(std::int64_t from, std::int64_t to, Money amount);
    AccountData balance(std::int64_t id);
    std::vector<TransactionData> history(std::int64_t id);
};
