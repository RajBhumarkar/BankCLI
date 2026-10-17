# BankCLI

A small C++17 + MySQL banking simulation for learning OOP, SQL and reliable transactions. It runs in a terminal and implements exactly seven banking features.

## Features

- Create customers with unique, validated phone numbers.
- Create savings accounts (minimum Rs. 500) or current accounts (minimum Rs. 0, no overdraft).
- Deposit and withdraw money with matching history records.
- Transfer money atomically between accounts.
- Show account balance, customer name and account type.
- Show the 10 most recent transactions.

Phone convention: 10 digits starting with 6–9, representing Indian mobile-number format. This checks format, not ownership or whether the number is active.

## Run on this computer now

The project has already been built. Portable MySQL 8.4.11 and Connector/C++ 26.7.0 are in the workspace's `work/dependencies` folder. The database uses **127.0.0.1:33307** to stay separate from standard installations.

Open **two PowerShell terminals** and run this in each:

```powershell
cd 'C:\Users\chint\Documents\Codex\2026-10-17\files-pasted-by-the-user-build\outputs\BankCLI'
```

In terminal 1:

```powershell
.\local.ps1 Start
```

This starts MySQL. Leave this terminal open. Wait for “ready for connections”.

In terminal 2:

```powershell
.\local.ps1 Run
```

This loads the local Windows-encrypted application credential, sets environment variables for the program, and starts BankCLI. The account has SELECT, INSERT and UPDATE access to only the application database. Environment variables are restored when the launcher finishes.

Already-created demonstration data:

| Account | Customer | Type | Balance |
| --- | --- | --- | --- |
| 1 | Demo Alice | SAVINGS | Rs. 4000.00 |
| 2 | Demo Bob | CURRENT | Rs. 3000.00 |

Balances will change when you perform operations.

After choosing 8 to exit, shut down MySQL cleanly:

```powershell
.\local.ps1 Stop
```

To rebuild after edits:

```powershell
.\local.ps1 Build
```

To run the rollback and integration demonstration while MySQL is running:

```powershell
.\local.ps1 Test
```

This creates a **new, separate test database each time**, runs the tests, and leaves its data available for inspection. Existing databases are never dropped. It uses the workspace administrator credential only for test setup and the test trigger.

The launcher is a convenience for this prepared workspace, not a portable installer. Its default work folder is two levels above this project, then `work`. If you move the project, pass `-WorkDirectory 'path-to-original-work'`. The encrypted credentials can be read only under the Windows account/computer that created them. Do not commit credentials, database files or downloaded binaries.

