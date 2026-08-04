# ⚽ Football Manager Game

<p align="center">

# Football Manager Game

### Simulador de Gestión Futbolística desarrollado en C++17

Motor de simulación propio • Arquitectura modular • IA táctica • Match Center • Modo Carrera

</p>

---

## 📖 Descripción

**Football Manager Game** es un simulador de gestión futbolística desarrollado completamente en **C++17** cuyo objetivo es recrear la experiencia de dirigir un club profesional mediante un motor de simulación propio, una arquitectura modular y un conjunto de sistemas interconectados que permiten gestionar todos los aspectos deportivos e institucionales de un equipo.

A diferencia de otros proyectos académicos centrados únicamente en la simulación de partidos, este proyecto busca representar el funcionamiento completo de un club de fútbol moderno, integrando la administración deportiva, el desarrollo de jugadores, la planificación de temporadas, la inteligencia artificial táctica y la evolución del mundo futbolístico dentro de un mismo ecosistema.

El proyecto ha sido diseñado utilizando una arquitectura modular que facilita el mantenimiento, la escalabilidad y la incorporación de nuevas funcionalidades sin afectar el resto del sistema.

---

# 🎯 Objetivos del Proyecto

Football Manager Game tiene como objetivo desarrollar un simulador completo de gestión futbolística que combine simulación deportiva, administración de clubes e inteligencia artificial.

Entre sus principales objetivos se encuentran:

- Crear un motor de simulación de partidos propio.
- Implementar un Modo Carrera completamente funcional.
- Simular temporadas completas.
- Gestionar plantillas profesionales.
- Administrar presupuestos y finanzas.
- Simular un mercado de fichajes dinámico.
- Desarrollar una IA capaz de tomar decisiones tácticas durante los partidos.
- Mantener una arquitectura modular y fácilmente extensible.
- Incorporar pruebas automatizadas para garantizar la estabilidad del proyecto.

---

# ⭐ Características principales

Actualmente el proyecto incorpora sistemas para:

## 🏟️ Gestión del Club

- Gestión de plantillas
- Desarrollo de jugadores
- Mercado de fichajes
- Finanzas
- Objetivos de la directiva
- Desarrollo juvenil
- Instalaciones
- Noticias
- Historial de temporadas
- Informes deportivos

---

## ⚽ Motor de Simulación

El motor de simulación ha sido desarrollado específicamente para este proyecto e incorpora diferentes subsistemas encargados de representar el desarrollo de un encuentro.

Entre ellos destacan:

- Simulación minuto a minuto
- Eventos dinámicos
- Resolución de acciones
- Estadísticas del encuentro
- Sistema de fatiga
- Moral de jugadores
- Lesiones
- Tarjetas
- Sustituciones
- Cambios tácticos
- Momentum del partido
- Valoraciones individuales

---

## 📺 Match Center

Durante la simulación el jugador puede seguir el desarrollo del encuentro mediante un Match Center que muestra información en tiempo real.

Incluye:

- Marcador
- Cronómetro
- Timeline de eventos
- Estadísticas
- Posesión
- Momentum
- Comentarios dinámicos
- Cambios tácticos
- Sustituciones
- Valoraciones en vivo

---

## 🧠 Inteligencia Artificial

El proyecto incorpora diversos sistemas de IA especializados.

Entre ellos:

- IA táctica
- IA de gestión de plantilla
- IA de fichajes
- IA para sustituciones
- IA de cambios tácticos
- IA basada en Momentum
- Personalidad de equipos

Cada entrenador puede modificar el comportamiento de su equipo dependiendo del contexto del partido.

---

## 📈 Estadísticas

El motor registra información detallada de cada encuentro.

Por ejemplo:

- Goles
- Asistencias
- Tiros
- Tiros al arco
- Posesión
- Faltas
- Tarjetas
- xG
- Momentum
- Cambios tácticos
- Sustituciones
- Valoraciones

---

# 🏗 Filosofía del Proyecto

Football Manager Game ha sido diseñado siguiendo varios principios fundamentales.

## Arquitectura Modular

Cada sistema del juego se encuentra separado en módulos independientes con responsabilidades claramente definidas.

Esto permite:

