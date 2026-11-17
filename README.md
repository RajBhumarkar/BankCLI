# BankCLI — simple C++ banking project

A terminal banking project using **C++17 and MySQL**. This version keeps the original features and database schema, with shorter account classes, clear variable names, and comments explaining the important steps.

Start with the menu, then learn one operation at a time. You do not need to understand every database-library detail to understand how a deposit or transfer works.

## The seven features

| Menu | Feature | What it does |
| --- | --- | --- |
| 1 | Create customer | Saves a name and a unique phone number. |
| 2 | Create account | Opens a savings or current account for a customer. |
| 3 | Deposit | Adds money and records it in history. |
| 4 | Withdraw | Removes money if the account rule allows it. |
| 5 | Transfer / payment | Moves money between two accounts and records both sides. |
| 6 | Check balance | Shows the customer name, account type, and balance. |
| 7 | Transaction history | Shows the ten most recent transactions. |

Option 8 exits. Here, a payment means an account-to-account transfer; there is no external payment gateway.

Savings accounts must keep **Rs. 500**. Current accounts can reach **Rs. 0**, with no overdraft. Phone numbers must contain ten digits and start with 6–9; this checks format only. A customer can have multiple accounts.

## How this matches the résumé

| Résumé point | Where to find it |
| --- | --- |
| Seven banking features in C++ and MySQL | The seven methods in `BankService.h` and menu options 1–7. |
| Five classes and two account types | `Account`, `SavingsAccount`, `CurrentAccount`, `Database`, `BankService`. |
| Inheritance and polymorphism | `Account.h` and `checkWithdrawal()` in `BankService.cpp`. |
| Three tables | `customers`, `accounts`, `transactions` in `schema.sql`. |
| Atomic transfer, row locks, rollback | `BankService::transfer()` and `Database::getAccount()`. |
| 66 checks and 80 concurrent transfers | `tests.cpp`; actual results and reproduction steps in [TESTING.md](TESTING.md). |

The updated code was built and **all 66 checks passed on 17 November 2026**. MySQL restart persistence was checked separately.

## Read the code in this order

```text
main.cpp              Read the user's choice and display results
    |
BankService.cpp       Validate input and perform the banking operation
    |             |
Account.h         Database.cpp
Withdrawal rule   Run SQL against MySQL
```

1. **`Account.h`** — the three small account classes and their withdrawal rules.
2. **`BankService.cpp`** — start with `deposit()`, then `withdraw()`, then `transfer()`.
3. **`main.cpp`** — connect each menu choice to a service method.
4. **`schema.sql`** — understand the three tables and their relationships.
5. **`Database.cpp`** — see the actual SQL behind the service calls.
6. **`Money.h`** — exact rupee/paise conversion and formatting.
7. **`tests.cpp`** — read after you understand the application.

The `.h` files describe what a class provides; the `.cpp` files implement its work. The tiny account classes are implemented directly in their header. [LEARN.md](LEARN.md) explains the C++ syntax and walks through a complete transfer.

## Set up and run on Windows

### 1. Install the tools once

