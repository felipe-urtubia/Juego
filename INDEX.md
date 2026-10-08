# Índice de Documentación - Chilean Footballito

**Versión pública actual:** `v0.1.2.1-alpha`

Este archivo sirve como punto de entrada a la documentación principal del proyecto.

## Documentación principal

### `README.md`
Descripción general del juego, funcionalidades actuales, arquitectura de alto nivel, compilación, pruebas, validación y roadmap resumido.

### `CHANGELOG.md`
Historial de cambios agrupado por versión publicada.

### `TODO.md`
Registro cronológico del desarrollo, integraciones, pruebas, validaciones y releases.

### `docs/ARCHITECTURE.md`
Descripción de la arquitectura técnica actual y de la separación entre los principales dominios del proyecto.

### `docs/ROADMAP.md`
Prioridades posteriores a `v0.1.2.0-alpha` y trabajo futuro.

### `CODEBASE_ANALYSIS.md`
Análisis técnico de la base de código.

---

## Match Center interactivo

Desde `v0.1.2.0-alpha`, el Match Center permite:

- Cambios de mentalidad.
- Cambios de instrucciones.
- Aplicar mentalidad e instrucción en el mismo corte.
- Sustituciones manuales.
- Visualización de sustituciones de la IA.
- Pausa y reanudación.
- Velocidades `1x`, `2x` y `4x`.
- Cortes de decisión que requieren confirmación con `CONTINUAR`.

Documentación relacionada:

- `docs/superpowers/specs/2026-09-19-interactive-match-center-design.md`
- `docs/superpowers/plans/2026-09-19-interactive-match-center-plan.md`
- `CHANGELOG.md`
- `TODO.md`

---

## CareerService y servicios de aplicación

La arquitectura de carrera continúa migrando desde grandes flujos de orquestación hacia servicios más pequeños y testeables.

Los cambios recientes incluyen lógica relacionada con:

- Finanzas semanales.
- Contratos.
- Ofertas de transferencias.
- Actualizaciones físicas.
- Simulación de divisiones.
- Liga activa.
- Copa.
- Comunicaciones del staff.
- Noticias de plantilla.
- Narrativas.
- Eventos de carrera.
- Reputación del manager.
- Despido del manager.

Los diseños y planes históricos están disponibles en:

- `docs/superpowers/specs/`
- `docs/superpowers/plans/`

---

## Calidad y validación

Para `v0.1.2.0-alpha` se verificó:

- Build completo.
- 100% de la suite CTest configurada.
- 5 divisiones.
- 90 equipos.
- 2200 jugadores.
- 0 errores.
- 0 advertencias.

---

## Auditorías históricas de bugs

Los siguientes documentos corresponden a auditorías realizadas en etapas anteriores:

- `BUG_SUMMARY.md`
- `BUG_ANALYSIS_DETAILED.md`
- `BUG_FIXES_GUIDE.md`

Se conservan como referencia histórica.

Sus líneas, severidades y estados no deben considerarse automáticamente vigentes sin comprobar nuevamente el código actual.

---

## Datos

### `docs/data_cleanup_report.md`

Contiene información relacionada con limpieza y consistencia de datos.

---

## Release actual

**Chilean Footballito `v0.1.2.1-alpha`**

https://github.com/felipe-urtubia/Juego/releases/tag/v0.1.2.1-alpha