- facilitar el mantenimiento;
- reducir el acoplamiento entre componentes;
- incorporar nuevas funcionalidades sin modificar grandes partes del código;
- simplificar las pruebas.

---

## Escalabilidad

El proyecto está preparado para seguir creciendo mediante la incorporación de nuevos módulos y sistemas sin necesidad de rediseñar la arquitectura existente.

---

## Mantenibilidad

La organización del código busca favorecer la legibilidad, la reutilización y la separación de responsabilidades.

---

## Calidad

Cada nueva funcionalidad busca incorporarse junto con pruebas y documentación para mantener la estabilidad del proyecto.

---

# 🚀 Estado actual del proyecto

Actualmente Football Manager Game se encuentra en desarrollo activo.

Entre los sistemas disponibles destacan:

✅ Motor de simulación

✅ Modo Carrera

✅ Match Center

✅ IA táctica

✅ Mercado de fichajes

✅ Finanzas

✅ Desarrollo juvenil

✅ Desarrollo de jugadores

✅ Sistema de noticias

✅ Estadísticas

✅ Sistema de Momentum

✅ Valoraciones dinámicas

✅ Pruebas automatizadas

---

# 💻 Tecnologías utilizadas

El proyecto utiliza principalmente:

| Tecnología | Uso |
|------------|-------------------------------|
| C++17 | Lenguaje principal |
| STL | Estructuras de datos |
| CMake | Sistema de compilación |
| Ninja | Build System |
| MSYS2 UCRT64 | Toolchain |
| GCC | Compilador |
| Git | Control de versiones |
| GitHub | Repositorio |

---

# 📑 Índice

1. Introducción
2. Arquitectura del Proyecto
3. Organización del Código
4. Motor de Simulación
5. Match Center
6. Inteligencia Artificial
7. Modo Carrera
8. Mercado de Fichajes
9. Finanzas
10. Desarrollo Juvenil
11. Sistema de Tests
12. Compilación
13. Roadmap
14. Documentación
15. Contribuciones
16. Licencia


# 🏗 Arquitectura del Proyecto

Football Manager Game ha sido diseñado siguiendo una arquitectura modular donde cada sistema posee responsabilidades claramente definidas.

En lugar de concentrar toda la lógica del juego en unos pocos archivos de gran tamaño, el proyecto divide cada área funcional en módulos independientes que interactúan entre sí mediante interfaces bien definidas.

Esta organización permite:

- Facilitar el mantenimiento del código.
- Reducir el acoplamiento entre sistemas.
- Reutilizar componentes.
- Agregar nuevas funcionalidades sin modificar el resto del proyecto.
- Mejorar la cobertura de pruebas.
- Escalar el proyecto conforme aumenta su complejidad.

---

# 📁 Organización General

El proyecto está organizado utilizando una estructura modular.

```text
FootballManagerGame/

│
├── assets/
├── data/
├── docs/
├── include/
├── mods/
├── saves/
├── src/
├── tests/
├── tools/
│
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── TODO.md
├── CHANGELOG.md
└── ...
```

Cada carpeta tiene una responsabilidad específica dentro del proyecto.

---

# 📂 Estructura de Carpetas

## src/

Contiene prácticamente toda la implementación del juego.

Aquí se encuentran los distintos módulos que conforman Football Manager Game.

```text
src/

ai/

career/

competition/

development/

engine/

finance/

gui/

io/

simulation/

transfers/

ui/

utils/

validators/
```

---

## include/

Contiene los archivos de cabecera del proyecto.

Aquí se definen las clases, estructuras e interfaces utilizadas por los distintos módulos.

Separar las declaraciones de las implementaciones permite mantener una organización más limpia y facilita la reutilización del código.

---

## tests/

Contiene las pruebas automatizadas del proyecto.

El objetivo es validar el correcto funcionamiento de los diferentes sistemas del juego.

Las pruebas permiten detectar regresiones durante el desarrollo y garantizan que nuevas funcionalidades no rompan el comportamiento existente.

---

## assets/

Almacena recursos utilizados por el juego.

Por ejemplo:

- imágenes
- logotipos
- iconos
- sonidos
- recursos gráficos

---

## data/

Contiene información utilizada por el motor.

