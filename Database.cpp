#include "Database.h"
#include <mysql_driver.h>
#include <cppconn/resultset.h>
#include <cppconn/datatype.h>
#include <cppconn/statement.h>
#include <cstdlib>
#include <stdexcept>
namespace {
std::string environment(const char* key, const char* fallback = nullptr) {
    const char* value = std::getenv(key);
    if (value) return value;
    if (fallback) return fallback;
    throw std::runtime_error(std::string("Set environment variable ") + key + " before starting BankCLI.");
}
}
Database::Database() {
    const auto host = environment("BANKCLI_DB_HOST", "tcp://127.0.0.1:3306");
    const auto user = environment("BANKCLI_DB_USER");
    const auto password = environment("BANKCLI_DB_PASSWORD");
    connection_.reset(sql::mysql::get_mysql_driver_instance()->connect(host, user, password));
    connection_->setSchema(environment("BANKCLI_DB_NAME", "bankcli"));
}
std::unique_ptr<sql::PreparedStatement> Database::prepare(const std::string& query) {
    return std::unique_ptr<sql::PreparedStatement>(connection_->prepareStatement(query));
}
// Explicit transactions leave autocommit enabled outside banking operations.
void Database::begin() {
    std::unique_ptr<sql::Statement> statement(connection_->createStatement());
    statement->execute("START TRANSACTION");
}
void Database::commit() {
    std::unique_ptr<sql::Statement> statement(connection_->createStatement());
    statement->execute("COMMIT");
}
void Database::rollback() {
    std::unique_ptr<sql::Statement> statement(connection_->createStatement());
    statement->execute("ROLLBACK");
}
std::int64_t Database::lastId() {
    auto statement = prepare("SELECT LAST_INSERT_ID()");
    std::unique_ptr<sql::ResultSet> rows(statement->executeQuery());
    rows->next();
    return rows->getInt64(1);
}
std::int64_t Database::insertCustomer(const std::string& name, const std::string& phone) {
    auto statement = prepare("INSERT INTO customers (name, phone) VALUES (?, ?)");
    statement->setString(1, name);
    statement->setString(2, phone);
    statement->executeUpdate();
    return lastId();
}
bool Database::customerExists(std::int64_t id) {
    auto statement = prepare("SELECT customer_id FROM customers WHERE customer_id = ?");
    statement->setInt64(1, id);
    std::unique_ptr<sql::ResultSet> rows(statement->executeQuery());
    return rows->next();
}
std::int64_t Database::insertAccount(std::int64_t customerId, const std::string& type, Money balance) {
    auto statement = prepare("INSERT INTO accounts (customer_id, account_type, balance_paise) VALUES (?, ?, ?)");
    statement->setInt64(1, customerId);
    statement->setString(2, type);
    statement->setInt64(3, balance);
    statement->executeUpdate();
    return lastId();
}
AccountData Database::getAccount(std::int64_t id, bool lock) {
    auto statement = prepare("SELECT account_id, customer_id, account_type, balance_paise FROM accounts WHERE account_id = ?" +
                             std::string(lock ? " FOR UPDATE" : ""));
    statement->setInt64(1, id);
    std::unique_ptr<sql::ResultSet> rows(statement->executeQuery());
    if (!rows->next()) throw std::invalid_argument("Account not found.");
    AccountData data{rows->getInt64(1), "", rows->getString(3).asStdString(), rows->getInt64(4)};
    if (!lock) {
        auto customer = prepare("SELECT name FROM customers WHERE customer_id = ?");
        customer->setInt64(1, rows->getInt64(2));
        std::unique_ptr<sql::ResultSet> names(customer->executeQuery());
        if (names->next()) data.customerName = names->getString(1).asStdString();
    }
    return data;
}
void Database::setBalance(std::int64_t id, Money balance) {
    auto statement = prepare("UPDATE accounts SET balance_paise = ? WHERE account_id = ?");
    statement->setInt64(1, balance);
    statement->setInt64(2, id);
    statement->executeUpdate();
}
void Database::record(std::int64_t id, const std::string& type, Money amount, std::int64_t relatedId) {
    auto statement = prepare("INSERT INTO transactions (account_id, related_account_id, transaction_type, amount_paise) VALUES (?, ?, ?, ?)");
    statement->setInt64(1, id);
    if (relatedId == 0) statement->setNull(2, sql::DataType::BIGINT);
    else statement->setInt64(2, relatedId);
    statement->setString(3, type);
    statement->setInt64(4, amount);
    statement->executeUpdate();
}
std::vector<TransactionData> Database::history(std::int64_t id) {
    auto statement = prepare("SELECT transaction_id, transaction_type, amount_paise, created_at FROM transactions WHERE account_id = ? ORDER BY transaction_id DESC LIMIT 10");
    statement->setInt64(1, id);
    std::unique_ptr<sql::ResultSet> rows(statement->executeQuery());
    std::vector<TransactionData> result;
    while (rows->next()) result.push_back({rows->getInt64(1), rows->getString(2).asStdString(), rows->getInt64(3), rows->getString(4).asStdString()});
    return result;
}
