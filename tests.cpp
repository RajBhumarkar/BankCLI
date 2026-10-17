#include "BankService.h"
#include <mysql_driver.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>

int checks = 0;
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("FAIL: " + message);
    ++checks;
}
void rejects(const std::function<void()>& operation, const std::string& message) {
    bool failed = false;
    try { operation(); } catch (const std::exception&) { failed = true; }
    require(failed, message);
}
void unitTests() {
    require(parseMoney("125.50") == 12550, "exact paise");
    require(parseMoney("0.1") == 10, "one decimal digit");
    require(parseMoney("0") == 0, "zero parsing");
    require(parseMoney("92233720368547758.07") == std::numeric_limits<Money>::max(), "BIGINT maximum");
    for (const auto& value : {"", "-1", "+1", "1.001", "1.", ".5", "1e3", "1.2.3", "1 2", "92233720368547758.08"})
        rejects([&] { parseMoney(value); }, "invalid money");
    require(formatMoney(12550) == "Rs. 125.50", "formatting");
    SavingsAccount savings(1, 60000);
    CurrentAccount current(2, 60000);
    Account* polymorphic = &savings;
    require(polymorphic->canWithdraw(10000), "savings exact minimum");
    require(!polymorphic->canWithdraw(10001), "savings minimum protected");
    polymorphic = &current;
    require(polymorphic->canWithdraw(60000), "current can reach zero");
    require(!polymorphic->canWithdraw(60001), "no overdraft");
    require(!polymorphic->canWithdraw(0) && !polymorphic->canWithdraw(-1), "positive withdrawal");
}