Dependiendo del sistema puede incluir:

- ligas
- clubes
- jugadores
- configuraciones
- bases de datos
- parámetros de simulación

---

## docs/

Documentación técnica del proyecto.

Aquí se almacenará toda la documentación interna para desarrolladores.

Entre ella:

- arquitectura
- motor de simulación
- inteligencia artificial
- modo carrera
- guía de compilación
- sistema de pruebas

---

## saves/

Almacena las partidas guardadas generadas por el usuario.

---

## tools/

Herramientas auxiliares utilizadas durante el desarrollo.

---

# 🧩 Organización por Módulos

Cada módulo posee una responsabilidad concreta.

Esto evita mezclar lógica de distintas áreas dentro del mismo archivo.

---

## 🎮 Engine

El módulo **Engine** actúa como núcleo del juego.

Es responsable de coordinar el funcionamiento general de Football Manager Game.

Entre sus responsabilidades se encuentran:

- inicialización
- control del flujo principal
- menús
- navegación
- tablas
- sistemas sociales
- configuración

El resto de módulos dependen del Engine para integrarse correctamente dentro del flujo del juego.

---

## 🏟 Career

El módulo **Career** administra el modo carrera.

Gestiona:

- temporadas
- calendario
- progresión
- informes
- noticias
- bandeja de entrada
- desarrollo semanal
- objetivos
- evolución del club

Es uno de los módulos más grandes del proyecto.

---

## ⚽ Simulation

Este módulo representa el corazón del juego.

Aquí se encuentra el motor encargado de simular cada partido.

Incluye diferentes sistemas especializados para resolver cada aspecto del encuentro.

Entre ellos:

- Match Engine
- Match Context
- Match Phase
- Match Events
- Match Statistics
- Match Center
- Match Renderer
- Match Resolution
- Momentum
- Valoraciones

Toda la simulación deportiva ocurre dentro de este módulo.

---

## 🧠 AI

El módulo de Inteligencia Artificial contiene los algoritmos que permiten que los equipos controlados por la CPU tomen decisiones de forma autónoma.

Actualmente incluye sistemas para:

- planificación de plantillas
- decisiones tácticas
- sustituciones
- gestión del partido
- mercado de fichajes

La IA utiliza el contexto del encuentro para adaptar su comportamiento durante la simulación.

---

## 💰 Finance

Administra toda la economía del club.

Entre sus funciones:

- presupuesto
- ingresos
- gastos
- salarios
- situación financiera

---

## 🔄 Transfers

Gestiona el mercado de fichajes.

Incluye:

- negociaciones
- ofertas
- compras
- ventas
- movimientos de jugadores

---

## 📈 Development

Responsable de la evolución de los jugadores.

Administra:

- progresión
- entrenamiento
- potencial
- crecimiento
- desarrollo juvenil

---

## 🖥 UI

Contiene la interfaz del juego.

Gestiona:

- pantallas
- menús
- navegación
- informes
- visualización de información

---

## 💾 IO

Responsable del sistema de persistencia.

Gestiona:

- guardado
- carga
- serialización
- almacenamiento de partidas

---

## ✔ Validators

Incluye diferentes rutinas de validación utilizadas por el proyecto para verificar la consistencia de los datos.

---

## 🛠 Utils

Agrupa funciones reutilizables utilizadas por múltiples módulos.

Por ejemplo:

- utilidades generales
- registro de eventos
- localización
- herramientas de soporte

---

# 🔄 Flujo General del Juego

El funcionamiento del proyecto puede resumirse mediante el siguiente flujo.

```text
Inicio del Juego

        │

        ▼

Game Controller

        │

        ▼

Modo Carrera

        │

        ▼

Simulación Semanal

        │

        ▼

Preparación del Partido

        │

        ▼

Match Engine

        │

        ▼

Match Center

        │

        ▼

Resultados

        │

        ▼

Actualización del Mundo

        │

        ▼

Guardar Progreso
```

Este flujo representa el ciclo principal que sigue el jugador durante una temporada.

Cada módulo interviene únicamente cuando corresponde, evitando dependencias innecesarias y manteniendo una arquitectura limpia.

---

# 🎯 Filosofía de Diseño

