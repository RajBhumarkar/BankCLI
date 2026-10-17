#pragma once
#include "Account.h"
#include <memory>
#include <vector>
#include <cppconn/connection.h>
#include <cppconn/prepared_statement.h>

// Plain data records, not extra service classes.
struct AccountData {
    std::int64_t id;
    std::string customerName;
    std::string type;
    Money balance;
};
struct TransactionData {
    std::int64_t id;
    std::string type;
    Money amount;
    std::string timestamp;
};
class Database {
    std::unique_ptr<sql::Connection> connection_;
    std::unique_ptr<sql::PreparedStatement> prepare(const std::string& query);
    std::int64_t lastId();
public:
    Database();
    void begin();
    void commit();
    void rollback();
    std::int64_t insertCustomer(const std::string& name, const std::string& phone);
    bool customerExists(std::int64_t id);
    std::int64_t insertAccount(std::int64_t customerId, const std::string& type, Money balance);
    AccountData getAccount(std::int64_t id, bool lock = false);
    void setBalance(std::int64_t id, Money balance);
    void record(std::int64_t id, const std::string& type, Money amount, std::int64_t relatedId = 0);
    std::vector<TransactionData> history(std::int64_t id);
};