void integrationTests() {
    const char* name = std::getenv("BANKCLI_DB_NAME");
    if (!name || std::string(name).size() < 5 ||
        std::string(name).substr(std::string(name).size() - 5) != "_test")
        throw std::runtime_error("Integration tests require an empty database whose name ends in _test.");
    Database database;
    BankService bank(database);
    auto* driver = sql::mysql::get_mysql_driver_instance();
    const char* host = std::getenv("BANKCLI_DB_HOST");
    std::unique_ptr<sql::Connection> admin(driver->connect(host ? host : "tcp://127.0.0.1:3306",
        std::getenv("BANKCLI_DB_USER"), std::getenv("BANKCLI_DB_PASSWORD")));
    admin->setSchema(name);
    std::unique_ptr<sql::Statement> sql(admin->createStatement());
    auto scalar = [&](const std::string& query) {
        std::unique_ptr<sql::ResultSet> rows(sql->executeQuery(query));
        rows->next();
        return rows->getInt64(1);
    };
    require(scalar("SELECT COUNT(*) FROM customers") == 0, "fresh test database");
    rejects([&] { bank.createCustomer(" ", "9876543210"); }, "empty name");
    rejects([&] { bank.createCustomer("Alice", "123"); }, "invalid phone");
    const auto alice = bank.createCustomer("Alice O'Neil", "9876543210");
    const auto bob = bank.createCustomer("Bob", "9876543211");
    rejects([&] { bank.createCustomer("Duplicate", "9876543210"); }, "duplicate phone");
    rejects([&] { bank.createAccount(999999, "CURRENT", 0); }, "missing customer");
    rejects([&] { bank.createAccount(alice, "SAVINGS", 49999); }, "opening minimum");
    const auto a = bank.createAccount(alice, "SAVINGS", 500000);
    const auto b = bank.createAccount(bob, "CURRENT", 200000);
    require(bank.balance(a).customerName == "Alice O'Neil", "prepared text roundtrip");
    bank.deposit(a, 12550);
    require(bank.balance(a).balance == 512550, "deposit");
    bank.withdraw(a, 12550);
    bank.transfer(a, b, 100000);
    require(bank.balance(a).balance == 400000 && bank.balance(b).balance == 300000, "successful transfer");
    require(bank.history(a)[0].type == "TRANSFER_OUT" && bank.history(b)[0].type == "TRANSFER_IN", "paired history");
    rejects([&] { bank.withdraw(a, 350001); }, "minimum violation");
    rejects([&] { bank.withdraw(b, 300001); }, "insufficient funds");
    rejects([&] { bank.transfer(a, a, 100); }, "self transfer");
    rejects([&] { bank.transfer(a, 999999, 100); }, "missing destination");
    rejects([&] { bank.deposit(999999, 100); }, "missing account");
    rejects([&] { bank.history(999999); }, "missing history account");
    rejects([&] { bank.deposit(a, 0); }, "zero deposit");
    rejects([&] { bank.transfer(a, b, -1); }, "negative transfer");
    require(bank.balance(a).balance == 400000 && bank.balance(b).balance == 300000, "rejections preserve balances");

    // A real SQL failure AFTER balance updates, and after the first transfer history row.
    sql->execute("CREATE TRIGGER bankcli_test_failure BEFORE INSERT ON transactions FOR EACH ROW "
                 "BEGIN IF NEW.amount_paise = 12345 AND NEW.transaction_type <> 'TRANSFER_OUT' THEN "
                 "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Intentional rollback test'; END IF; END");
    try {
        const auto beforeRows = scalar("SELECT COUNT(*) FROM transactions");
        rejects([&] { bank.transfer(a, b, 12345); }, "late transfer SQL failure");
        require(bank.balance(a).balance == 400000 && bank.balance(b).balance == 300000, "rollback restores BOTH balances");
        require(scalar("SELECT COUNT(*) FROM transactions") == beforeRows, "rollback removes first history row");
        rejects([&] { bank.deposit(a, 12345); }, "deposit SQL failure");
        rejects([&] { bank.withdraw(a, 12345); }, "withdrawal SQL failure");
        require(bank.balance(a).balance == 400000, "deposit and withdrawal rollback");
        const auto accountsBefore = scalar("SELECT COUNT(*) FROM accounts");
        rejects([&] { bank.createAccount(bob, "CURRENT", 12345); }, "opening record SQL failure");
        require(scalar("SELECT COUNT(*) FROM accounts") == accountsBefore, "account creation rollback");
        require(scalar("SELECT COUNT(*) FROM transactions") == beforeRows, "all failed histories rolled back");
    } catch (...) {
        sql->execute("DROP TRIGGER bankcli_test_failure");
        throw;
    }
    sql->execute("DROP TRIGGER bankcli_test_failure");
    std::cout << "Rollback demo: A=4000.00, B=3000.00 before and after forced SQL failure.\n";

    for (int i = 0; i < 12; ++i) bank.deposit(b, 100);
    const auto history = bank.history(b);
    require(history.size() == 10, "history limit");
    for (std::size_t i = 1; i < history.size(); ++i)
        require(history[i - 1].id > history[i].id, "newest first");
    const auto maximum = bank.createAccount(bob, "CURRENT", std::numeric_limits<Money>::max());
    rejects([&] { bank.deposit(maximum, 1); }, "deposit overflow");
    rejects([&] { bank.transfer(a, maximum, 1); }, "transfer overflow");
    require(bank.balance(a).balance == 400000, "overflow preserves source");
    const auto empty = bank.createAccount(bob, "CURRENT", 0);
    require(bank.history(empty).empty(), "zero-opening empty history");
    bank.withdraw(a, 350000);
    require(bank.balance(a).balance == 50000, "savings exact minimum");
    bank.deposit(a, 350000);

    // Independent connections contend on the same two account rows.
    auto transfers = [=](std::int64_t from, std::int64_t to) {
        auto* threadDriver = sql::mysql::get_mysql_driver_instance();
        threadDriver->threadInit();
        try {
            { Database local; BankService service(local);
              for (int i = 0; i < 40; ++i) service.transfer(from, to, 100); }
        } catch (...) { threadDriver->threadEnd(); throw; }
        threadDriver->threadEnd();
    };
    auto first = std::async(std::launch::async, transfers, a, b);
    auto second = std::async(std::launch::async, transfers, b, a);
    first.get();
    second.get();
    require(bank.balance(a).balance == 400000 && bank.balance(b).balance == 301200, "concurrent opposite transfers preserve balances");
    require(scalar("SELECT COUNT(*) FROM transactions WHERE transaction_type IN ('TRANSFER_IN','TRANSFER_OUT')") == 162, "all paired transfer records");
    { Database reopened; BankService restarted(reopened);
      require(restarted.balance(a).balance == 400000, "persistence after reconnect"); }
}
int main(int argc, char* argv[]) {
    try {
        unitTests();
        if (argc == 2 && std::string(argv[1]) == "--integration") integrationTests();
        std::cout << "PASS: " << checks << " checks.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
