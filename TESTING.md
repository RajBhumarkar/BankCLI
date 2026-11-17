# Verification and test instructions

Verified **17 November 2026**, Windows x64, using the updated source in this project.

## Actual results

| Check | Result |
| --- | --- |
| CMake Release build of BankCLI and BankCLITests | Passed; no project compiler warnings. |
| CTest `account_rules` | Passed. |
| Complete unit + MySQL integration suite | **66 checks passed**: 20 unit and 46 integration checks. |
| Two independent worker connections | 40 transfers each in opposite directions; **80 transfers completed**. |
| Late transfer SQL failure | Both balances and the first inserted history row rolled back. |
| Deposit, withdrawal, account-opening SQL failures | Pending balances/accounts/history rolled back. |
| All seven menu features | Passed using a user with SELECT, INSERT, UPDATE only. |
| Invalid menu/input cases | Passed: duplicate phone, missing account, invalid ID, zero deposit, excess decimals, savings minimum. |
| Option 8 and end-of-input | Clean exit. |
| `build.ps1` | Configured, built, copied DLLs, and ran CTest successfully. |
| `run.ps1` | Launched the app, accepted scripted input, and restored connection/password environment variables. |
| Fresh application process | Read the previously committed balances. |
| Clean MySQL shutdown and restart | Alice remained at Rs. 4000.00, Bob at Rs. 3000.00; all six demonstration history records remained. |

Integration output:

```text
Rollback demo: A=4000.00, B=3000.00 before and after forced SQL failure.
PASS: 66 checks.
```

The test server used a new data directory on 127.0.0.1:33317. No existing database was used or modified. The server was stopped after verification. Local credentials and server data are excluded from this project.

Toolchain: MSVC 19.51.36260 (Visual Studio Build Tools 2026), CMake 4.3.1-msvc1, MySQL Community Server 8.4.11, Connector/C++ 26.7.0 with the JDBC-style API. Other toolchains were not tested in this update.

## Run the unit checks

Build using the README instructions. The build script runs CTest. You can also run the test program directly without a database connection:

```powershell
.\build\Release\BankCLITests.exe
```

Expected output: `PASS: 20 checks.` The connector DLLs still need to be beside the program.

These checks cover exact money conversion, formatting, integer limits, malformed amounts, savings minimums, current-account zero balance, and calls through an Account pointer.

## Run all 66 checks against MySQL

Use a **new, empty test database for every run**, with a name ending in `_test`. The suite refuses other names and checks that all three tables are empty. It keeps test rows afterward for inspection and removes its test trigger.

From the project folder, connect using `mysql --host=127.0.0.1 --user=root -p`, then run the following at the `mysql>` prompt. Replace the password placeholder. If you already created the test user, skip CREATE USER and use its existing password.

```sql
CREATE DATABASE bankcli_run1_test CHARACTER SET utf8mb4;
USE bankcli_run1_test;
SOURCE schema.sql;

CREATE USER 'bankcli_test'@'localhost' IDENTIFIED BY 'REPLACE_WITH_YOUR_TEST_PASSWORD';
GRANT SELECT, INSERT, UPDATE, TRIGGER ON bankcli_run1_test.* TO 'bankcli_test'@'localhost';
EXIT;
```

Then, in PowerShell:

```powershell
.\run.ps1 -Integration -Database bankcli_run1_test -User bankcli_test
```

Enter the test user's password. If the server uses another port, add `-Port YOUR_PORT`. For the next run, use a new name such as `bankcli_run2_test`, load the schema there, and grant the test user the same permissions on that database.

The ordinary application user does not need TRIGGER permission. If your MySQL server has binary logging enabled and disallows a test user from creating triggers, use an administrator only in the isolated test database, or use a dedicated local test server with binary logging disabled. Do not relax permissions on the application database.

## How the rollback test works

The test installs a trigger only in the isolated test database:

```sql
CREATE TRIGGER bankcli_test_failure
BEFORE INSERT ON transactions FOR EACH ROW
BEGIN
    IF NEW.amount_paise = 12345 AND NEW.transaction_type <> 'TRANSFER_OUT' THEN
        SIGNAL SQLSTATE '45000'
        SET MESSAGE_TEXT = 'Intentional rollback test';
    END IF;
END;
```

It attempts a transfer of Rs. 123.45. Both balance updates and TRANSFER_OUT succeed first. The second history insert raises MySQL error 1644. The test checks that this specific SQL error occurred, then verifies that both balances and the total history count are unchanged after rollback. It also checks rollback for deposits, withdrawals, and opening an account. Valid operations work afterward.

## What the concurrency check proves

Two worker connections transfer Rs. 1 back and forth between the same accounts, 40 times each. The test checks the final balances and that all 160 concurrent history records were saved, plus the two records from the earlier normal transfer. The failed transfer contributes no records.

This checks correctness under that workload. It is not a throughput benchmark or exhaustive deadlock test.

## Recheck persistence manually

Run the README demonstration in your application database. Exit, run the application again, and check the balances. Then exit, cleanly stop/start your own MySQL server using your normal service controls, and check again. Both balances and history should remain.

The automatic integration suite checks persistence through a new connection. The server-restart result above was verified separately; the test executable does not restart your server.

No power-loss testing, production certification, remote TLS validation, or automatic recovery from a lost COMMIT response is claimed.
