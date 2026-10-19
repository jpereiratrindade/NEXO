BEGIN;

ALTER TABLE nexo_users DROP COLUMN password_hash;

INSERT INTO schema_migrations(version) VALUES ('005_remove_password_credential');

COMMIT;
