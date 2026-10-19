BEGIN;

ALTER TABLE nexo_users ALTER COLUMN password_hash DROP NOT NULL;

CREATE TABLE nexo_login_codes (
    challenge_id char(32) PRIMARY KEY,
    email varchar(254) NOT NULL CHECK (email ~ '^[^@[:space:]]+@embrapa[.]br$'),
    code_hash char(64) NOT NULL,
    attempts smallint NOT NULL DEFAULT 0 CHECK (attempts BETWEEN 0 AND 5),
    expires_at timestamptz NOT NULL,
    consumed_at timestamptz,
    created_at timestamptz NOT NULL DEFAULT now()
);

CREATE INDEX nexo_login_codes_rate_idx ON nexo_login_codes(email, created_at DESC);
CREATE INDEX nexo_login_codes_expiry_idx ON nexo_login_codes(expires_at)
    WHERE consumed_at IS NULL;

INSERT INTO schema_migrations(version) VALUES ('004_email_login_codes');
COMMIT;
