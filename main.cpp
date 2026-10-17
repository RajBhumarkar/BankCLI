#include "BankService.h"
#include <cppconn/exception.h>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
std::string readText(const std::string& prompt) {
    std::cout << prompt;
    std::string text;
    if (!std::getline(std::cin, text)) throw std::runtime_error("Input ended.");
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}
std::int64_t readId(const std::string& prompt) {
    const auto text = readText(prompt);
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("Enter a positive whole-number ID.");
    std::istringstream input(text);
    std::int64_t id = 0;
    if (!(input >> id) || id <= 0) throw std::invalid_argument("Invalid or too-large ID.");
    return id;
}
Money readAmount(const std::string& prompt) { return parseMoney(readText(prompt)); }
void menu() {
    std::cout << "\n========== BANKCLI ==========\n\n"
              << "1. Create Customer\n2. Create Account\n3. Deposit\n4. Withdraw\n"
              << "5. Transfer Money\n6. Check Balance\n7. Transaction History\n8. Exit\n\n";
}
}

int main() {
    try {
        Database database;
        BankService bank(database);
        std::cout << "Connected to MySQL. BankCLI is an educational simulation.\n";
        while (true) {
            menu();
            try {
                const auto choice = readText("Enter choice: ");
                if (choice == "8") { std::cout << "Goodbye!\n"; break; }
                if (choice == "1") {
                    const auto name = readText("Full name: ");
                    const auto phone = readText("Phone (10 digits): ");
                    const auto id = bank.createCustomer(name, phone);
                    std::cout << "Customer created. ID: " << id << '\n';
                } else if (choice == "2") {
                    const auto customer = readId("Customer ID: ");
                    const auto type = readText("Type (1 = Savings, 2 = Current): ");
                    if (type != "1" && type != "2") throw std::invalid_argument("Choose 1 or 2.");
                    const auto amount = readAmount("Opening balance in rupees: ");
                    const auto id = bank.createAccount(customer, type == "1" ? "SAVINGS" : "CURRENT", amount);
                    std::cout << "Account created. Number: " << id << '\n';
                } else if (choice == "3" || choice == "4") {
                    const auto id = readId("Account number: ");
                    const auto amount = readAmount("Amount in rupees: ");
                    if (choice == "3") bank.deposit(id, amount);
                    else bank.withdraw(id, amount);
                    std::cout << "Operation completed.\n";
                } else if (choice == "5") {
                    const auto from = readId("From account: ");
                    const auto to = readId("To account: ");
                    const auto amount = readAmount("Amount in rupees: ");
                    bank.transfer(from, to, amount);
                    std::cout << "Transfer completed.\n";
                } else if (choice == "6") {
                    const auto data = bank.balance(readId("Account number: "));
                    std::cout << data.customerName << " | " << data.type << " | " << formatMoney(data.balance) << '\n';
                } else if (choice == "7") {
                    const auto rows = bank.history(readId("Account number: "));
                    std::cout << "ID | Type | Amount | Timestamp\n";
                    for (const auto& row : rows)
                        std::cout << row.id << " | " << row.type << " | " << formatMoney(row.amount) << " | " << row.timestamp << '\n';
                    if (rows.empty()) std::cout << "No transactions yet.\n";
                } else {
                    std::cout << "Invalid choice. Enter a number from 1 to 8.\n";
                }
            } catch (const sql::SQLException& error) {
                if (error.getErrorCode() == 1062) std::cerr << "That phone number is already registered.\n";
                else {
                    // Stop rather than reuse a connection whose transaction outcome may be unknown.
                    std::cerr << "Database operation failed (code " << error.getErrorCode()
                              << "). Reconnect and check balances/history before retrying.\n";
                    return 1;
                }
            } catch (const std::invalid_argument& error) {
                std::cerr << error.what() << '\n';
            } catch (const std::runtime_error&) {
                if (std::cin.eof()) { std::cout << "\nInput ended. Goodbye!\n"; return 0; }
                throw;
            }
        }
    } catch (const sql::SQLException& error) {
        std::cerr << "MySQL connection/setup failed (code " << error.getErrorCode()
                  << "). Check server, environment variables and schema.sql.\n";
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
