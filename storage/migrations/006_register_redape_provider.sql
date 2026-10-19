BEGIN;

INSERT INTO nexo_providers(provider_id, display_name, authority_uri, status)
VALUES ('redape', 'Repositório de Dados de Pesquisa da Embrapa',
        'https://redape.dados.embrapa.br/', 'active');

INSERT INTO schema_migrations(version) VALUES ('006_register_redape_provider');

COMMIT;
