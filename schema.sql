-- Run with MySQL 8.0.16+ (CHECK constraints must be enforced).
-- Select the intended database before running this script. No DROP statements.
CREATE TABLE customers (
    customer_id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    phone VARCHAR(10) NOT NULL UNIQUE,
    created_at TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    CONSTRAINT customer_name CHECK (CHAR_LENGTH(TRIM(name)) > 0),
    CONSTRAINT customer_phone CHECK (phone REGEXP '^[6-9][0-9]{9}$')
) ENGINE=InnoDB;

CREATE TABLE accounts (
    account_id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    customer_id BIGINT NOT NULL,
    account_type VARCHAR(7) NOT NULL,
    balance_paise BIGINT NOT NULL,
    created_at TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    FOREIGN KEY (customer_id) REFERENCES customers(customer_id),
    CONSTRAINT account_kind CHECK (account_type IN ('SAVINGS', 'CURRENT')),
    CONSTRAINT account_minimum CHECK (
        (account_type = 'SAVINGS' AND balance_paise >= 50000) OR
        (account_type = 'CURRENT' AND balance_paise >= 0)
    )
) ENGINE=InnoDB;

CREATE TABLE transactions (
    transaction_id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
    account_id BIGINT NOT NULL,
    related_account_id BIGINT NULL,
    transaction_type VARCHAR(12) NOT NULL,
    amount_paise BIGINT NOT NULL,
    created_at TIMESTAMP(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    FOREIGN KEY (account_id) REFERENCES accounts(account_id),
    FOREIGN KEY (related_account_id) REFERENCES accounts(account_id),
    INDEX account_history (account_id, transaction_id),
    CONSTRAINT positive_amount CHECK (amount_paise > 0),
    CONSTRAINT transaction_kind CHECK (transaction_type IN
        ('OPENING', 'DEPOSIT', 'WITHDRAWAL', 'TRANSFER_OUT', 'TRANSFER_IN')),
    CONSTRAINT transfer_relation CHECK (
        (transaction_type IN ('TRANSFER_OUT', 'TRANSFER_IN') AND
         related_account_id IS NOT NULL AND related_account_id <> account_id)
        OR
        (transaction_type IN ('OPENING', 'DEPOSIT', 'WITHDRAWAL') AND related_account_id IS NULL)
    )
) ENGINE=InnoDB;