El desarrollo de Football Manager Game se basa en cuatro principios fundamentales.

## Modularidad

Cada sistema debe cumplir una única responsabilidad.

---

## Escalabilidad

Las nuevas funcionalidades deben poder incorporarse sin rediseñar el proyecto.

---

## Mantenibilidad

La organización del código busca facilitar futuras modificaciones y reducir el riesgo de errores.

---

## Reutilización

Siempre que sea posible, los componentes se diseñan para ser reutilizados por otros módulos del proyecto.

---

Con esta arquitectura el proyecto puede seguir creciendo mediante la incorporación de nuevos sistemas sin comprometer la estabilidad del código existente.


# ⚽ Motor de Simulación

El Motor de Simulación constituye el núcleo de Football Manager Game y es el encargado de transformar el estado de dos equipos en el desarrollo completo de un partido.

A diferencia de un sistema basado únicamente en probabilidades, el motor utiliza un conjunto de módulos especializados que colaboran entre sí para representar el comportamiento de un encuentro de fútbol de forma coherente y extensible.

Su arquitectura modular permite añadir nuevas mecánicas sin modificar la estructura principal del sistema.

---

# Objetivos del Motor

El motor ha sido diseñado con los siguientes objetivos:

- Simular partidos completos.
- Mantener consistencia estadística.
- Permitir que la IA intervenga durante el encuentro.
- Generar eventos dinámicos.
- Registrar estadísticas.
- Alimentar el Match Center.
- Producir información para el Modo Carrera.

---

# Flujo General de un Partido

Todo encuentro sigue un flujo similar al siguiente:

```text
Crear Contexto del Partido

            │

            ▼

Inicializar Equipos

            │

            ▼

Preparar Estado Inicial

            │

            ▼

Iniciar Simulación

            │

            ▼

Resolver Minuto

            │

            ▼

Actualizar Momentum

            │

            ▼

Generar Eventos

            │

            ▼

Actualizar Estadísticas

            │

            ▼

Ejecutar IA Táctica

            │

            ▼

Actualizar Match Center

            │

            ▼

Finalizar Partido

            │

            ▼

Generar Reporte Final
```

---

# Componentes Principales

El motor está compuesto por varios subsistemas especializados.

---

## Match Engine

Es el componente encargado de coordinar toda la simulación.

Entre sus responsabilidades destacan:

- controlar el flujo del partido;
- avanzar el reloj;
- coordinar los módulos de simulación;
- actualizar el estado del encuentro;
- comunicar cambios al Match Center.

Actúa como el punto central desde el que se orquesta toda la simulación.

---

## Match Context

Representa el estado actual del partido.

Incluye información como:

- marcador;
- minuto;
- posesión;
- fatiga;
- moral;
- sustituciones;
- lesiones;
- tarjetas;
- estadísticas;
- contexto táctico.

Todos los módulos consultan este contexto para tomar decisiones.

---

## Match Events

Durante cada iteración del motor pueden generarse diferentes eventos.

Por ejemplo:

- goles;
- tiros;
- córners;
- faltas;
- tarjetas;
- lesiones;
- cambios;
- fueras de juego;
- oportunidades de gol.

Cada evento modifica el estado del encuentro y puede desencadenar nuevas acciones.

---

## Match Resolution

Este sistema determina el resultado de cada acción del partido.

Entre otras tareas:

- resolver disparos;
- calcular probabilidades;
- decidir recuperaciones;
- validar acciones ofensivas;
- actualizar el marcador.

---

## Match Statistics

Registra toda la información estadística producida durante el encuentro.

Entre las estadísticas almacenadas se encuentran:

- tiros;
- tiros al arco;
- goles;
- posesión;
- faltas;
- tarjetas;
- córners;
- xG;
- asistencias;
- recuperaciones;
- pérdidas.

Estas estadísticas alimentan tanto el Match Center como los informes posteriores.

---

## Match Momentum

Uno de los sistemas más importantes del proyecto.

El objetivo del Momentum es representar cuál de los dos equipos domina el desarrollo del partido.

No depende únicamente del marcador.

También considera diferentes factores generados durante la simulación.

Por ejemplo:

- ocasiones creadas;
- presión ofensiva;
- posesión;
- tiros;
- ritmo del encuentro;
- secuencia reciente de eventos.