- Install [Microsoft C++ Build Tools](https://visualstudio.microsoft.com/downloads/) with **Desktop development with C++**, a Windows SDK, and **C++ CMake tools for Windows**. See [Microsoft's setup instructions](https://code.visualstudio.com/docs/cpp/config-msvc).
- Install and start [MySQL Community Server](https://dev.mysql.com/downloads/mysql/), version **8.0.16 or newer**. Workbench is optional; it does not replace the server.
- Download the **Windows x64 ZIP** of [MySQL Connector/C++](https://dev.mysql.com/downloads/connector/cpp/) and extract it somewhere you can keep it, for example `C:\dev\mysql-connector-c++`.

The connector folder must contain `include\jdbc\mysql_driver.h` and `lib64\vs14\mysqlcppconn.lib`. This project uses the JDBC-style C++ API. Use MSVC with the official Windows connector binaries; the Windows script is for Visual Studio builds.

### 2. Create the database once

Extract this project. Open PowerShell **in the project folder**, where `schema.sql` is located. Connect as your MySQL administrator:

```powershell
mysql --host=127.0.0.1 --user=root -p
```

If `mysql` is not on PATH, use its full path, for example `& 'C:\Program Files\MySQL\MySQL Server 8.4\bin\mysql.exe' --host=127.0.0.1 --user=root -p`.

Run these commands at the `mysql>` prompt. Replace the password placeholder with your own password locally:

```sql
CREATE DATABASE bankcli CHARACTER SET utf8mb4;
USE bankcli;
SOURCE schema.sql;

CREATE USER 'bankcli'@'localhost' IDENTIFIED BY 'REPLACE_WITH_YOUR_PASSWORD';
GRANT SELECT, INSERT, UPDATE ON bankcli.* TO 'bankcli'@'localhost';
EXIT;
```

Run the schema only on a fresh database. It creates exactly three tables and contains no DROP statements. **If you already have the original BankCLI database, its schema is unchanged: keep that database and skip this step.** Do not recreate tables over your existing data.

### 3. Build

In the project folder, run:

```powershell
.\build.ps1 -ConnectorRoot 'C:\dev\mysql-connector-c++'
```

Replace the path with your extracted connector folder. The script locates CMake, builds the application and tests, copies the required DLLs, and runs the account/parser tests. A running database is not needed for this build step.

If PowerShell blocks a downloaded script, inspect it first. You can allow scripts just for the current terminal:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
```

### 4. Run

Keep MySQL running, then use:

```powershell
.\run.ps1
```

Enter the **bankcli user's password**, which you chose in step 2. The script prompts without echoing it, runs the menu, and restores the previous connection environment afterward.

For a different database, user, or port:

```powershell
.\run.ps1 -Database my_bank -User my_user -Server 127.0.0.1 -Port 3306
```

After changing C++ code, run the build command again.

## Try a five-minute demonstration

On a fresh database, use the IDs printed by the program:

1. Create Alice with phone `9876543210`, then Bob with `9876543211`.
2. Create Alice's **savings** account with opening balance `5000`.
3. Create Bob's **current** account with opening balance `2000`.
4. Deposit `125.50` into Alice's account, then withdraw `125.50`.
5. Transfer `1000` from Alice to Bob.
6. Check both balances: Alice has **Rs. 4000.00** and Bob has **Rs. 3000.00**.
7. View both histories. The transfer has one `TRANSFER_OUT` and one `TRANSFER_IN` record.
8. Try withdrawing `3500.01` from Alice. It is rejected because less than Rs. 500 would remain.
9. Exit and run again. The balances are still saved in MySQL.

Enter amounts as `100`, `100.5`, or `100.50`, without currency symbols or commas. More than two decimal places, negative amounts, and zero deposits/withdrawals/transfers are rejected.

## Why transfers are reliable

`transfer()` follows the same visible sequence every time:

```text
START TRANSACTION
  lock the smaller account ID with SELECT ... FOR UPDATE
  lock the larger account ID with SELECT ... FOR UPDATE
  check the withdrawal rule and destination balance limit
  update the source balance
  update the destination balance
  insert TRANSFER_OUT
  insert TRANSFER_IN
COMMIT
```

If a step fails before a successful commit, `catch (...)` attempts `ROLLBACK`. The test deliberately fails the second history insert after the other writes have happened and checks that all four changes disappear. Deposits, withdrawals, and account opening also group balances and history in one transaction. [MySQL's locking-read documentation](https://dev.mysql.com/doc/refman/8.4/en/innodb-locking-reads.html) explains `FOR UPDATE`.

Money is stored as whole **paise**, using C++ `int64_t` and MySQL `BIGINT`: `125.50` becomes `12550`. This avoids floating-point rounding. Addition is checked for overflow.

## Files and tables

| File | Purpose |
| --- | --- |
| `Account.h` | Three account classes; no SQL or menu code. |
| `Money.h` | Money type, minimum balance constant, parser, formatter. |
| `BankService.h/.cpp` | Seven banking operations and input/business rules. |
| `Database.h/.cpp` | MySQL connection, prepared queries, transaction commands. |
| `main.cpp` | Menu, prompts, and error messages. |
| `schema.sql` | The original three-table schema, unchanged. |
| `tests.cpp` | 20 unit checks plus 46 integration checks. |
| `CMakeLists.txt` | Build instructions for CMake. |
| `build.ps1` / `run.ps1` | Portable Windows build and run helpers. |
| `LEARN.md` / `TESTING.md` | Code explanation and verification instructions. |

```text
customers (customer_id, name, phone)
    1 ─── many accounts (account_id, customer_id, account_type, balance_paise)
                   1 ─── many transactions
                         (transaction_id, account_id, related_account_id,
                          transaction_type, amount_paise, created_at)
```

`related_account_id` identifies the other account in a transfer. A positive opening balance gets an `OPENING` history record; a zero-balance current account starts with an empty history. History is ordered by newest transaction ID, not by concurrent commit time. `AccountData` and `TransactionData` are plain data structs, separate from the five behavior classes.

## If something fails

| Message/problem | What to check |
| --- | --- |
| Connector not found | Pass the folder containing `include\jdbc` and `lib64`. |
| DLL missing | Build through CMake and keep the copied DLLs beside the executable. See [MySQL's Windows linking notes](https://dev.mysql.com/doc/connectors/en/connector-cpp-apps-windows-notes.html). |
| Connection refused | Start MySQL and check its port. |
| Access denied | Check the database user/password and that user's host. |
| Unknown database/table | Check the database name and whether `schema.sql` was loaded. |
| Compiler/linker mismatch | Use an x64 Visual Studio build with the x64 Windows connector. |
| CMake generator/cache mismatch | Choose a fresh build folder for a different generator. |

For Linux or another CMake setup, configure with `cmake -S . -B build -DMYSQL_CONNECTOR_ROOT=/path/to/connector`, then `cmake --build build`. Only the Windows build was verified for this update. The program reads `BANKCLI_DB_HOST`, `BANKCLI_DB_NAME`, `BANKCLI_DB_USER`, and `BANKCLI_DB_PASSWORD`; the Windows run script sets these for you.

## Scope

This is a local learning project. It has no login, interest, loans, overdraft, external payments, or web application. Unexpected SQL errors stop the menu so it does not reuse a possibly broken connection. If the connection fails during COMMIT, reconnect and inspect balances/history before retrying; the outcome can be uncertain. There are no automatic retries or idempotency keys. Lock ordering reduces deadlocks but cannot prevent every possible deadlock caused by other database workloads.

Tests need a separate fresh database ending in `_test`; see [TESTING.md](TESTING.md). Database software, connector binaries, passwords, and database contents are not included in the source ZIP.

Based on [RajBhumarkar/BankCLI](https://github.com/RajBhumarkar/BankCLI), commit `9fa584a3a5acb4eec50e1fea53c8f48ce38df74a`. This update replaces the original machine-specific launcher and documentation. It does not publish changes to GitHub.
