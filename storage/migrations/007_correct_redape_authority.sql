BEGIN;

UPDATE nexo_providers
SET authority_uri = 'https://www.redape.dados.embrapa.br/'
WHERE provider_id = 'redape';

INSERT INTO schema_migrations(version) VALUES ('007_correct_redape_authority');

COMMIT;
