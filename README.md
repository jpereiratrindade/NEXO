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
- autenticação sem senha por código enviado ao e-mail `@embrapa.br`;
- código de uso único com expiração, limite de tentativas e envio SMTP/TLS;
- sessão opaca, CSRF e cookie HttpOnly;
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
export NEXO_AUTH_PEPPER='segredo-aleatorio-com-no-minimo-32-caracteres'
export NEXO_SMTP_URL='smtps://smtp.exemplo:465'
export NEXO_SMTP_FROM='nexo@embrapa.br'
export NEXO_SMTP_USERNAME='usuario-do-relay'
export NEXO_SMTP_PASSWORD='senha-do-relay'
psql "$NEXO_DATABASE_URL" -f storage/migrations/001_initial.sql
psql "$NEXO_DATABASE_URL" -f storage/migrations/002_authentication.sql
psql "$NEXO_DATABASE_URL" -f storage/migrations/003_embrapa_email_identity.sql
psql "$NEXO_DATABASE_URL" -f storage/migrations/004_email_login_codes.sql
./build/nexo-server setup-admin --email nome@embrapa.br --display-name "Administrador"
```

`NEXO_SMTP_PASSWORD` e `NEXO_AUTH_PEPPER` existem somente no ambiente operacional.
Nunca são gravados pelo NEXO. O código recebido por e-mail também nunca é
persistido em claro.

Somente loopback é aceito. Persistência e conector GeoInfo entram nos próximos
incrementos.
