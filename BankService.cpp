#include "BankService.h"
#include "Account.h"
#include <algorithm>

// Small helpers keep the same validation out of each banking operation.
static void checkId(std::int64_t id) {
    if (id <= 0) throw std::invalid_argument("ID must be a positive whole number.");
}

static void checkAmount(Money amount) {
    if (amount <= 0) throw std::invalid_argument("Amount must be greater than zero.");
}

static void checkAddition(Money balance, Money amount) {
    if (amount > std::numeric_limits<Money>::max() - balance) {
        throw std::invalid_argument("The resulting balance would be too large.");
    }
}

static void checkWithdrawal(const AccountData& data, Money amount) {
    // Ordinary local objects: no new, delete, or heap allocation is needed here.
    SavingsAccount savings(data.balance);
    CurrentAccount current(data.balance);
    Account* account = nullptr;

    if (data.type == "SAVINGS") account = &savings;
    else if (data.type == "CURRENT") account = &current;
    else throw std::runtime_error("Unknown account type.");

    // Runtime polymorphism: the pointed-to account chooses the withdrawal rule.
    if (!account->canWithdraw(amount)) {
        throw std::invalid_argument(
            "Insufficient balance. Savings must keep Rs. 500; current cannot go below zero.");
    }
}

std::int64_t BankService::createCustomer(const std::string& name, const std::string& phone) {
    if (name.empty() || name.size() > 100 || name.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::invalid_argument("Name must contain 1 to 100 bytes of text.");
    }
    if (phone.size() != 10 || phone[0] < '6' || phone[0] > '9' ||
        phone.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument("Phone must be 10 digits, starting with 6, 7, 8 or 9.");
    }
    return database_.insertCustomer(name, phone);
}

std::int64_t BankService::createAccount(std::int64_t customerId, const std::string& type, Money opening) {
    checkId(customerId);
    if (type != "SAVINGS" && type != "CURRENT") {
        throw std::invalid_argument("Choose SAVINGS or CURRENT.");
    }
    const Money minimum = type == "SAVINGS" ? SAVINGS_MINIMUM : 0;
    if (opening < minimum) {
        throw std::invalid_argument("Savings needs at least Rs. 500; current needs at least Rs. 0.");
    }

    database_.begin();
    try {
        if (!database_.customerExists(customerId)) throw std::invalid_argument("Customer not found.");
        const auto id = database_.insertAccount(customerId, type, opening);
        if (opening > 0) database_.record(id, "OPENING", opening);
        database_.commit();
        return id;
    } catch (...) {
        database_.rollback();
        throw;
    }
}

void BankService::deposit(std::int64_t id, Money amount) {
    checkId(id);
    checkAmount(amount);

    database_.begin();
    try {
        const AccountData account = database_.getAccount(id, true);
        checkAddition(account.balance, amount);
        database_.setBalance(id, account.balance + amount);
        database_.record(id, "DEPOSIT", amount);
        database_.commit();
    } catch (...) {
        database_.rollback();
        throw;
    }
}

void BankService::withdraw(std::int64_t id, Money amount) {
    checkId(id);
    checkAmount(amount);

    database_.begin();
    try {
        const AccountData account = database_.getAccount(id, true);
        checkWithdrawal(account, amount);
        database_.setBalance(id, account.balance - amount);
        database_.record(id, "WITHDRAWAL", amount);
        database_.commit();
    } catch (...) {
        database_.rollback();
        throw;
    }
}

void BankService::transfer(std::int64_t from, std::int64_t to, Money amount) {
    checkId(from);
    checkId(to);
    checkAmount(amount);
    if (from == to) throw std::invalid_argument("Choose two different accounts.");

    database_.begin();
    try {
        // 1. Lock the smaller ID first in BOTH transfer directions.
        //    Consistent order avoids two transfers waiting on each other's row.
        const AccountData lower = database_.getAccount(std::min(from, to), true);
        const AccountData higher = database_.getAccount(std::max(from, to), true);
        const AccountData& source = from == lower.id ? lower : higher;
        const AccountData& destination = to == lower.id ? lower : higher;

        // 2. Check the account rule and integer limit before changing anything.
        checkWithdrawal(source, amount);
        checkAddition(destination.balance, amount);

        // 3. Change two balances and add two linked history records.
        database_.setBalance(from, source.balance - amount);
        database_.setBalance(to, destination.balance + amount);
        database_.record(from, "TRANSFER_OUT", amount, to);
        database_.record(to, "TRANSFER_IN", amount, from);

        // 4. Save all four changes together.
        database_.commit();
    } catch (...) {
        database_.rollback(); // Undo every change if any step failed.
        throw;                // Let the menu display the error.
    }
}

AccountData BankService::balance(std::int64_t id) {
    checkId(id);
    return database_.getAccount(id);
}

std::vector<TransactionData> BankService::history(std::int64_t id) {
    checkId(id);
    database_.getAccount(id); // Report a missing account instead of an empty history.
    return database_.history(id);
}
