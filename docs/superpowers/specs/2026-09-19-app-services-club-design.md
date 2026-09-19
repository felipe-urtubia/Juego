# Diseño: separación de club y staff en App Services

## Objetivo

Reducir responsabilidades de `src/career/app_services.cpp` separando la gestión de mejoras del club y revisión de staff en un módulo independiente, sin modificar el comportamiento del juego ni la API pública.

## Nuevo módulo

Crear:

`src/career/app_services_club.cpp`

El archivo `include/career/app_services.h` se mantiene como fachada pública y conserva las firmas existentes.

## Servicios públicos a mover

* `upgradeClubService`
* `reviewStaffStructureService`

## Helpers privados a mover

* `nextStaffHireName`
* `upgradeCost`
* `upgradeLabel`
* `isFacilityUpgrade`
* `staffUpgradeForRole`

## Dependencia compartida

`syncInfrastructureFromTeam` también es utilizada por otros flujos de `app_services.cpp`.

Por lo tanto:

* su implementación original permanecerá en `app_services.cpp`;
* `app_services_club.cpp` tendrá una copia privada idéntica para mantener aislado el nuevo módulo;
* no se modificarán los otros consumidores de infraestructura.

El helper genérico `failure` también permanecerá en `app_services.cpp`, con una copia privada mínima en el nuevo módulo.

## Fuera de alcance

No mover ni modificar:

* `changeYouthRegionService`
* servicios de desarrollo de jugadores
* vestuario
* decisiones semanales
* scouting
* transferencias
* reportes
* carga/guardado de carrera
* otros flujos de infraestructura

## Restricciones

* No modificar firmas públicas.
* No modificar mensajes visibles.
* No modificar costos de mejoras.
* No modificar niveles o incrementos de staff.
* No modificar restricciones de deuda.
* No modificar cobertura regional de scouting.
* No modificar reglas de infraestructura.
* No modificar estructura de guardado.
* No realizar limpiezas adicionales fuera del alcance.

## Estrategia

1. Agregar prueba estructural para `app_services_club.cpp`.
2. Confirmar RED.
3. Crear el módulo mínimo.
4. Agregarlo a `FM_CAREER_SOURCES`.
5. Preparar dependencias privadas.
6. Mover helpers exclusivos.
7. Mover `upgradeClubService` y `reviewStaffStructureService`.
8. Confirmar que `syncInfrastructureFromTeam` original siga disponible para otros flujos.
9. Compilar y confirmar GREEN.
10. Compilar GUI y CLI.
11. Ejecutar validación completa.
12. Registrar evidencia en `TODO.md`.

## Resultado esperado

`app_services.cpp` conserva los flujos generales de carrera e infraestructura compartida, mientras que las mejoras del club y la revisión del staff quedan implementadas en `app_services_club.cpp` sin cambios funcionales.
