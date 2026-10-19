BEGIN;

CREATE TABLE nexo_users (
    user_id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    login varchar(120) NOT NULL UNIQUE CHECK (btrim(login) <> ''),
    display_name varchar(200) NOT NULL CHECK (btrim(display_name) <> ''),
    password_hash text NOT NULL CHECK (password_hash LIKE 'pbkdf2-sha256$310000$%'),
    role varchar(20) NOT NULL CHECK (role IN ('admin', 'reader')),
    active boolean NOT NULL DEFAULT true,
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE nexo_sessions (
    session_id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id uuid NOT NULL REFERENCES nexo_users(user_id),
    secret_hash char(64) NOT NULL UNIQUE,
    csrf_hash char(64) NOT NULL,
    expires_at timestamptz NOT NULL,
    revoked_at timestamptz,
    created_at timestamptz NOT NULL DEFAULT now()
);

CREATE INDEX nexo_sessions_active_idx ON nexo_sessions(secret_hash, expires_at)
    WHERE revoked_at IS NULL;

INSERT INTO schema_migrations(version) VALUES ('002_authentication');
COMMIT;
