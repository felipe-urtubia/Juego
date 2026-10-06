# Arquitectura

## Descripción general

Chilean Footballito es un simulador de gestión futbolística desarrollado en C++17 con una arquitectura modular organizada principalmente bajo `src/` e `include/`.

El proyecto separa las principales responsabilidades en dominios independientes:

- Motor y estado general.
- Competiciones.
- Simulación de partidos.
- Inteligencia artificial.
- Modo Carrera.
- Desarrollo de jugadores.
- Finanzas.
- Transferencias.
- Persistencia.
- Interfaz.
- Validación.
- Utilidades compartidas.

La arquitectura continúa evolucionando desde módulos históricos de gran tamaño hacia servicios más pequeños, especializados y fáciles de probar.

---

# Capas principales

## Engine

Rutas principales:

- `include/engine/`
- `src/engine/`

Responsabilidades:

- Estado central del juego.
- Runtime general.
- Flujo principal de la aplicación.
- Controladores.
- Integración del estado de carrera.
- Coordinación de alto nivel entre sistemas.

El Engine funciona como punto de integración entre los distintos dominios del proyecto.

---

## Competition

Rutas principales:

- `include/competition/`
- `src/competition/`

Responsabilidades:

- Reglas de competición.
- Tablas de posiciones.
- Comportamiento de divisiones.
- Resolución de temporadas.
- Reglas específicas de liga y copa.
- Ascensos y descensos.

Este módulo intenta mantener las reglas competitivas separadas de la interfaz y de la simulación de partidos.

---

## Simulation

Rutas principales:

- `include/simulation/`
- `src/simulation/`

Representa el núcleo de la simulación deportiva.

Entre sus responsabilidades se encuentran:

- Match Engine.
- Match Context.
- Fases del partido.
- Generación de eventos.
- Resolución de acciones.
- Estadísticas.
- Fatiga.
- Moral.
- Lesiones.
- Tarjetas.
- Sustituciones.
- Momentum.
- Valoraciones individuales.
- Timeline.
- Runtime de partido en vivo.

La simulación devuelve datos estructurados y evita depender directamente de la presentación.

---

## Inteligencia Artificial

Rutas principales:

- `include/ai/`
- `src/ai/`

Responsabilidades:

- Decisiones tácticas.
- Gestión del partido.
- Sustituciones.
- Planificación de plantilla.
- Evaluación de fichajes.
- Adaptación al contexto del encuentro.

La IA utiliza información generada por el motor, como resultado, minuto, cansancio, tarjetas, Momentum y sustituciones disponibles.

---

## Career

Rutas principales:

- `include/career/`
- `src/career/`

Gestiona los principales sistemas del Modo Carrera.

Entre sus responsabilidades se encuentran:

- Progresión semanal.
- Temporadas.
- Calendario.
- Liga activa.
- Simulación de otras divisiones.
- Copa.
- Comunicaciones del staff.
- Noticias de plantilla.
- Narrativas.
- Eventos de carrera.
- Reputación del manager.
- Riesgo de despido.
- Servicios de carrera.

Una parte importante de la evolución reciente consiste en mover lógica desde grandes funciones de orquestación hacia `CareerService` y servicios especializados.

---

## Development

Rutas principales:

- `include/development/`
- `src/development/`

Responsabilidades:

- Entrenamiento.
- Progresión de jugadores.
- Desarrollo derivado del rendimiento.
- Desarrollo juvenil.
- Generación y evolución de talentos.

---

## Finance

Rutas principales:

- `include/finance/`
- `src/finance/`

Responsabilidades:

- Finanzas semanales.
- Ingresos.
- Gastos.
- Salarios.
- Proyecciones económicas.
- Flujo de caja.
- Cálculos relacionados con el salary cap.

La lógica financiera permanece separada de la GUI.

La interfaz consume los resultados de este módulo para mostrar alertas, indicadores y paneles financieros.

---

## Transfers

Rutas principales:

- `include/transfers/`
- `src/transfers/`

Responsabilidades:

- Mercado de fichajes.
- Evaluación de objetivos.
- Negociaciones.
- Ofertas.
- Valoraciones.
- Salarios.
- Roles prometidos.
- Estrategia de mercado de clubes controlados por IA.

---

## Input / Output

Rutas principales:

- `include/io/`
- `src/io/`

Responsabilidades:

- Carga de datos externos.
- Guardado de partidas.
- Carga de partidas.
- Serialización.
- Normalización de plantillas.
- Manejo de archivos.
- Manejo seguro de rutas.

La persistencia se mantiene separada de la lógica de presentación.

