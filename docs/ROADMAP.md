# Roadmap

## Base actual: v0.1.2.1-alpha

La versión `v0.1.2.1-alpha` establece una base funcional sobre la que continuará el desarrollo del proyecto.

Entre los sistemas ya implementados se encuentran:

- Interfaz gráfica Win32.
- Frontend CLI.
- Modo Carrera.
- Motor de simulación de partidos.
- Match Center interactivo.
- Cambios de mentalidad durante el partido.
- Cambios de instrucciones tácticas.
- Aplicación combinada de mentalidad e instrucción.
- Sustituciones manuales.
- Sustituciones de la IA visibles.
- Pausa y reanudación.
- Velocidades `1x`, `2x` y `4x`.
- Estadísticas de partido.
- xG.
- Momentum.
- Valoraciones de jugadores.
- Mercado de fichajes.
- Finanzas semanales.
- Salary cap relativo a los ingresos del club.
- KPIs del dashboard.
- Menú contextual de jugadores.
- Modularización progresiva mediante `CareerService`.
- Pruebas automatizadas.
- Validación general mediante CLI.

Este roadmap parte desde ese estado y no considera estas funciones como pendientes.

---

# Prioridades de arquitectura

## Modularización

El proyecto continuará reduciendo módulos de gran tamaño.

Prioridades:

- Dividir lógica de simulación demasiado concentrada.
- Dividir módulos de GUI demasiado grandes.
- Mantener `CareerService` modularizado y realizar nuevas extracciones solo cuando un módulo vuelva a concentrar responsabilidades.
- Reducir código transitorio o de compatibilidad cuando sea seguro.
- Mantener separada la presentación de la lógica de negocio.
- Aumentar el uso de APIs estructuradas entre módulos.

## Acceso seguro a datos

Se buscará continuar reemplazando patrones legacy inseguros.

Objetivos:

- Reducir accesos por índice sin validación.
- Reducir dependencias innecesarias de punteros compartidos entre dominios.
- Utilizar identificadores estables cuando corresponda.
- Centralizar validaciones de acceso.
- Evitar duplicación de lógica de búsqueda y resolución.

## Servicios de aplicación

La dirección general consiste en que GUI y CLI consuman servicios comunes.

Áreas a seguir migrando:

- Flujo semanal.
- Competencias.
- Finanzas.
- Transferencias.
- Manager.
- Desarrollo.
- Eventos de carrera.
- Comunicaciones.

---

# Motor de Simulación

El motor continuará evolucionando sobre la base actual.

## Fases y transiciones

Mejoras posibles:

- Fases de partido más detalladas.
- Transiciones defensa-ataque más profundas.
- Transiciones ataque-defensa.
- Mayor relación entre presión y fatiga.
- Mayor relación entre mentalidad y generación de ocasiones.
- Mayor influencia del Momentum sobre el desarrollo del partido.
- Más eventos contextuales.

## Balance

Se busca mejorar la estabilidad estadística mediante:

- Simulaciones masivas.
- Distribución de goles.
- Distribución de xG.
- Frecuencia de lesiones.
- Frecuencia de tarjetas.
- Frecuencia de sustituciones.
- Rendimiento local/visitante.
- Impacto táctico.
- Diferencias entre niveles de equipo.

## Eventos futuros

Posibles incorporaciones:

- Clima dinámico.
- Árbitros con perfiles propios.
- VAR.
- Revisión de jugadas.
- Eventos especiales.
- Repeticiones.
- Highlights.

---

# Match Center

El Match Center interactivo ya está implementado.

El trabajo futuro se orientará a profundidad visual, feedback y cobertura.

## Visualización

Posibles mejoras:

- Más estadísticas en vivo.
- Comparativas entre jugadores.
- Comparativas entre equipos.
- Paneles adicionales.
- Mejor representación del Momentum.
- Mayor claridad visual de cambios tácticos.
- Mejor timeline.
- Filtros de eventos.
- Indicadores de fatiga y rendimiento.

