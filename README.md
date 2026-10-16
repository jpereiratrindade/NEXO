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
- testes do domínio e dos contratos.

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## Executar

```bash
./build/nexo-server --port 18115
```

Somente loopback é aceito. Persistência e conector GeoInfo entram nos próximos
incrementos.