Este valor es utilizado posteriormente por la Inteligencia Artificial para modificar el comportamiento táctico de cada entrenador.

---

## Player Ratings

Cada jugador recibe una valoración dinámica durante el partido.

La puntuación evoluciona dependiendo de:

- goles;
- asistencias;
- recuperaciones;
- pérdidas;
- precisión de pase;
- acciones defensivas;
- influencia ofensiva;
- errores cometidos.

Estas valoraciones son utilizadas posteriormente para determinar:

- mejor jugador;
- rendimiento individual;
- informes;
- análisis posteriores.

---

# Sistema de Eventos

El motor trabaja continuamente generando eventos.

Ejemplo simplificado.

```text
Minuto

↓

Calcular contexto

↓

Actualizar Momentum

↓

Generar oportunidades

↓

Resolver acciones

↓

Actualizar estadísticas

↓

Actualizar valoraciones

↓

Notificar Match Center
```

Este proceso se repite continuamente hasta finalizar el encuentro.

---

# Integración con la IA

Uno de los aspectos más importantes del motor consiste en la comunicación con la Inteligencia Artificial.

Durante el encuentro la IA recibe información relacionada con:

- resultado;
- minuto;
- cansancio;
- tarjetas;
- marcador;
- número de sustituciones;
- Momentum.

Con esta información cada entrenador puede:

- modificar la presión;
- cambiar el ritmo;
- alterar la formación;
- realizar sustituciones;
- cambiar instrucciones tácticas.

Esto permite que cada partido evolucione de forma dinámica.

---

# Integración con Match Center

Toda la información generada por el motor es enviada al Match Center.

Entre ella:

- marcador;
- cronómetro;
- estadísticas;
- comentarios;
- Momentum;
- sustituciones;
- cambios tácticos;
- timeline;
- valoraciones.

De esta manera el jugador puede seguir el desarrollo del encuentro en tiempo real.

---

# Integración con el Modo Carrera

Al finalizar un partido el motor entrega información a otros módulos del proyecto.

Entre ellos:

- calendario;
- clasificación;
- estadísticas de temporada;
- desarrollo de jugadores;
- moral;
- informes;
- noticias;
- historial del club.

Esto permite que cada encuentro tenga consecuencias dentro del Modo Carrera.

---

# Diseño Modular

El Motor de Simulación ha sido construido siguiendo una arquitectura desacoplada.

```text
Match Engine

      │

      ├────────► Match Context

      ├────────► Match Events

      ├────────► Match Resolution

      ├────────► Match Statistics

      ├────────► Match Momentum

      ├────────► Player Ratings

      ├────────► Tactical AI

      └────────► Match Center
```

Cada módulo posee una responsabilidad específica, reduciendo el acoplamiento y facilitando la incorporación de nuevas funcionalidades.

---

# Ventajas de esta Arquitectura

La organización modular ofrece múltiples beneficios:

- Mayor mantenibilidad.
- Código más limpio.
- Separación de responsabilidades.
- Escalabilidad.
- Facilidad para realizar pruebas.
- Incorporación de nuevos sistemas sin modificar el núcleo del motor.

Esta arquitectura permite que Football Manager Game continúe creciendo sin necesidad de rediseñar completamente el sistema de simulación.


# 🏆 Modo Carrera

El Modo Carrera representa el núcleo de la experiencia de Football Manager Game.

Su objetivo es permitir que el jugador gestione un club de fútbol a largo plazo, tomando decisiones deportivas, económicas y estratégicas que afectan directamente la evolución de la institución.

Cada temporada genera nuevos desafíos, oportunidades y consecuencias derivadas de las decisiones tomadas por el jugador.

---

# Objetivos del Modo Carrera

El sistema de carrera busca simular el funcionamiento de un club profesional mediante múltiples sistemas que trabajan de forma integrada.

Entre ellos:

- Gestión deportiva.
- Administración económica.
- Planificación de plantillas.
- Desarrollo juvenil.
- Evolución de jugadores.
- Objetivos institucionales.
- Calendario competitivo.
- Mercado de fichajes.
- Informes técnicos.
- Estadísticas históricas.

