# Diseño: separación de transferencias en App Services

## Objetivo

Reducir responsabilidades de `src/career/app_services.cpp` separando la lógica de transferencias, contratos y préstamos en un módulo independiente, sin modificar el comportamiento del juego ni la API pública.

## Nuevo módulo

Crear:

`src/career/app_services_transfers.cpp`

El archivo `include/career/app_services.h` se mantiene como fachada pública y conserva las firmas existentes.

## Servicios públicos a mover

* `buyTransferTargetService`
* `triggerReleaseClauseService`
* `signPreContractService`
* `renewPlayerContractService`
* `sellPlayerService`
* `loanInPlayerService`
* `loanOutPlayerService`

## Helpers privados a mover

* `debtRestrictionMessage`
* `totalNegotiationCommitment`
* `describeContractExtras`
* `eraseNamedSelection`
* `failureFromNegotiation`
* `transferWindowClosedFailure`
* `appendNegotiationMessages`
* `registerNegotiatedPromise`

## Fuera de alcance

No mover ni modificar:

* `changeYouthRegionService`
* `takeManagerJobService`
* `cyclePlayerDevelopmentPlanService`
* `cyclePlayerInstructionService`
* `talkToPlayerService`
* servicios de scouting
* servicios de reportes
* lógica de vestuario
* lógica de juveniles
* flujo general de carrera

## Restricciones

* No modificar firmas públicas.
* No modificar mensajes visibles.
* No modificar costos de transferencias.
* No modificar salarios, cláusulas, comisiones ni bonos.
* No modificar reglas de ventana de transferencias.
* No modificar restricciones de deuda.
* No modificar estructura de guardado.
* No modificar la lógica de negociación.
* No realizar limpiezas adicionales fuera del alcance.

## Estrategia

1. Agregar una prueba estructural que espere la existencia de `app_services_transfers.cpp`.
2. Confirmar RED.
3. Crear el nuevo módulo.
4. Agregarlo a `FM_CAREER_SOURCES`.
5. Mover helpers y servicios manteniendo el código sin cambios funcionales.
6. Confirmar compilación.
7. Confirmar GREEN.
8. Compilar GUI y CLI.
9. Ejecutar validación completa.
10. Registrar evidencia en `TODO.md`.

## Resultado esperado

`app_services.cpp` conserva las responsabilidades generales de carrera, mientras que las operaciones de mercado, contratos y préstamos quedan implementadas en `app_services_transfers.cpp`, sin cambios funcionales para el usuario.