---

## Presentación

Rutas principales:

- `include/ui/`
- `src/ui/`
- `include/gui/`
- `src/gui/`

El proyecto dispone actualmente de dos frontends:

- Interfaz de consola.
- Interfaz gráfica Win32.

La interfaz debe limitarse principalmente a:

- Renderizar información.
- Capturar acciones del usuario.
- Navegar entre pantallas.
- Enviar decisiones a los servicios y runtimes correspondientes.

La lógica de negocio debe permanecer fuera de la GUI siempre que sea posible.

---

## Validators

Rutas principales:

- `include/validators/`
- `src/validators/`

Responsabilidades:

- Validación de ligas.
- Integridad de calendarios.
- Integridad de guardados.
- Validación de plantillas.
- Auditoría general de datos.

Este sistema también es utilizado por `FootballManagerCLI`.

---

## Utilities

Rutas principales:

- `include/utils/`
- `src/utils/`

Incluye utilidades compartidas para:

- Rutas.
- Parsing.
- Aleatoriedad.
- Conversión de datos.
- Funciones auxiliares de uso común.

---

# Flujo general de ejecución

Una representación simplificada del flujo principal es:

```text
GUI / CLI
   |
   v
Game Controller / Runtime de Carrera
   |
   +------> CareerService
   |
   +------> Competition
   |
   +------> Finance
   |
   +------> Transfers
   |
   +------> Development
   |
   +------> Match Engine
                 |
                 +--> Match Context
                 +--> Eventos
                 +--> Resolución
                 +--> Estadísticas
                 +--> Momentum
                 +--> Valoraciones
                 +--> IA táctica
                 |
                 v
          Live Match Runtime
                 ^
                 |
                 +---- Match Center Win32
```

La GUI no mantiene una segunda simulación independiente.

El Match Center observa y controla el mismo runtime utilizado por el motor de simulación.

---

# Match Center interactivo

Desde `v0.1.2.0-alpha`, el Match Center funciona como una interfaz interactiva conectada al runtime del partido.

Durante un encuentro el jugador puede:

- Cambiar mentalidad.
- Cambiar instrucciones tácticas.
- Aplicar mentalidad e instrucción en el mismo corte de decisión.
- Realizar sustituciones manuales.
- Pausar el partido.
- Reanudar el partido.
- Cambiar la velocidad entre `1x`, `2x` y `4x`.
- Confirmar cortes tácticos mediante `CONTINUAR`.

La IA también puede realizar sustituciones y esos cambios se propagan a través del mismo flujo de eventos y timeline.

Los cortes de decisión permanecen detenidos hasta que el jugador confirma explícitamente que la simulación puede continuar.

---

# Integración del runtime del partido

Flujo simplificado:

```text
Match Engine
    |
    v
Live Match Runtime
    |
    +------> Estado del partido
    |
    +------> Eventos
    |
    +------> Estadísticas
    |
    +------> Solicitud de decisión
                  |
                  v
            Match Center Win32
                  |
                  +--> Mentalidad
                  +--> Instrucción
                  +--> Sustitución
                  +--> Continuar
                  |
                  v
            Decisión del runtime
                  |
                  v
             Match Engine
```

El bridge entre GUI y simulación permite mantener separada la presentación de la lógica deportiva.

El worker de simulación revisa periódicamente:

- Estado de pausa.
- Estado de cancelación.
- Velocidad de reproducción.

Esto permite que la interfaz permanezca responsive sin duplicar el estado del partido.

---

# Sustituciones

El sistema de sustituciones permite realizar cambios durante el encuentro.

La interfaz puede:

- Seleccionar al jugador que sale.
- Seleccionar al jugador que entra.
- Consultar posición.
- Consultar estado físico.
- Consultar media.
- Validar disponibilidad.
- Respetar el límite de sustituciones.
- Actualizar XI y banca.
- Registrar el cambio en el timeline.

Las sustituciones realizadas por equipos controlados por IA también se muestran en el Match Center.

---

# Decisiones tácticas

Las decisiones tácticas se realizan mediante estructuras explícitas enviadas al runtime.

Actualmente pueden incluir:

- Cambio de mentalidad.
- Cambio de instrucción.
- Cambio conjunto de mentalidad e instrucción.

Las instrucciones incompatibles se muestran deshabilitadas.

Cuando una nueva mentalidad invalida la instrucción anterior, el sistema utiliza una alternativa coherente.

Este diseño evita que la interfaz modifique directamente estados internos de la simulación sin validación.

---

# Dirección de CareerService