## Interacción táctica

Posibles mejoras:

- Más instrucciones.
- Más feedback sobre compatibilidad táctica.
- Explicación del impacto de cada decisión.
- Ajustes por líneas.
- Ajustes de presión.
- Ajustes de ritmo.
- Comportamiento con y sin balón.
- Roles individuales.

## Accesibilidad e interfaz

Prioridades:

- Mejor escalado en diferentes resoluciones.
- Mejor navegación.
- Mayor legibilidad.
- Estados visuales más claros.
- Mejor feedback de botones y decisiones.
- Reducción de solapamientos en resoluciones pequeñas.

## Penales

Como opción futura podría incorporarse un mini-juego interactivo para definiciones por penales.

No forma parte del ciclo inmediato de desarrollo.

---

# Inteligencia Artificial

La IA continuará mejorando en distintas áreas.

## IA táctica

Objetivos:

- Adaptación más fuerte al rival.
- Respuesta más precisa al marcador.
- Mayor uso del Momentum.
- Mejor lectura de fatiga.
- Mejor interpretación del riesgo.
- Decisiones más variadas según minuto y contexto.

## Personalidad de entrenadores

Posibles perfiles:

- Conservador.
- Ofensivo.
- Pragmático.
- Presionante.
- Contratacante.
- Formador.
- Rotador.

Cada perfil podría influir en:

- Tácticas.
- Sustituciones.
- Fichajes.
- Rotación.
- Gestión del riesgo.

## Planificación de plantilla

Mejoras previstas:

- Detección más precisa de necesidades.
- Rotación a medio plazo.
- Gestión de profundidad por posición.
- Renovaciones.
- Desarrollo de juveniles.
- Venta de excedentes.

---

# Modo Carrera

El Modo Carrera continuará ampliando la gestión deportiva e institucional.

## Directiva

Posibles mejoras:

- Relaciones más profundas con la directiva.
- Evaluaciones periódicas.
- Confianza del club.
- Objetivos a corto y largo plazo.
- Consecuencias por incumplimiento.
- Renovaciones del manager.

## Prensa y comunicación

Posibles sistemas:

- Conferencias de prensa.
- Declaraciones.
- Reacciones de jugadores.
- Reacciones de la directiva.
- Efectos sobre moral y reputación.

## Patrocinadores

Posibles incorporaciones:

- Contratos de patrocinio.
- Bonificaciones por objetivos.
- Duración de acuerdos.
- Diferentes niveles de patrocinador.
- Impacto financiero.

## Historial

Mejoras previstas:

- Historial ampliado del manager.
- Historial de clubes dirigidos.
- Títulos.
- Ascensos.
- Descensos.
- Premios.
- Récords.

---

# Desarrollo Juvenil

El sistema juvenil puede ampliarse con:

- Academia más profunda.
- Calidad de instalaciones.
- Entrenadores juveniles.
- Scouting juvenil.
- Generación regional de talentos.
- Diferencias por club.
- Trayectorias de desarrollo.
- Cesiones.
- Promoción gradual al primer equipo.

---

# Mercado de Fichajes

El mercado continuará profundizando sus negociaciones.

## Negociaciones

Mejoras posibles:

- Negociación en múltiples pasos.
- Contraofertas más complejas.
- Bonificaciones.
- Variables contractuales.
- Cláusulas.
- Promesas deportivas.
- Consecuencias por promesas incumplidas.

## IA de mercado

Objetivos:

- Mejor evaluación de necesidades.
- Mayor coherencia económica.
- Mejor manejo de salarios.
- Mejor decisión entre comprar, vender o mantener.
- Evaluación de edad y potencial.
- Estrategias específicas por división.

---

# Finanzas

El sistema financiero continuará creciendo sobre el salary cap ya implementado.

Posibles mejoras:

