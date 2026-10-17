#include "BankService.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace {
void positive(Money amount) {
    if (amount <= 0) throw std::invalid_argument("Amount must be greater than zero.");
}
void validId(std::int64_t id) {
    if (id <= 0) throw std::invalid_argument("ID must be a positive whole number.");
}
void checkAddition(Money balance, Money amount) {
    if (amount > std::numeric_limits<Money>::max() - balance)
        throw std::invalid_argument("The resulting balance would be too large.");
}
void checkWithdrawal(const AccountData& data, Money amount) {
    // Actual runtime polymorphism: the database type chooses the derived object.
    std::unique_ptr<Account> account;
    if (data.type == "SAVINGS") account = std::make_unique<SavingsAccount>(data.id, data.balance);
    else if (data.type == "CURRENT") account = std::make_unique<CurrentAccount>(data.id, data.balance);
    else throw std::runtime_error("Unknown account type.");
    if (!account->canWithdraw(amount))
        throw std::invalid_argument("Insufficient balance: savings must retain Rs. 500.00; current accounts cannot overdraw.");
}
}

std::int64_t BankService::createCustomer(const std::string& name, const std::string& phone) {
    if (name.empty() || name.size() > 100 || name.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::invalid_argument("Name must contain 1 to 100 bytes of text.");
    // Explicit project convention: 10 digits, first digit 6-9 (Indian mobile format).
    if (phone.size() != 10 || phone[0] < '6' || phone[0] > '9' ||
        phone.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("Phone must be 10 digits, starting with 6, 7, 8 or 9.");
    return db_.insertCustomer(name, phone); // UNIQUE constraint also handles concurrent duplicates.
}
std::int64_t BankService::createAccount(std::int64_t customerId, const std::string& type, Money opening) {
    validId(customerId);
    if (type != "SAVINGS" && type != "CURRENT") throw std::invalid_argument("Choose SAVINGS or CURRENT.");
    if (opening < (type == "SAVINGS" ? 50000 : 0))
        throw std::invalid_argument("Savings requires at least Rs. 500.00; current requires at least Rs. 0.00.");
    db_.begin();
    try {
        if (!db_.customerExists(customerId)) throw std::invalid_argument("Customer not found.");
        const auto id = db_.insertAccount(customerId, type, opening);
        if (opening > 0) db_.record(id, "OPENING", opening);
        db_.commit();
        return id;
    } catch (...) {
        db_.rollback();
        throw;
    }
}
void BankService::deposit(std::int64_t id, Money amount) {
    validId(id);
    positive(amount);
    db_.begin();
    try {
        const auto data = db_.getAccount(id, true);
        checkAddition(data.balance, amount);
        db_.setBalance(id, data.balance + amount);
        db_.record(id, "DEPOSIT", amount);
        db_.commit();
    } catch (...) {
        db_.rollback();
        throw;
    }
}
void BankService::withdraw(std::int64_t id, Money amount) {
    validId(id);
    positive(amount);
    db_.begin();
    try {
        const auto data = db_.getAccount(id, true);
        checkWithdrawal(data, amount);
        db_.setBalance(id, data.balance - amount);
        db_.record(id, "WITHDRAWAL", amount);
        db_.commit();
    } catch (...) {
        db_.rollback();
        throw;
    }
}
void BankService::transfer(std::int64_t from, std::int64_t to, Money amount) {
    validId(from);
    validId(to);
    positive(amount);
    if (from == to) throw std::invalid_argument("Choose two different accounts.");
    db_.begin();
    try {
        // Two separate primary-key lookups guarantee the same lock order for A->B and B->A.
        const auto lower = db_.getAccount(std::min(from, to), true);
        const auto higher = db_.getAccount(std::max(from, to), true);
        const auto& source = from == lower.id ? lower : higher;
        const auto& destination = to == lower.id ? lower : higher;
        checkWithdrawal(source, amount);
        checkAddition(destination.balance, amount);
        db_.setBalance(from, source.balance - amount);
        db_.setBalance(to, destination.balance + amount);
        db_.record(from, "TRANSFER_OUT", amount, to);
        db_.record(to, "TRANSFER_IN", amount, from);
        db_.commit(); // Both balances AND both history rows become permanent together.
    } catch (...) {
        db_.rollback(); // Undo all writes if ANY step fails.
        throw;
    }
}
AccountData BankService::balance(std::int64_t id) {
    validId(id);
    return db_.getAccount(id);
}
std::vector<TransactionData> BankService::history(std::int64_t id) {
    validId(id);
    db_.getAccount(id); // Distinguish a missing account from an empty history.
    return db_.history(id);
}