If PowerShell blocks the local script, inspect it first; for this terminal session only you can use:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
```

This changes script policy only until that terminal closes.

## Fresh Windows setup (another computer or a clone)

### 1. Compiler and CMake

Install [Microsoft C++ Build Tools](https://learn.microsoft.com/en-us/cpp/overview/acquire-msvc), selecting **Desktop development with C++**, a Windows SDK, and **C++ CMake tools for Windows**. VS Code is the editor; these tools compile the program.

Open **Developer PowerShell for Visual Studio** and check:

```powershell
cl
cmake --version
```

A compiler banner followed by “no source files” is normal for the first command.

### 2. MySQL and Connector/C++

Install [MySQL Community Server](https://dev.mysql.com/downloads/mysql/8.4.html). Use MySQL 8.0.16 or newer so CHECK constraints are enforced; this project was tested with 8.4.11. Workbench alone is not a database server.

Download the Windows x64 ZIP from [Connector/C++](https://dev.mysql.com/downloads/connector/cpp/) and extract it, for example to `C:\dev\mysql-connector-c++-26.7.0-winx64`. Use the JDBC-style C++ API shipped in that archive, not Connector/NET. The tested compiler and connector are both x64.

The connector provides:
- Headers in `include/jdbc`, describing the C++ database API.
- `lib64/vs14/mysqlcppconn.lib`, which the linker uses.
- DLLs in `lib64`, needed when the executable runs.

Our CMake file locates these and copies runtime DLLs beside the executable. See [MySQL's Windows linking notes](https://dev.mysql.com/doc/connector-cpp/26.7/en/connector-cpp-apps-windows-notes.html).

### 3. Create the database

Connect to your own MySQL server as its administrator using Workbench or the mysql client. Run:

```sql
CREATE DATABASE bankcli CHARACTER SET utf8mb4;
USE bankcli;
```

Then open and execute `schema.sql` against that database. It creates only three tables and does not drop existing data. Run it once on an empty database.

Create an application user with your own password. In the following SQL, replace the password placeholder **locally**, not in a committed file:

```sql
CREATE USER 'bankcli'@'localhost' IDENTIFIED BY '<choose-your-own-password>';
GRANT SELECT, INSERT, UPDATE ON bankcli.* TO 'bankcli'@'localhost';
```

The app does not need schema-editing privileges.

### 4. Build

In Developer PowerShell, change to your cloned BankCLI folder, then:

```powershell
cmake -S . -B build -A x64 -DMYSQL_CONNECTOR_ROOT='C:/dev/mysql-connector-c++-26.7.0-winx64' -DBANKCLI_BUILD_TESTS=ON
cmake --build build --config Release
```

Replace the connector path with your actual extracted location.

- `-S .`: read the project's CMake instructions here.
- `-B build`: keep generated files in a separate folder.
- `-A x64`: match the 64-bit connector.
- `MYSQL_CONNECTOR_ROOT`: tells CMake where the library is.
- `--build`: invokes the compiler and linker.
- `Release`: matches the downloaded release connector binaries.

### 5. Connect and run

Set variables in the same terminal where you run the program:

```powershell
$env:BANKCLI_DB_HOST = 'tcp://127.0.0.1:3306'
$env:BANKCLI_DB_NAME = 'bankcli'
$env:BANKCLI_DB_USER = 'bankcli'
$secret = Read-Host 'MySQL application password' -AsSecureString
$credential = [pscredential]::new('bankcli', $secret)
$env:BANKCLI_DB_PASSWORD = $credential.GetNetworkCredential().Password
.\build\Release\BankCLI.exe
Remove-Item Env:BANKCLI_DB_PASSWORD
```

`Read-Host` avoids putting the password into shell history. The program receives it through its environment. No database password is hardcoded in C++.

If setup fails, fix the first error before proceeding:
- Connector not found: check the extracted folder and `include/jdbc/mysql_driver.h`.
- DLL missing: build BankCLI through CMake so the post-build copy runs.
- Connection refused: start the server and check the port.
- Access denied: check username/password and the MySQL account's host.
- Unknown database/table: select the intended database and run `schema.sql`.
- Linker architecture errors: use x64 compiler and x64 connector.
- Existing CMake cache from another generator: use a new build directory, such as `build-msvc`.

## How the code fits together

```text
main.cpp → BankService → Account / SavingsAccount / CurrentAccount
                      → Database → MySQL
