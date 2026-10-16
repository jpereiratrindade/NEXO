# Orientações para trabalhar no NEXO

## Propósito

O NEXO articula informação científica distribuída, executa transformações
transitórias e preserva proveniência verificável sem substituir repositórios
institucionais.

## Regras

- Use C++26 e PostgreSQL/pgvector exclusivo.
- Preserve contratos públicos versionados em `contracts/`.
- Dados científicos externos permanecem transitórios.
- Nunca persista credenciais, tokens ou respostas brutas de provedores.
- HTTP escuta somente em loopback.
- Migrações são incrementais e imutáveis depois de aplicadas.
- Cálculo não vira achado científico sem promoção humana explícita.

## Verificação

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```