---

# Flujo General

El ciclo del Modo Carrera puede resumirse de la siguiente forma.

```text
Nueva Temporada

        │

        ▼

Calendario

        │

        ▼

Entrenamientos

        │

        ▼

Preparación del Partido

        │

        ▼

Simulación

        │

        ▼

Actualización del Club

        │

        ▼

Noticias

        │

        ▼

Mercado

        │

        ▼

Nueva Semana
```

---

# Gestión del Club

El jugador controla prácticamente todos los aspectos deportivos del equipo.

Entre ellos:

- Plantilla.
- Formación.
- Tácticas.
- Convocatorias.
- Presupuesto.
- Objetivos.
- Desarrollo de jugadores.
- Mercado de fichajes.

Cada decisión repercute en el rendimiento deportivo y económico del club.

---

# Calendario

El calendario organiza toda la temporada.

Incluye:

- Jornadas de liga.
- Copas.
- Fechas internacionales.
- Entrenamientos.
- Descanso.
- Mercado de fichajes.

El avance semanal constituye el eje principal del modo carrera.

---

# Desarrollo de Jugadores

Cada futbolista evoluciona con el paso del tiempo.

Su crecimiento depende de diferentes factores como:

- Edad.
- Potencial.
- Rendimiento.
- Minutos disputados.
- Entrenamiento.
- Estado físico.
- Moral.

Esto permite construir proyectos deportivos a largo plazo.

---

# Desarrollo Juvenil

El sistema juvenil permite incorporar nuevos talentos al club.

Su finalidad es ofrecer una fuente constante de jugadores jóvenes capaces de integrarse progresivamente al primer equipo.

Este sistema favorece la planificación a largo plazo y la sostenibilidad deportiva.

---

# Finanzas

La economía constituye uno de los pilares del Modo Carrera.

El sistema financiero administra:

- Presupuesto.
- Salarios.
- Ingresos.
- Gastos.
- Balance del club.
- Situación económica.

La gestión responsable de estos recursos resulta fundamental para garantizar la estabilidad del proyecto deportivo.

---

# Mercado de Fichajes

Durante la temporada el jugador puede reforzar su plantilla mediante el mercado de transferencias.

El sistema contempla procesos como:

- Compra de jugadores.
- Venta de futbolistas.
- Negociaciones.
- Valoración económica.
- Incorporación de nuevos talentos.

Las decisiones realizadas durante el mercado condicionan el rendimiento del equipo en las competiciones futuras.

---

# Informes

El Modo Carrera genera distintos informes destinados a facilitar la toma de decisiones.

Entre ellos pueden encontrarse:

- Informes deportivos.
- Informes económicos.
- Estado de la plantilla.
- Rendimiento de jugadores.
- Evolución de la temporada.

Estos informes permiten disponer de una visión global de la situación del club.

---

# Inteligencia Artificial

Football Manager Game incorpora diferentes sistemas de Inteligencia Artificial especializados.

En lugar de concentrar toda la lógica en un único componente, el proyecto divide el comportamiento automático en módulos independientes.

Esto permite ampliar las capacidades de la IA sin modificar el resto del motor.

---

# Objetivos de la IA

La Inteligencia Artificial busca representar el comportamiento de entrenadores y clubes controlados por la CPU.

Entre sus objetivos destacan:

- Gestionar plantillas.
- Realizar sustituciones.
- Adaptar tácticas.
- Gestionar fichajes.
- Reaccionar al desarrollo del partido.
- Mantener coherencia deportiva.

---

# IA Táctica

Durante cada encuentro la IA analiza continuamente el contexto del partido.

Entre las variables consideradas destacan:

- Resultado.
- Minuto.
- Estado físico.
- Tarjetas.
- Diferencia de goles.
- Momentum.
- Cambios disponibles.

A partir de esta información puede modificar el planteamiento táctico del equipo.

---

# IA de Sustituciones

La IA decide cuándo realizar cambios de jugadores considerando factores como:

- Fatiga.
- Rendimiento.
- Lesiones.
- Tarjetas.
- Situación táctica.
- Resultado.

El objetivo es mantener la competitividad del equipo durante todo el encuentro.

---

# IA basada en Momentum

