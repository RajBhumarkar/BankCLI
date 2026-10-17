# Verification record

Performed on 17 October 2026, Windows x64.

## Toolchain

- Visual Studio 18 2026 Build Tools; MSVC 19.51.
- C++17 Release build, CMake 4.3.1-msvc1.
- Official MySQL Community Server 8.4.11, portable ZIP.
- Official MySQL Connector/C++ 26.7.0, JDBC-style API.
- Server bound to 127.0.0.1:33307, separate workspace data directory.
- Downloaded ZIP MD5 values matched the checksums on the official download pages.
- Initial MinGW DLL problem bypassed using MSVC.

## Actual results

- CMake configured and compiled BankCLI and BankCLITests successfully.
- CTest: account_rules passed.
- Full integration suite: **66 checks passed**, repeated successfully in a second newly created database.
- Menu-driven CLI: all seven features exercised using the restricted application database user.
- CLI additionally checked invalid choices, duplicate phone, missing account, zero deposit, savings minimum failure, option 8 and end-of-input exit.
- Application restarted in a new process: committed balances were retained.
- MySQL cleanly shut down and restarted: Demo Alice retained Rs. 4000.00 and Demo Bob retained Rs. 3000.00.
- local.ps1 Start, Run, Stop, Build and Test executed successfully.

## What the 66 checks cover

Exact paise conversion; formatting; integer limit; malformed amounts; polymorphic savings/current withdrawal boundaries; empty name; invalid/duplicate phone; missing customer; opening minimum; apostrophe-safe name persistence; deposits; withdrawals; successful transfer; paired history; insufficient funds; minimum violation; self-transfer; missing accounts; zero/negative amounts; overflow; zero-opening history; history limit/order; SQL-failure rollback; concurrency; reconnect persistence.

## Reproduce the important rollback test

With the workspace server running:

```powershell
.\local.ps1 Test
```

This creates a fresh database with a name ending in _test. It applies schema.sql and runs:

```powershell
.\build\Release\BankCLITests.exe --integration
```

The script supplies the environment variables and test administrator credential. A fresh database is required for each integration run; the executable refuses a database whose name does not end in _test, and requires an empty customers table.

For another computer, create a new empty database ending in _test, apply schema.sql, and point BANKCLI_DB_HOST, BANKCLI_DB_NAME, BANKCLI_DB_USER and BANKCLI_DB_PASSWORD at it. The test user needs permissions to insert/update/select and the TRIGGER privilege. Use these elevated permissions only in the test database.

The test creates two accounts and performs a normal transfer. It then installs this test-only trigger:

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

It attempts a transfer of Rs. 123.45. Both UPDATE statements and TRANSFER_OUT execute; inserting TRANSFER_IN raises the SQL error. BankService catches it and calls ROLLBACK.

The test asserts:
- Both balances match their pre-transfer values.
- The total history row count is unchanged.
- Failed deposit and withdrawal history writes undo their balance updates.
- A failed opening-history write removes the new account as well.
- Later valid transactions still work.

The trigger is removed, including on test failure.

## Concurrency

Two independent connections each perform 40 transfers of Rs. 1 in opposite directions between the same accounts. Both batches finish; final balances match expected values, and 162 transfer records exist (160 from concurrent transfers plus 2 from the earlier successful transfer). No partial history records remain from the failed transfer.

## Run only account/parser tests

```powershell
ctest --test-dir build -C Release --output-on-failure
```

No running database is required for these tests. Connector DLLs must still be present beside the executable.

## Not claimed

No production certification, load benchmark, crash/power-loss testing, exhaustive deadlock testing, remote TLS validation, or GitHub publication. SQL connection-loss outcome during COMMIT is not automatically resolved.
