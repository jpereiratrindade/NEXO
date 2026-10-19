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

export NEXO_DATABASE_URL="postgresql://nexo@127.0.0.1:55432/nexo"
export NEXO_AUTH_PEPPER="${NEXO_AUTH_PEPPER:-$(openssl rand -hex 32)}"

cmake -S . -B build -G Ninja
cmake --build build
exec ./build/nexo-server --port "$port"
