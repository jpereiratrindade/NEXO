BEGIN;

CREATE EXTENSION IF NOT EXISTS pgcrypto;

CREATE TABLE schema_migrations (
    version text PRIMARY KEY,
    applied_at timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE nexo_providers (
    provider_id varchar(64) PRIMARY KEY CHECK (provider_id ~ '^[a-z][a-z0-9_-]{1,63}$'),
    display_name varchar(200) NOT NULL CHECK (btrim(display_name) <> ''),
    authority_uri text NOT NULL CHECK (authority_uri ~ '^https://'),
    status varchar(20) NOT NULL DEFAULT 'proposed'
        CHECK (status IN ('proposed', 'active', 'suspended', 'retired')),
    created_at timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE nexo_external_references (
    reference_id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    provider_id varchar(64) NOT NULL REFERENCES nexo_providers(provider_id),
    external_id varchar(500) NOT NULL CHECK (btrim(external_id) <> ''),
    title varchar(1000) NOT NULL CHECK (btrim(title) <> ''),
    canonical_uri varchar(4000) NOT NULL CHECK (canonical_uri ~ '^https?://'),
    version varchar(500),
    content_digest varchar(71) CHECK (content_digest ~ '^sha256:[0-9a-f]{64}$'),
    observed_at timestamptz NOT NULL,
    media_type varchar(200),
    access_class varchar(20) NOT NULL DEFAULT 'unknown'
        CHECK (access_class IN ('public', 'restricted', 'embargoed', 'unknown')),
    created_at timestamptz NOT NULL DEFAULT now(),
    UNIQUE NULLS NOT DISTINCT (provider_id, external_id, version)
);

CREATE TABLE nexo_reference_observations (
    observation_id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    reference_id uuid NOT NULL REFERENCES nexo_external_references(reference_id),
    connector_id varchar(64) NOT NULL,
    query_scope jsonb NOT NULL DEFAULT '{}'::jsonb CHECK (jsonb_typeof(query_scope) = 'object'),
    retrieval_status varchar(20) NOT NULL
        CHECK (retrieval_status IN ('discovered', 'retrieved', 'denied', 'unavailable', 'invalid')),
    receipt jsonb NOT NULL DEFAULT '{}'::jsonb CHECK (jsonb_typeof(receipt) = 'object'),
    observed_at timestamptz NOT NULL DEFAULT now()
);

COMMENT ON TABLE nexo_external_references IS
    'Catálogo de identidade e estado conhecido; nunca contém dado científico bruto.';
COMMENT ON TABLE nexo_reference_observations IS
    'Recibo sem credenciais, tokens ou resposta bruta do provedor.';

INSERT INTO schema_migrations(version) VALUES ('001_initial');
COMMIT;