```

| File | Responsibility |
| --- | --- |
| main.cpp | Menu, parsing IDs, prompts, display and error messages. |
| Account.h / Account.cpp | Five main classes begin with the three account classes; money parsing/formatting helpers also live here. |
| Database.h / Database.cpp | Database class: connection, prepared queries, locks and transaction commands. |
| BankService.h / BankService.cpp | BankService class: the seven operations and business-rule coordination. |
| schema.sql | Customers, accounts and transaction history, with keys and constraints. |
| CMakeLists.txt | Build instructions and optional framework-free tests. |
| tests.cpp | Reproducible unit, integration, concurrency and rollback checks. |
| local.ps1 | Commands for this workspace's portable database and build tools. |
| TESTING.md | Actual test results and reproduction instructions. |
| .gitignore | Excludes builds, secrets and local data. |

There are **five main classes**. `AccountData` and `TransactionData` are simple structs for returning query results. `unique_ptr` owns connector objects and automatically deletes them when their scope ends; it avoids manual cleanup on exceptions.

The code applies separation of responsibilities: the UI handles interaction, the service checks business rules, account subclasses enforce withdrawal policies, and Database contains SQL. It does not implement a full design-pattern framework or claim to cover all SOLID principles.

### Milestone 1: CLI

The loop reads complete lines, validates choices and exits cleanly on option 8 or end of input. Failures are reported instead of crashing or silently accepting partial numbers.

Exercise: Why should `2abc` be rejected as an account number?

### Milestone 2: inheritance and polymorphism

`Account` stores private ID and balance fields. Its virtual `canWithdraw` method is overridden in both subclasses.

For a Rs. 600 balance and a Rs. 200 withdrawal:
- SavingsAccount returns false: only Rs. 400 would remain.
- CurrentAccount returns true.

The actual service creates the subclass based on the stored account type and calls `account->canWithdraw(amount)` through a base-class pointer. This is runtime polymorphism used in both withdrawals and outgoing transfers.

Exercise: Which method runs when an `Account*` points to a `SavingsAccount`?

### Milestone 3: persistence

Customers are stored once, accounts refer to customers by foreign key, and history records refer to accounts. Names and phones are not copied into every transaction. Account balance is deliberately stored as current state and changed in the same transaction as its history.

All user values are bound to `?` parameters, including names containing apostrophes. Fixed transaction commands contain no user input and use normal statements.

Exercise: Why is a UNIQUE database constraint still necessary after input validation?

### Milestone 4: banking and ACID

A transfer follows this order:

1. START TRANSACTION.
2. Read and lock the lower account ID with SELECT ... FOR UPDATE.
3. Read and lock the higher account ID.
4. Check source withdrawal rules and destination overflow.
5. Update both balances.
6. Insert TRANSFER_OUT and TRANSFER_IN records, linked by related_account_id.
7. COMMIT.

An exception runs ROLLBACK. Deposit, withdrawal and account opening also group balance and history changes together.

- **Atomicity:** the entire transfer commits or rolls back.
- **Consistency:** foreign keys, CHECK constraints and service rules protect valid state.
- **Isolation:** row locks prevent competing balance updates from overwriting each other. Consistent lock order reduces deadlocks for opposite transfers.
- **Durability:** committed data survives application and server restarts; this was tested.

Opening funds are recorded as OPENING, except a zero-balance current account has no history until its first monetary operation. All amounts are positive in history; the type indicates direction. History is ordered by descending transaction ID, a deterministic insertion order, not a claim about concurrent commit order.

Amounts are stored as signed BIGINT paise. `125.50` becomes `12550` using digit arithmetic. Inputs with more than two decimal places are rejected, and addition is checked before it can overflow.

Exercise: What would go wrong if we committed the source debit before crediting the destination?

### Milestone 5: verification and GitHub

See TESTING.md for the checks actually run. This includes a real SQL error after writes, concurrent transfers, and persistence after restarting MySQL.

The source is ready for a GitHub repository. Compiled programs, downloaded dependencies, credentials and database files are excluded. To publish an existing local repository, create an empty GitHub repository in your account, then use its actual URL:

```powershell
git remote add origin https://github.com/YOUR-USERNAME/BankCLI.git
git push -u origin main
```

Replace YOUR-USERNAME with your account. These commands require GitHub authentication. No remote repository has been created or pushed by this project setup.

Exercise: Why is a pre-validation failure insufficient to demonstrate rollback after a partial write?

## Short demonstration

On a fresh database, create two customers, then savings account A with Rs. 5000 and current account B with Rs. 2000. Select transfer and send Rs. 1000 from A to B.

```text
Before: A = Rs. 5000.00, B = Rs. 2000.00
After:  A = Rs. 4000.00, B = Rs. 3000.00
```

Try withdrawing Rs. 3500.01 from A: it fails, retaining Rs. 4000.00. Use history to inspect the corresponding records.

The integration test prints:

```text
Rollback demo: A=4000.00, B=3000.00 before and after forced SQL failure.
PASS: 66 checks.
```

## Scope and limitations

This is a local educational simulation, not real banking software. There is no authentication, interest, overdraft, loan system, web server or deployment.

The app stops on unexpected database errors instead of reusing a possibly broken connection. A connection loss during COMMIT can leave the outcome uncertain; reconnect and check balances/history before retrying. There is no automatic retry or idempotency key. The consistent locking order does not guarantee that every possible external workload can never deadlock.

The test-only trigger lives in an isolated test database, is removed by the test, and is never installed in the application database. Unit tests alone do not prove SQL correctness; the integration tests use real MySQL.
