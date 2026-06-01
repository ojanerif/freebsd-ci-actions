---
module: daily-log
type: module
file: dev-docs/modules/daily-log.md
keywords: [daily, sprint, log, tarefas, sessao, registro]
description: Registro diário de tarefas concluídas, commits e pendências por sessão de trabalho
status: active
last_modified: 2026-06-02
---

# Daily Log — Módulo de Registro Diário

## Propósito

Registrar ao final de cada sessão de trabalho um resumo do que foi feito,
para consulta futura, rastreabilidade de decisões e continuidade entre sessões.

## Localização dos arquivos

Os dailies ficam em `dev-docs/daily/YYYY-MM-DD.md`, um arquivo por dia.

## Formato de cada daily

```markdown
---
date: YYYY-MM-DD
session: sess_YYYY-MM-DD_HHMM
actor_id: usr_osvaldo | agent_claude
actor_type: human | ai-agent
sprint_week: "YYYY-MM-DD / YYYY-MM-DD"
tickets_worked: [SWLSVROS-XXXX, ...]
---

# Daily — YYYY-MM-DD (Dia-da-semana)

## Resumo
Uma frase descrevendo o foco do dia.

## Tarefas concluídas
### <Ticket ou tema>
- O que foi feito (arquivos, commits, decisões)

## Commits do dia
| Commit | Repo | Descrição |

## Bloqueios / pendências
- O que ficou aberto e por quê

## Sprint status
Onde estamos no sprint da semana.
```

## Instruções para o agente (Shutdown Protocol)

Ao final de cada sessão de trabalho que produziu algum resultado concreto
(commits, decisões, Jira updates, análises), o agente DEVE:

1. Criar ou atualizar `dev-docs/daily/YYYY-MM-DD.md` com o resumo do dia.
2. Usar a data atual como nome do arquivo.
3. Incluir: resumo, tarefas concluídas, commits, bloqueios, sprint status.
4. Appender uma linha ao Learning Log abaixo.
5. Registrar evento no audit-log: `action: "daily.save"`.

## Learning Log

```
2026-06-02 | Módulo daily-log criado. Primeiro daily registrado para 2026-06-02.
             Sprint adiantado: 6597 e 6595 entregues na segunda. | daily-log | usr_osvaldo
```
