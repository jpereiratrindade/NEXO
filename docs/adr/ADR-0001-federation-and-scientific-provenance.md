# ADR-0001 — Federação e proveniência científica

- Estado: aceito
- Data: 2026-10-16

## Contexto

Dados científicos relevantes permanecem distribuídos entre sistemas
institucionais com autoridade, regras de acesso e formas de persistência
próprias. Copiá-los permanentemente para outro repositório criaria conflito de
autoridade e governança.

## Decisão

NEXO é plataforma de articulação, não repositório universal.

1. Referência externa forma catálogo independente, identificada por provedor,
   identificador externo e versão ou estado conhecido.
2. Uso de referência pertence a uma execução e registra consulta, instante,
   digest e condições de obtenção.
3. Dados brutos recuperados existem somente em área transitória governada.
4. Execução produz recibo e candidato; achado exige promoção humana.
5. Publicação externa é fluxo separado, autorizado e idempotente.
6. SisTer consome projeções contratuais; não acessa tabelas NEXO.

## Consequências

- conectores implementam contrato comum, sem contaminar domínio;
- credenciais ficam em mecanismo operacional externo ao catálogo;
- disponibilidade de provedor degrada conector, não saúde básica do NEXO;
- snapshots integrais somente quando regra institucional permitir e exigir.