Uno de los principales objetivos recientes del proyecto ha sido reducir la lógica concentrada en grandes flujos de carrera.

La migración hacia `CareerService` y servicios relacionados incluye lógica de:

- Finanzas semanales.
- Contratos.
- Ofertas de transferencias.
- Actualizaciones físicas.
- Simulación de divisiones.
- Liga activa.
- Copa.
- Comunicaciones del staff.
- Noticias de plantilla.
- Narrativas semanales.
- Eventos de carrera.
- Reputación del manager.
- Despido del manager.

El objetivo es que GUI y CLI consuman APIs estructuradas mientras las reglas de negocio permanecen en servicios testeables.

---

# Finanzas y salary cap

El sistema económico se mantiene separado de la presentación.

Incluye:

- Ingresos.
- Gastos.
- Salarios.
- Flujo de caja.
- Proyecciones.
- Masa salarial.
- Salary cap relativo a los ingresos del club.

La GUI utiliza estos datos para mostrar:

- KPIs.
- Alertas.
- Porcentaje de uso.
- Riesgos económicos.
- Información financiera del club.

---

# Persistencia

La persistencia del Modo Carrera se organiza en módulos separados.

Incluye:

- Guardado.
- Carga.
- Serialización.
- Validación posterior a carga.
- Manejo de rutas.
- Normalización de datos.

Los archivos de usuario permanecen bajo `saves/`.

Los datos del juego permanecen bajo `data/`.

---

# Principios de diseño

La arquitectura actual sigue estos principios:

- Separación de responsabilidades.
- Presentación separada de lógica de negocio.
- Simulación basada en datos estructurados.
- Servicios de dominio testeables.
- Interfaces explícitas entre GUI y runtime.
- Reducción progresiva de módulos monolíticos.
- Validación de accesos.
- Reutilización de lógica.
- Compatibilidad con frontend gráfico y CLI.
- Pruebas de regresión para cambios relevantes.
- Build fuera de las carpetas de código fuente.
- Releases públicas sin archivos de desarrollo innecesarios.

---

# Compilación

Los principales targets del proyecto son:

- `FootballManager`
- `FootballManagerCLI`
- `FootballManagerTests`

Build utilizado durante desarrollo y validación:

```powershell
cmake --build .\build-ci
```

---

# Pruebas

La suite automatizada se ejecuta mediante:

```powershell
ctest --test-dir .\build-ci --output-on-failure
```

Para `v0.1.2.0-alpha`:

- 100% de los tests configurados aprobaron.

La cobertura incluye, entre otras áreas:

- Motor de simulación.
- Match Center.
- Runtime interactivo.
- Sustituciones manuales.
- Sustituciones de IA.
- Decisiones tácticas combinadas.
- Persistencia.
- Economía.
- Calendario.
- Desarrollo.
- IA.
- Servicios de carrera.

---

# Validación general

La CLI permite ejecutar una auditoría de datos:

```powershell
.\build-ci\bin\FootballManagerCLI.exe --validate
```

Resultado verificado para `v0.1.2.0-alpha`:

```text
Divisiones: 5
Equipos revisados: 90
Jugadores crudos: 2200
Errores: 0
Advertencias: 0
Resultado: sin fallas
```

---

# Estado actual de la migración

La base de código ya utiliza principalmente `src/` e `include/`.

Entre los avances arquitectónicos se encuentran:

- Separación del motor en sistemas especializados.
- Uso de estructuras de resultado del partido.
- Match Context dedicado.
- Eventos y estadísticas separados.
- Runtime interactivo conectado al Match Center.
- Sistemas específicos de desarrollo.
- Sistemas específicos de finanzas.
- Serialización separada.
- Estado de carrera separado de la GUI.
- Migración progresiva hacia CareerService.
- Servicios independientes para distintos flujos semanales.

---

# Trabajo arquitectónico pendiente

La migración todavía continúa.

Entre las prioridades futuras se encuentran:

- Reducir módulos de simulación demasiado grandes.
- Reducir módulos de GUI demasiado grandes.
- Continuar separando la orquestación del Modo Carrera.
- Eliminar compatibilidad transitoria cuando sea seguro.
- Aumentar cobertura de regresión del Match Center.
- Añadir más smoke tests de GUI.
- Añadir simulaciones de larga duración.
- Mejorar pruebas de balance.
- Reemplazar patrones legacy inseguros.
- Utilizar identificadores y accesos validados cuando corresponda.
- Mantener sincronizada la documentación con cada release.

---

# Versión documentada

Este documento refleja el estado general del proyecto correspondiente a:

**Chilean Footballito `v0.1.2.0-alpha`**