Uno de los desarrollos más recientes del proyecto consiste en integrar el sistema de Momentum dentro de la toma de decisiones de la IA.

El comportamiento del entrenador puede modificarse dependiendo del dominio mostrado durante el encuentro.

Cuando un equipo atraviesa una fase favorable puede aumentar la presión ofensiva o asumir mayores riesgos.

Por el contrario, si pierde el control del partido puede optar por reorganizar su estructura táctica o introducir cambios para recuperar la iniciativa.

Esta integración permite que el desarrollo de los partidos resulte más dinámico y menos predecible.

---

# Diseño Modular de la IA

```text
AI

│

├── Gestión de Plantilla

├── Gestión del Partido

├── Cambios Tácticos

├── Sustituciones

├── Mercado de Fichajes

└── Momentum
```

Cada componente posee responsabilidades claramente definidas y puede evolucionar de forma independiente.

---

# Integración con el resto del Proyecto

La Inteligencia Artificial mantiene comunicación con distintos módulos.

```text
Match Engine

        │

        ▼

Momentum

        │

        ▼

IA Táctica

        │

        ▼

Cambios

        │

        ▼

Match Center

        │

        ▼

Modo Carrera
```

Esta comunicación permite que las decisiones tomadas durante los partidos tengan repercusiones dentro del Modo Carrera y contribuyan a la evolución general del mundo del juego.

---

# Filosofía de Diseño

El diseño del Modo Carrera y de la Inteligencia Artificial persigue tres objetivos fundamentales:

- Simular decisiones coherentes.
- Favorecer la planificación a largo plazo.
- Mantener una experiencia dinámica y variada en cada temporada.

Gracias a esta arquitectura modular, el proyecto puede seguir incorporando nuevas funcionalidades sin comprometer la estabilidad del resto de sistemas.


# 🔨 Compilación del Proyecto

Football Manager Game utiliza **CMake** como sistema de generación de proyectos y está preparado para compilarse utilizando distintos entornos de desarrollo.

Actualmente el entorno recomendado para Windows es:

- MSYS2 UCRT64
- GCC
- Ninja
- CMake 3.16 o superior

Esta configuración permite obtener compilaciones rápidas, reproducibles y compatibles con el resto del proyecto.

---

# Requisitos

Antes de compilar el proyecto es necesario disponer de las siguientes herramientas.

| Herramienta | Versión recomendada |
|------------|---------------------|
| CMake | 3.16 o superior |
| GCC | Compatible con C++17 |
| Ninja | Última versión estable |
| Git | Última versión |
| MSYS2 UCRT64 | Recomendado para Windows |

---

# Obtener el Proyecto

```bash
git clone https://github.com/TU_USUARIO/FootballManagerGame.git

cd FootballManagerGame
```

---

# Configuración

El proyecto utiliza **CMake Presets** para simplificar el proceso de configuración.

```bash
cmake --preset Juego-UCRT64-Ninja
```

Este comando genera automáticamente la carpeta de compilación utilizando la configuración definida en `CMakePresets.json`.

---

# Compilación

Una vez configurado el proyecto basta con ejecutar:

```bash
cmake --build out/build
```

El sistema compilará todos los ejecutables configurados por CMake.

---

# Ejecutables

El proyecto genera diferentes ejecutables dependiendo de la configuración.

Entre ellos pueden encontrarse:

- FootballManager
- FootballManagerCLI
- FootballManagerTests

Cada uno cumple una función específica dentro del proyecto.

---

# Sistema de Pruebas

Uno de los objetivos principales del proyecto consiste en mantener un alto nivel de estabilidad.

Para ello Football Manager Game incorpora una colección de pruebas automatizadas que permiten detectar regresiones durante el desarrollo.

---

# Ejecutar las pruebas

Una vez compilado el proyecto es posible ejecutar todas las pruebas mediante:

```bash
ctest --test-dir out/build --output-on-failure
```

También es posible ejecutar directamente el ejecutable de pruebas.

---

# Cobertura de pruebas

Las pruebas verifican distintos módulos del proyecto.

Entre ellos:

- Motor de simulación.
- Match Center.
- IA táctica.
- Sistema de validaciones.
- Persistencia.
- Calendario.
- Economía.
- Desarrollo de jugadores.
- Momentum.
- Valoraciones.
- Componentes auxiliares.

