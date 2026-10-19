#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
port="${NEXO_PORT:-18116}"

if ! command -v podman >/dev/null 2>&1; then
  echo "Podman não encontrado. Instale-o ou configure NEXO_DATABASE_URL." >&2
  exit 1
fi

cd "$root"
podman compose up -d postgres

echo "Aguardando PostgreSQL local..."
for attempt in $(seq 1 30); do
  if podman exec nexo-postgres pg_isready -U nexo -d nexo >/dev/null 2>&1; then
    break
  fi
  if [[ "$attempt" -eq 30 ]]; then
    echo "PostgreSQL não ficou pronto." >&2
    exit 1
  fi
  sleep 1
done

for migration in storage/migrations/*.sql; do
  version="$(basename "$migration" .sql)"
  migrations_table="$(podman exec nexo-postgres psql -U nexo -d nexo -Atc \
    "SELECT to_regclass('public.schema_migrations') IS NOT NULL")"
  if [[ "$migrations_table" == "t" ]] && podman exec nexo-postgres psql -U nexo -d nexo -Atc \
      "SELECT 1 FROM schema_migrations WHERE version='$version'" | grep -qx 1; then
    continue
  fi
  echo "Aplicando migração $version..."
  podman exec -i nexo-postgres psql -v ON_ERROR_STOP=1 -U nexo -d nexo < "$migration"
done

export NEXO_DATABASE_URL="postgresql://nexo@127.0.0.1:55432/nexo"
export NEXO_AUTH_PEPPER="${NEXO_AUTH_PEPPER:-$(openssl rand -hex 32)}"

cmake -S . -B build -G Ninja
cmake --build build
exec ./build/nexo-server --port "$port"
