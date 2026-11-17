# Learn BankCLI one part at a time

## 1. Start with an account rule

An account object holds a balance and answers `canWithdraw(amount)`.

```cpp
SavingsAccount savings(60000); // Rs. 600
CurrentAccount current(60000);

savings.canWithdraw(20000);   // false: only Rs. 400 would remain
current.canWithdraw(20000);   // true: current accounts may go down to zero
```

The values inside C++ are in paise. The menu converts the user's rupee input first.

`SavingsAccount : public Account` means SavingsAccount inherits from Account. The shared base class stores the balance. Each child class supplies its own withdrawal rule.

`virtual` allows the child object's rule to be selected when code uses a base-class pointer:

```cpp
Account* account = &savings;
bool allowed = account->canWithdraw(20000); // calls SavingsAccount's method
```

`&savings` means the address of the existing savings object. `account->` calls a method through a pointer. No object is copied or allocated on the heap here. The pointer is used only while the local object still exists.

In `BankService.cpp`, `checkWithdrawal()` chooses a savings or current object using the type saved in MySQL. Withdrawals AND outgoing transfers use this helper, so both enforce the same rule.

## 2. Follow one deposit

Suppose account 1 has Rs. 1000 and the user deposits Rs. 250.

1. `main.cpp` reads account `1` and amount `250`.
2. `parseMoney("250")` returns `25000` paise.
3. The menu calls `bank.deposit(1, 25000)`.
4. `BankService` checks that the ID and amount are positive.
5. `Database::begin()` starts a SQL transaction.
6. `getAccount(1, true)` reads and locks the account row.
7. `setBalance()` saves `125000` paise, and `record()` adds a DEPOSIT history row.
8. `commit()` saves both changes. If an operation fails, `rollback()` undoes pending changes.
9. The menu displays success.

The menu does not build SQL. Database does not ask the user questions. BankService connects the input, rules, and SQL in one readable operation.

## 3. Understand the three tables

- **customers**: who owns an account. Phone is unique.
- **accounts**: the owner ID, account type, and current balance.
- **transactions**: what happened to an account, when, and for how much.

An account's `customer_id` must refer to a real customer. This relationship is a foreign key. A customer's name is stored once instead of being copied into every transaction.

An SQL query with `?` placeholders is a prepared statement:

```cpp
auto statement = prepare("INSERT INTO customers (name, phone) VALUES (?, ?)");
statement->setString(1, name);
statement->setString(2, phone);
```

The first value replaces the first placeholder as data, and the second replaces the second. An apostrophe in `Alice O'Neil` does not become part of the SQL syntax.

## 4. Walk through a transfer

Alice has Rs. 5000. Bob has Rs. 2000. Alice sends Bob Rs. 1000.

| Saved value | Before | After |
| --- | --- | --- |
| Alice's balance | Rs. 5000 | Rs. 4000 |
| Bob's balance | Rs. 2000 | Rs. 3000 |
| Alice's history | Existing rows | New TRANSFER_OUT of Rs. 1000, related to Bob's account |
| Bob's history | Existing rows | New TRANSFER_IN of Rs. 1000, related to Alice's account |

All four writes belong to one SQL transaction. They commit together. A failed write causes rollback of the pending transaction.

`FOR UPDATE` locks the selected account row until the transaction ends. Another transfer using that row must wait, then read the current balance. This prevents both transfers from calculating their changes from the same old balance.

Both directions lock the smaller account ID first. If account 1 sends to 2 while account 2 sends to 1, both operations request locks in the order 1, then 2. This avoids the opposite-order deadlock pattern.

The repeated `begin / try / commit / catch / rollback` blocks are intentional. Each operation shows its entire transaction without a template, callback, or extra transaction class hiding the steps.

## 5. Recognize the C++ syntax you will see

| Syntax | Meaning here |
| --- | --- |
| `Money` | A name for a signed 64-bit integer storing paise. |
| `const` | This value or method does not change the relevant data. |
| `auto` | Let the compiler infer the variable's type. |
| `Database&` | Refer to an existing database object rather than copying it. |
| `std::vector<T>` | A list of items, such as history records. |
| `std::unique_ptr<T>` | Own a connector object and clean it up automatically. |
| `throw` | Report a failure to the calling code. |
| `catch (...)` | Catch any failure so pending writes can be rolled back, then rethrow it. |
| `override` | This child method implements a virtual base-class method. |

The project has five behavior classes. `AccountData` and `TransactionData` are simple result records; they are not extra service layers.

## 6. Explain the résumé using the code

You should be able to point to and explain these concrete examples:

- **Inheritance**: SavingsAccount and CurrentAccount derive from Account.
- **Polymorphism**: the Account pointer calls the correct `canWithdraw()` implementation.
- **Atomicity**: two balance updates and two history inserts commit or roll back together.
- **Consistency**: service rules and SQL constraints protect balances, references, and valid transaction types.
- **Isolation**: locked reads serialize conflicting balance changes.
- **Durability**: committed records survive closing the application and a clean MySQL restart.

Run the demonstration in README.md before studying the tests. Then read the deliberate SQL-failure test: it proves rollback after real writes, which a rejected input alone cannot prove.

## 7. What was simplified

- Account objects hold only their balance; an unused account-ID field was removed.
- Small account classes are together in `Account.h`; there is no separate Account.cpp to jump into.
- Money conversion has its own header, so it does not distract from inheritance.
- Withdrawal checks use ordinary local account objects and a base pointer instead of allocating a derived object dynamically.
- Database variables and result-building steps are spelled out.
- Each banking operation keeps a visible transaction sequence.
- The build shares one copy of the banking code between the menu and tests.
- Build/run scripts take ordinary paths and connection settings instead of relying on an old workspace or credential files.

MySQL still needs installation and a connector. Keeping real SQL transactions, persistence, row locks, and concurrency means that setup remains part of this project.
