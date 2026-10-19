-- NEXO Migration 009: Metadados científicos estruturados e classes de salvaguarda legal
-- Adiciona colunas normalizadas para enriquecer a proveniência e a cartografia relacional
-- sem violar a regra de nunca persistir payloads brutos externos.

ALTER TABLE nexo_external_references
    ADD COLUMN IF NOT EXISTS description text,
    ADD COLUMN IF NOT EXISTS keywords text[] NOT NULL DEFAULT '{}',
    ADD COLUMN IF NOT EXISTS authors text[] NOT NULL DEFAULT '{}',
    ADD COLUMN IF NOT EXISTS subjects text[] NOT NULL DEFAULT '{}',
    ADD COLUMN IF NOT EXISTS collection_name varchar(500),
    ADD COLUMN IF NOT EXISTS restricted_files integer NOT NULL DEFAULT 0;

INSERT INTO schema_migrations(version) VALUES ('009_scientific_metadata');
