BEGIN;

CREATE TABLE nexo_graph_runs (
    run_id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    algorithm varchar(100) NOT NULL,
    parameters jsonb NOT NULL CHECK (jsonb_typeof(parameters) = 'object'),
    input_count integer NOT NULL CHECK (input_count >= 0),
    community_count integer NOT NULL CHECK (community_count >= 0),
    created_at timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE nexo_graph_nodes (
    run_id uuid NOT NULL REFERENCES nexo_graph_runs(run_id) ON DELETE CASCADE,
    reference_id uuid NOT NULL REFERENCES nexo_external_references(reference_id),
    community_id integer NOT NULL,
    degree integer NOT NULL CHECK (degree >= 0),
    weighted_degree double precision NOT NULL CHECK (weighted_degree >= 0),
    bridge_score double precision NOT NULL CHECK (bridge_score BETWEEN 0 AND 1),
    novelty_score double precision NOT NULL CHECK (novelty_score BETWEEN 0 AND 1),
    PRIMARY KEY (run_id, reference_id)
);

CREATE TABLE nexo_graph_edges (
    run_id uuid NOT NULL REFERENCES nexo_graph_runs(run_id) ON DELETE CASCADE,
    source_reference_id uuid NOT NULL REFERENCES nexo_external_references(reference_id),
    target_reference_id uuid NOT NULL REFERENCES nexo_external_references(reference_id),
    weight double precision NOT NULL CHECK (weight > 0 AND weight <= 1),
    evidence jsonb NOT NULL CHECK (jsonb_typeof(evidence) = 'array'),
    PRIMARY KEY (run_id, source_reference_id, target_reference_id),
    CHECK (source_reference_id < target_reference_id)
);

COMMENT ON TABLE nexo_graph_runs IS
    'Execuções reproduzíveis de descoberta estrutural; não constituem achados científicos.';

INSERT INTO schema_migrations(version) VALUES ('008_graph_learning');

COMMIT;
