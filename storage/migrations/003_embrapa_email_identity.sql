BEGIN;

ALTER TABLE nexo_users RENAME COLUMN login TO email;
ALTER TABLE nexo_users ALTER COLUMN email TYPE varchar(254);

UPDATE nexo_users
SET email = lower(email), active = false, updated_at = now()
WHERE email <> lower(email) OR email !~ '^[^@[:space:]]+@embrapa[.]br$';

ALTER TABLE nexo_users ADD CONSTRAINT nexo_users_active_embrapa_email
    CHECK (NOT active OR email ~ '^[^@[:space:]]+@embrapa[.]br$');

INSERT INTO schema_migrations(version) VALUES ('003_embrapa_email_identity');
COMMIT;
