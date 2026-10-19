# NEXO

**Sempre pronto. Sempre incompleto.**

Plataforma independente de federação e proveniência científica. O NEXO
articula objetos mantidos por sistemas de autoridade, executa processamento
transitório e registra como cada resultado foi produzido.

Backend escrito em C++26.

## Fronteira

- sistemas institucionais preservam dados primários e produtos publicados;
- NEXO preserva referências, usos, recibos, transformações e achados aprovados;
- insumos externos brutos não formam acervo local;
- SisTer poderá oferecer experiência integrada por contratos públicos.

Este projeto é um MVP isolado. Não substitui `sister-nexo`.

## Estado do incremento 0

- modelo de referência externa verificável;
- contrato genérico de conector;
- migração PostgreSQL inicial;
- servidor HTTP mínimo em `127.0.0.1`;
- endpoints `/api/health`, `/api/readiness` e `/api/capabilities`;
- interface web responsiva servida na raiz `/`;
- autenticação por e-mail `@embrapa.br` com PBKDF2, sessão opaca, CSRF e cookie HttpOnly;
- testes do domínio e dos contratos.

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Todos os testes seguem o perfil **PROBANTE**: cada caso declara e verifica uma
invariante observável do domínio, dos contratos ou da experiência web.

## Executar

```bash
./build/nexo-server --port 18115
```

A interface estará disponível em `http://127.0.0.1:18115/`.

Defina `NEXO_DATABASE_URL`, aplique as migrações e crie o primeiro administrador:

```bash
export NEXO_DATABASE_URL='postgresql://...'
psql "$NEXO_DATABASE_URL" -f storage/migrations/001_initial.sql
psql "$NEXO_DATABASE_URL" -f storage/migrations/002_authentication.sql
./build/nexo-server setup-admin --email nome@embrapa.br --display-name "Administrador"
```

A senha é solicitada no terminal e nunca aceita por argumento.

Somente loopback é aceito. Persistência e conector GeoInfo entram nos próximos
incrementos.