- Proyecciones de temporada.
- Presupuesto anual.
- Patrocinios.
- Premios de competición.
- Ingresos por asistencia.
- Costos operativos.
- Mejor gestión de deuda.
- Alertas financieras más precisas.
- Planificación económica a largo plazo.

---

# Dashboard

El dashboard ya incorpora KPIs visuales.

Posibles mejoras:

- Más indicadores configurables.
- Prioridades personalizadas.
- Comparación semanal.
- Tendencias.
- Alertas agrupadas.
- Accesos rápidos.
- Métricas históricas.

---

# Persistencia

Objetivos futuros:

- Mayor robustez frente a archivos dañados.
- Compatibilidad entre versiones cuando sea posible.
- Validaciones más detalladas.
- Mejor diagnóstico de errores.
- Migraciones de formato controladas.
- Backups automáticos opcionales.

---

# Calidad y pruebas

La calidad seguirá siendo un requisito para cada release.

## Tests automáticos

Prioridades:

- Mantener cobertura actual.
- Añadir pruebas de regresión para nuevas features.
- Aumentar cobertura de servicios de carrera.
- Aumentar cobertura de transferencias.
- Aumentar cobertura de economía.
- Aumentar cobertura de competiciones.
- Ampliar pruebas del Match Center.

## Simulaciones largas

Se planea aumentar las pruebas de larga duración para detectar problemas de balance o acumulación.

Áreas:

- Goles.
- Lesiones.
- Tarjetas.
- Fatiga.
- Desarrollo.
- Economía.
- Mercado.
- Salarios.
- Evolución de plantillas.

## GUI

Posibles mejoras de validación:

- Smoke tests del frontend Win32.
- Verificación de apertura de pantallas.
- Pruebas de navegación.
- Validación de estados básicos.
- Detección de errores al iniciar carrera.

---

# Validación de releases

Cada release debería mantener como mínimo:

```powershell
cmake --build .\build-ci
```

seguido de:

```powershell
ctest --test-dir .\build-ci --output-on-failure
```

y:

```powershell
.\build-ci\bin\FootballManagerCLI.exe --validate
```

También deberá verificarse:

- `git diff --check`.
- Contenido correcto del paquete.
- Ausencia de guardados personales.
- Ausencia de binarios de desarrollo innecesarios.
- Funcionamiento del ejecutable descargado desde la release pública.
- Documentación actualizada.

---

# Packaging

Las releases públicas deben continuar utilizando una estructura limpia.

Ejemplo:

```text
FootballManager-vX.Y.Z/
├── assets/
├── data/
├── saves/
├── FootballManager.exe
└── LEEME.txt
```

No deberían incluir:

- `FootballManagerCLI.exe`.
- `FootballManagerTests.exe`.
- Builds intermedios.
- Archivos temporales.
- Guardados personales.
- Carpetas de desarrollo.

---

# Documentación

La documentación deberá mantenerse sincronizada con el código.

Archivos principales:

- `README.md`
- `CHANGELOG.md`
- `TODO.md`
- `INDEX.md`
- `docs/ARCHITECTURE.md`
- `docs/ROADMAP.md`

Los documentos históricos de bugs deberán mantenerse como referencia y no como representación automática del estado actual del proyecto.

---

# Objetivo general

El desarrollo futuro de Chilean Footballito busca profundizar tres pilares:

1. **Realismo**
   - Mejor simulación.
   - Mejor IA.
   - Más profundidad de gestión.

2. **Mantenibilidad**
   - Menor acoplamiento.
   - Servicios especializados.
   - Más pruebas.
   - Mejor separación de responsabilidades.

3. **Experiencia de usuario**
   - Interfaz más clara.
   - Match Center más informativo.
   - Mejor feedback.
   - Menor fricción al gestionar una carrera.

---

# Versión de referencia

Este roadmap parte del estado alcanzado en:

**Chilean Footballito `v0.1.2.1-alpha`**