El objetivo es detectar errores antes de incorporar nuevas funcionalidades.

---

# Filosofía de Desarrollo

Cada nueva característica debería seguir el siguiente flujo de trabajo:

```text
Implementar

      │

      ▼

Compilar

      │

      ▼

Ejecutar pruebas

      │

      ▼

Corregir errores

      │

      ▼

Actualizar documentación

      │

      ▼

Realizar commit

      │

      ▼

Publicar cambios
```

---

# Organización del Código

El proyecto intenta mantener una separación clara entre responsabilidades.

Como norma general:

- La lógica del juego pertenece a `src/`.
- Las declaraciones públicas se mantienen en `include/`.
- Las pruebas se almacenan en `tests/`.
- La documentación técnica se encuentra en `docs/`.
- Los recursos del juego permanecen en `assets/`.
- Los datos del proyecto se almacenan en `data/`.

Esta organización facilita el mantenimiento conforme aumenta el tamaño del proyecto.

---

# Convenciones del Proyecto

Durante el desarrollo se recomienda seguir las siguientes prácticas.

## Código

- Mantener funciones con una única responsabilidad.
- Evitar duplicación de lógica.
- Documentar algoritmos complejos.
- Favorecer la modularidad.

---

## Nuevos módulos

Cuando se incorpora un nuevo sistema es recomendable:

- crear un módulo independiente;
- añadir sus cabeceras correspondientes;
- registrar las nuevas fuentes en CMake;
- incorporar pruebas automatizadas;
- documentar el funcionamiento del sistema.

---

## Commits

Se recomienda utilizar mensajes descriptivos.

Ejemplos:

```text
feat(match): agregar Match Center

feat(ai): integrar IA basada en momentum

fix(simulation): corregir cálculo de estadísticas

refactor(engine): reorganizar flujo de simulación

docs(readme): actualizar documentación del proyecto
```

---

# Roadmap

Football Manager Game continúa evolucionando de forma activa.

Entre las funcionalidades previstas para futuras versiones destacan:

## Motor de Simulación

- Repeticiones de jugadas.
- Sistema de highlights.
- Clima dinámico.
- Árbitros con comportamiento propio.
- VAR.
- Eventos especiales.

---

## Inteligencia Artificial

- Entrenadores con personalidad.
- Adaptación táctica avanzada.
- Aprendizaje basado en temporadas.
- Estrategias específicas según competición.

---

## Modo Carrera

- Conferencias de prensa.
- Patrocinadores.
- Relaciones con la directiva.
- Historial completo de entrenadores.
- Academia juvenil ampliada.

---

## Interfaz

- Mejoras visuales del Match Center.
- Paneles configurables.
- Más estadísticas en tiempo real.
- Comparativas entre jugadores.

---

# Documentación

Además de este README, el proyecto incluye documentación complementaria destinada tanto a usuarios como a desarrolladores.

Entre ella:

- TODO.md
- CHANGELOG.md
- INDEX.md
- CODEBASE_ANALYSIS.md
- BUG_SUMMARY.md
- BUG_ANALYSIS_DETAILED.md
- BUG_FIXES_GUIDE.md

La intención es mantener toda la información técnica organizada y actualizada conforme evolucione el proyecto.

---

# Contribuciones

Las contribuciones son bienvenidas.

Antes de enviar cambios se recomienda:

1. Compilar el proyecto.
2. Ejecutar todas las pruebas.
3. Mantener la arquitectura modular.
4. Documentar nuevas funcionalidades.
5. Actualizar la documentación correspondiente.

---

# Licencia

La licencia del proyecto deberá definirse o actualizarse conforme evolucione el desarrollo.

Hasta entonces, se recomienda consultar el repositorio para conocer las condiciones de uso y distribución.

---

# Agradecimientos

Football Manager Game es el resultado de un proceso continuo de diseño, desarrollo, refactorización y mejora incremental.

Cada nuevo sistema incorporado busca aumentar el nivel de realismo, mantenibilidad y escalabilidad del proyecto, manteniendo una arquitectura preparada para seguir creciendo en futuras versiones.