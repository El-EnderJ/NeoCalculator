# SCIENTIFIC-QUANTITIES-01 — Medidas, constantes e incertidumbre

Fecha: 2026-10-07. La nueva capa se integra en **Calculation**, sobre Quantity y el CAS existente. Una medida conserva la identidad de sus fuentes al pasar por Ans, PreAns, historial, A–F, conversiones y FORMAT. Las entradas nuevas son independientes por contrato; las constantes físicas utilizan CODATA 2022 y su covarianza conocida. Una propagación no sustentada produce un error específico, nunca un resultado presentado como exacto.

## Baseline y preservación

HEAD y rama comprobados: `main`, `ab4017764a62a03e0befd51a50fa82f3c31b4b70`, `feat(units): support affine temperatures and temperature differences`. El índice estaba vacío y la única entrada inicial fuera de seguimiento era `docs/RESUMEN_CHAT_TOOLBOX.png`. Se conservaron ese archivo y `.vscode/settings.json` byte por byte.

La preservación anterior a las ediciones está en `out/scientific-quantities-01/A-baseline/`: archivo de HEAD, parches de índice/working tree, estado y copias con hashes de los dos archivos protegidos. Los worktrees de kernel, datos y validación son independientes, bajo `C:/.codex-cache/scientific-quantities-01/`. Un único integrador trasladó las entregas finales de los archivos centrales. Los agentes editaron posteriormente solo scripts de validación de su propiedad. No se efectuaron commits, push, despliegue web ni flash; tampoco stash, reset, clean ni staging masivo.

Se revisaron los contratos CALC-UNITS-01/02, UNIT-CATALOG-02 y su inventario/datos, UNIT-PICKER-UX-PERF-01, TOOLBOX-01 y CALCULATION-FORMAT-01. Permanecen las 193 definiciones calculables, los roles Point/Difference, referencias contextuales separadas, favoritos, cuatro locales y A–F de sesión.

El emulador publicado se comprobó y arrancó al inicio. Las compilaciones se hicieron fuera de `C:/.piobuild/numOS`, en `C:/.piobuild/numOS-scientific-build/{native,pool,firmware}`; el script elimina un `PLATFORMIO_BUILD_DIR` heredado. La publicación final se realiza exclusivamente mediante el publicador existente, que valida y sustituye atómicamente, conservando `previous-program.exe`.

## Organización de la entrega

| Flujo | Responsabilidad | Entrega verificable |
|---|---|---|
| Datos/metrología | Auditoría original, fuentes primarias, generador offline, oráculos, covarianzas | `B-data`, `data/scientific`, inventario generado |
| Kernel | Payload, identidad, combinación, aritmética, dominios, límites, perfil Xtensa | `C-kernel`, `ScientificUncertainty`, Quantity |
| Producto/integración | Measurement, cursor/layout/render, Toolbox, FORMAT, locales, web | `D-product`, `E-final/web-final-v3`, galería |
| Validación | Mutantes causales, regresiones, 900 trazas, producto, recursos | `E-final/validation-final-v3`, recursos y ciclos |

Los intentos fallidos se conservan separados de los resultados finales. Las repeticiones no se suman como casos únicos. `E-final/validation-final-v3/summary.json` identifica el binario de producto y cada grupo de evidencia. Las carpetas anteriores documentan los candidatos que precedieron a la corrección angular y a las comprobaciones de Giac y Ayuda.

## Autoridades y datos offline

Dataset: **CODATA 2022**; NIST web **9.0**, publicado en la web el **2024-05-09**; publicación de referencia **2025-04-30**; consulta **2026-10-07**. No se afirma un conjunto recomendado CODATA 2026. Fuentes: [NIST constants](https://physics.nist.gov/constants), [versiones NIST](https://physics.nist.gov/cuu/Reference/versioncon.shtml) y [publicación CODATA 2022](https://www.nist.gov/publications/codata-recommended-values-fundamental-physical-constants-2022).

El nominal matemático es independiente de la incertidumbre física: almacenar un decimal mediante un racional no convierte una constante medida en exacta. La auditoría completa y reproducible de las 14 referencias iniciales y de G está en [PHYSICAL_CONSTANTS_CODATA_2022.md](PHYSICAL_CONSTANTS_CODATA_2022.md); su equivalente machine-readable es `src/math/scientific/physical_constants.json`.

Las referencias originales eran c, a₀, mₑ, Eₕ, tiempo atómico, ℏ, longitud/tiempo/masa/fuerza/energía/temperatura de Planck, g₀ y e. Los ReferenceId 1–14 no cambian. c y e son exactas por definición SI; ℏ conserva exactamente h/(2π); g₀ es exacta por convención, no gravedad local medida. Fuerza y energía de Planck pasan de nominales redondeados a sus fórmulas centrales y sensibilidades. La temperatura de Planck conserva su rol de punto termodinámico.

Se añade solo **G**, ReferenceId 59 / ConstantId 15: es básica y ya es dependencia de seis referencias de Planck existentes. h y k se capturan como dependencias exactas sin crear nuevas filas de Toolbox. Euler e sigue siendo un nodo distinto de carga elemental. Mach, pH, dB, calendario y otras referencias contextuales permanecen fuera.

`ConstantId`, `ConstantDatasetVersion`, ReferenceId, nominal, u, exactitud y metadata están separados. Cambiar un dataset futuro no exige cambiar favoritos ni identidades de AST. El payload de una sesión ya evaluada es inmutable y conserva su versión; no se ofrece carga de datasets en caliente.

`data/scientific/codata2022/source_capture.json` captura únicamente las filas numéricas necesarias y pares autorizados, con URL/hash/fechas. `constants_spec.json` conserva la auditoría inicial. `independent_oracle.json` y los oráculos Decimal/Jacobi son independientes del kernel. `metrology_sources.json` identifica cinco autoridades, secciones y extractos breves; no se redistribuyen páginas completas ni PDF. El fetch es una acción explícita de mantenimiento. El generador, compilación y tests funcionan offline.

## Representación y SourceId

`Quantity::Value` mantiene coeficiente exacto, dimensiones y rol; añade `Provenance::{Exact,Measured,DerivedUncertain}` y un `shared_ptr<const Uncertainty>` opcional. Una Quantity exacta no reserva un array de contribuciones. Copias de Ans, historial o A–F comparten un payload inmutable; nuevas operaciones publican un candidato completo al terminar.

Cada payload contiene como máximo **16** contribuciones ordenadas `{SourceId, coefficient}`. El coeficiente es la sensibilidad multiplicada por la incertidumbre estándar de la fuente. La fusión ordenada combina identidades iguales en O(n), elimina solo cancelación exactamente cero y rechaza la entrada 17 antes de escribir fuera del buffer. No hay recorte por epsilon ni descarte de fuentes pequeñas.

Las fuentes de usuario ocupan 1…0x7fffffff. Un contador atómico saturante no hace wrap ni reutiliza identificadores. Las constantes usan el bit alto, versión del dataset y ConstantId. `UINT32_MAX` se reserva para AST de presentación y no es admisible como fuente de evaluación. Agotar el contador devuelve `source_exhausted`; no requiere asignación heap.

Una plantilla nueva genera otra identidad, aunque sus dígitos coincidan. Clonar o recuperar la misma entrada conserva SourceId, incluida su edición posterior. Para declarar otra medición independiente se inserta una nueva plantilla. No se usan direcciones de memoria ni strings de expresiones como identidad de fuente.

| Operación | Resultado probado |
|---|---|
| X−X; X=10±1 | 0, u=0, sin contribuciones residuales |
| X+X | 20, u=2, una fuente |
| X−Y; entradas independientes 10±1 | 0, u=√2, dos fuentes |
| (10±1)+(20±2) | 30, u=√5 |
| (2±0.1)(3±0.2) | 6, u=0.5 |
| (100±1) cm → m | 1 m, u=0.01 m |
| (20±0.5) °C → °F | 68 °F, u=0.9 Δ°F |
| G−G; 2G | u=0; u=2u(G) |

## Correlación y estabilidad numérica

La misma identidad tiene ρ=1. Dos fuentes de usuario distintas, o usuario frente a constante, son independientes **por el contrato explícito de esta versión**, no por una afirmación sobre mediciones de laboratorio en general.

Se capturaron los 36 pares de nueve constantes inciertas tabuladas, y se derivaron 19 pares adicionales de las dos referencias dependientes de G: 55 pares de 11 constantes inciertas. El runtime almacena 27 pares no nulos y máscaras de disponibilidad para los ceros explícitos. Un par desconocido devuelve `correlation_unavailable`; nunca se sustituye por cero. Diagonal, simetría, rango y disponibilidad son verificables offline.

Para a₀·mₑ, ρ=−0.99999. Un oráculo Decimal de 70 cifras obtiene nominal `4.8204782634965449593616e-41` y u=`7.3474178868567063004e-51`; imponer ρ=0 da `1.6593332935255792496e-50` y falla el detector. La matriz **completa capturada** es semidefinida positiva dentro de 1e−12; el mínimo calculado es −2.22e−16 por redondeo. No se rellena una matriz parcial para justificarla. Pares perfectamente correlacionados permiten singularidad.

La varianza emplea escalado y suma compensada. Se rechazan NaN, infinito, subnormales y underflow relevante. Un negativo solo se limita a cero si cabe en la cota de error de redondeo derivada del número de operaciones; un negativo material es error numérico. `double` se utiliza únicamente para sensibilidades y sus comprobaciones: nunca decide dimensiones, rol, compatibilidad, igualdad exacta ni la autoridad del nominal. Las funciones cerradas admitidas se evalúan como acompañante numérico desde el árbol exacto, sin reutilizar el texto `evalf` redondeado para el display. La regresión generada de 128 triples y oráculos independientes comprueba el orden de combinación dentro de tolerancia de coma flotante; no se promete asociatividad bit a bit.

La varianza independiente recorre las fuentes en O(n); los términos cruzados se consultan solo entre constantes, con n≤16. No se ejecuta Giac por fuente. El nominal sigue el pipeline existente; la incertidumbre añade sus acompañantes numéricos acotados.

## Propagación y no linealidad

Se implementa propagación local de primer orden con covarianza: suma/resta, producto/cociente, escalas exactas, potencias racionales admitidas, raíces y funciones soportadas abs, exp, ln, log10, sin, cos y tan. Se conservan las restricciones de dimensiones y los modos angulares existentes. Una función no soportada con incertidumbre se rechaza explícitamente.

Los coeficientes canónicos Quantity se evalúan siempre en radianes. Una función trigonométrica con argumento adimensional introducida en DEG aplica π/180 una sola vez en el límite de entrada; una unidad angular explícita ya está en radianes coherentes. Se conserva Quantity incluso en un resultado exacto adimensional que aún contiene funciones angulares, para que Ans, memorias y FORMAT no reinterpreten `sin(1)` al cambiar Settings. Las memorias escalares anteriores se importan mediante su árbol tipado, con presupuestos de 400 nodos/profundidad 40 antes y después de normalizar. La revisión adicional detectó este fallo con `sin((1±0.01) rad)` en DEG; se conservan el fallo y las pruebas posteriores de nominal, sensibilidad, producto y cambios DEG/RAD.

El alcance se fundamenta en [JCGM 100:2008 §§5.1–5.2](https://doi.org/10.59161/JCGM100-2008E), [Amd.1:2026](https://doi.org/10.59161/PPDI3267) y [GUM-6:2020, anexo F.2](https://doi.org/10.59161/JCGMGUM-6-2020). **No se afirma conformidad completa con GUM.** [JCGM 101:2008](https://doi.org/10.59161/JCGM101-2008) se estudió como alternativa para propagación de distribuciones; Monte Carlo no forma parte del runtime.

La comprobación local usa la vecindad ±u y una cota analítica M₂ de la segunda derivada. El resto R=M₂u²/2 debe caber en δ=0.5×10^(floor(log10(u_lineal))−1), media unidad del último dígito de un informe de dos cifras de u. Este presupuesto es una política explícita de fidelidad local, **no un criterio normativo universal ni una probabilidad de cobertura**. No cambia al pulsar FORMAT y no demuestra validez global para distribuciones desconocidas.

Para (10±0.1)²: u_lineal=2, R=0.01, δ=0.05; pasa. Para (0±1)², la derivada nula no acredita certeza: `uncertainty_nonlinear`. También se rechazan la vecindad que cruza cero en un denominador, un dominio inválido de raíz o un polo de tan. El nominal matemático válido no se confunde con una propagación válida.

Si u>0 y binary64 no distingue x+u o x−u de x, las funciones no lineales y potencias rechazan `uncertainty_numeric`: no pueden certificar su vecindad local. Esto cubre ángulos enormes con incertidumbres no resolubles y puntos estacionarios con u extremadamente pequeña. Los casos realmente lineales p=0/p=1 y abs con signo conocido conservan sus atajos. Es una restricción numérica del producto, no una declaración de imposibilidad física de la medida.

Se comprobó un defecto del Giac incluido: su simplificación opcional de `sin(pi/180)` arroja `std::length_error` en native y no resulta recuperable de forma equivalente en WASM. Un recorrido tipado de hasta 400 nodos/profundidad 40 detecta funciones trigonométricas cerradas con π y conserva directamente el resultado exacto ya válido de `eval`, evitando esa simplificación opcional. Las identidades simbólicas y ángulos notables conservan su camino; no se reescriben strings. Una excepción `bad_alloc` sigue siendo error y la orden explícita Simplify conserva su contrato.

Para productos de conjuntos disjuntos de fuentes de usuario se usa la identidad de segundos momentos Var(XY)=y²uₓ²+x²uᵧ²+uₓ²uᵧ², válida por independencia y momentos finitos sin exigir normalidad. La corrección en u debe ser menor que δ; el payload sigue siendo de primer orden. En el producto obligatorio 2±0.1 por 3±0.2, la corrección es aproximadamente 0.00039984 frente a δ=0.005. Los productos correlacionados usan una cota local conservadora; los cocientes exigen distancia positiva al cero.

## Entrada, presentación y navegación

La categoría **Medidas / Measurements** inserta `NodeMeasurement` (entry 750): dos slots inequívocos, nominal y u, con SourceId. No se redefine ± como operador binario. Los paréntesis estructurales protegen el sentido de `(12±0.1) V`; se requiere escribir la unidad una sola vez. La ayuda declara incertidumbre estándar, k=1, independencia de plantillas nuevas y dependencia al reutilizar una medida.

El nodo se integra en clonación, escaneo de unidades, cursor, borrado y navegación. Sus métricas de ±, espacios y delimitadores se calculan con los helpers tipográficos MATH existentes. Layout guarda los orígenes utilizados por renderer y cursor: no se duplican fórmulas estadísticas ni geometría. Se conservan estilos y fuentes STIX; no se modifican archivos de fuentes, MATH-TEX, el ensamblador ni el lookup TeX de ocho clases. Layout/draw/cursor no crean payloads ni asignan memoria por fuente.

La transición a Ayuda reutiliza el panel existente y dibuja los textos inmutables del catálogo. La inyección de fallos C encontró que incluso `lv_label_set_text_static` podía reservar un callback de layout; el nuevo camino no modifica etiquetas ni reserva memoria al abrir Ayuda. Se registra un callback al abrir Toolbox, con su coste incluido en los ciclos de pool fijo. No se atribuye a esta comprobación cobertura de todo fallo asíncrono del allocator de dibujo de LVGL.

La incertidumbre debe ser real, no negativa, exacta como entrada matemática y compatible dimensionalmente. Si el nominal es punto térmico, u debe ser diferencia. Nominal/u que ya incorporan incertidumbre, complejos y símbolos fuera del contrato se rechazan. Aplicar °C/°F a una medida adimensional transforma el punto por la afinidad y las contribuciones solo por la pendiente. P−Q es diferencia; P−P cancela sus fuentes sin perder el rol.

Para resultados inciertos, FORMAT muestra **Unidad de salida**, **Valor e incertidumbre**, **Más dígitos + u** e **Incertidumbre**. Esta última ofrece estándar u (k=1), relativa u_r, expandida U (k=2) y parentética. La selección y cancelación conservan el canónico y todas las fuentes. Los resultados exactos y escalares conservan sus formatos anteriores. En medidas no se llama «Exact» a la vista central, evitando una afirmación metrológica falsa.

JCGM 100 §7.2.6 respalda limitar cifras de u y alinear el valor central; el valor predeterminado es dos cifras significativas, con manejo de acarreo y nominal cero. Más dígitos mantiene la inspección de binary64. Los valores internos nunca se reconstruyen del display. Para separaciones extremas de escala se conservan ambas cantidades en notación científica independiente. [NIST TN 1297 §7](https://www.nist.gov/pml/nist-technical-note-1297/nist-tn-1297-7-reporting-uncertainty) respalda identificar u/U y el factor: k=2 no se presenta como «95 %».

La relativa usa el nominal canónico; es indefinida con nominal cero (—). En puntos térmicos usa el origen Kelvin, etiquetado u_r(K), sin depender del número Celsius/Fahrenheit. La vista estándar térmica escribe explícitamente `68.00 °F ± 0.90 Δ°F`.

Constantes, ayuda, errores y FORMAT funcionan en en-US, en-GB, es-ES y es-419, compartiendo textos regionales cuando corresponde. Los símbolos u, u_r, U y k no se traducen. Los favoritos conservan ReferenceId. Ans, PreAns, historial, recuperación, edición, HOME/reentrada y A–F comparten las fuentes. A–F permanece de sesión para estas cantidades; no se añadió persistencia durable ni se restaura un escalar durable anterior.

La [galería autocontenida](../out/scientific-quantities-01/E-final/gallery/index.html) contiene 146 originales 320×240 (43 native, 7 de revisión adicional y 96 web), embebidos sin fuentes ni recursos externos, además de [ZIP](../out/scientific-quantities-01/E-final/gallery/scientific-quantities-gallery.zip) y manifiesto SHA-256. Incluye plantilla vacía, todos los modos, locales, búsqueda/favoritos, constantes, cancelación, independientes y errores. La prueba de desplazamiento del resultado correlacionado verifica ambos extremos, saturación y retorno a la vista inicial sin modificar el canónico. Los resultados que exceden el ancho se desplazan; no se afirma que toda expresión arbitraria quepa simultáneamente en 320 píxeles.

## Validación final y fallos

| Gate | Resultado / evidencia |
|---|---|
| Contrato científico independiente | 2.223 aserciones, 128 triples generados, 10 grupos |
| Dataset offline | 491 aserciones; IDs, dimensiones, exactitud, correlaciones y matriz completa |
| Producto científico | 48 recorridos únicos PASS; 43 PNG originales |
| Revisión adicional | 4 recorridos visuales; plantilla vacía, búsqueda, límite y paneo |
| Mutaciones obligatorias | 20 científicas + 12 unidades + 12 temperaturas: todas rechazadas |
| Corpus histórico | 900/900; solo se excluyen campos temporales micros/warmSolveMicros |
| Host anterior | 6.336 aserciones unidades; 1.436 temperaturas; demás suites de input/spacing/brackets/catalog/picker |
| Producto anterior | Input 113, unidades 23, temperaturas 29, picker 14, brackets 20, Toolbox 207 |
| Observador de UI | FORMAT 28, presentación EN/ES 2, viewport 7 |
| Layout nuevo | 31 aserciones; 3.000 layouts con asignaciones C++ denegadas |
| Ciclos pool final | 100 medidos tras 4 warmups, historial lleno; sin reinicio de Giac |
| Web científica | Chromium, Firefox, WebKit × shell/componente: seis superficies PASS |
| Firmware | WROOM y CAM compilan; sin ejecución ni flash en placas |

Las 20 mutaciones alteran de forma causal código ejecutado: identidad de misma/nueva fuente, Ans, historial, conversión, offset térmico, redondeo como canónico, exactitud de constantes, rho cero/signo/rango, descarte en límite, varianza negativa, derivada cero, wrap, semántica ±, favoritos cruzados, k=2 sobre canónico y relativa del display. Sus logs muestran compilación y rechazo. Las siete mutaciones de dataset se reportan aparte y **no** se suman como siete familias nuevas cuando solapan las veinte.

Los fallos se inyectan en overlays de prueba; no se introducen fallos en el binario publicado. La creación de SourceId y la tabla de correlaciones no asignan heap: se prueban saturación persistente, resolver ausente, rho fuera de rango y NaN, en vez de fingir un malloc inexistente. El contador no reutiliza IDs; el resultado anterior queda intacto. El allocator de firmware se prueba también mediante una shim host de `esp_heap_caps`: 69 checks, flags INTERNAL|8BIT, 10 fallos consecutivos de source y 10 de combinación, cancelación sin heap, recuperación y cero bloques vivos al terminar. Esto ejecuta la rama de allocator de producción en host, no hardware.

Errores específicos: `uncertainty_invalid`, `uncertainty_limit`, `uncertainty_numeric`, `correlation_unavailable`, `uncertainty_nonlinear`, `source_exhausted` y `allocation`; incompatibilidad dimensional y rol conservan sus diagnósticos. La publicación es transaccional: no se transforma un fallo de u en un resultado exacto ni se pierde silenciosamente una fuente.

## Recursos y rendimiento

Los perfiles Xtensa finales están en `E-final/resources-final-v3/{wroom,cam}/resources.json`. Son compilaciones locales; la referencia instalada WROOM sigue siendo SHA-256 `7aac4006651624212d7561061436121e1781c9ce5b43d22fe528fd93b562c1a6` y no se escribió ninguna placa.

| Imagen | Bytes de imagen | Margen app | RAM estática | IRAM |
|---|---:|---:|---:|---:|
| WROOM instalada, baseline | 6.269.680 | 283.920 | 119.672 | 60.407 |
| WROOM candidata final | 6.297.920 | 255.680 | 119.672 | 60.407 |
| CAM candidata final | 6.209.824 | 343.776 | 118.264 | 59.039 |

La partición de 6.553.600 bytes no cambia. WROOM añade 28.240 bytes de imagen respecto a la referencia instalada; conserva 124.608 bytes de margen por encima del objetivo de 131.072. RAM estática e IRAM permanecen iguales. SHA-256 WROOM final: `aef8e43706b8b19debacb49d4ce59020685aac75ad5c8e16a2b73fa944f76c5c`; CAM: `52b23ee4374887f7aaa4bdd4b1b667053ca6c0d8818945cc617d8f74ead26e90`.

| Tipo Xtensa | sizeof, bytes |
|---|---:|
| Quantity::Value | 56; baseline 44, incremento 12 |
| Uncertainty | 264 |
| Contribution | 16 |
| HistoryEntry | 160 |
| CalculationEvaluation | 200 |
| CalculationApp | 1.016 |
| NodeMeasurement | 64 |
| ConstantRecord / referencia de registro | 40 / 120 |
| Quantity::Display | 88 |

El payload se crea con `allocate_shared` y allocator interno `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT` en firmware. No reserva las 16 contribuciones dentro de cada Quantity ni HistoryEntry. Un payload compartido puede ser referenciado por resultado, Ans y varias entradas sin copiar su array. El nominal Giac y los AST mantienen sus estrategias existentes. Los perfiles ya activaban excepciones por Giac; esta fase no modifica esos flags ni añade RTTI. NodeMeasurement implementa los overrides existentes de MathNode, sin ampliar su interfaz virtual.

El análisis de marcos Xtensa mide **marcos propios**, no el pico completo de la tarea: layout de Measurement 96 B, combinación 352 B, creación de fuente 304 B, varianza 128 B, producto 128 B, potencia 112 B y función unaria 96 B. El escáner existente `Quantity::contains` conserva su buffer acotado y usa 1.712 B. Walker::node utiliza 448 B por nivel (presupuesto de profundidad 16); la normalización del árbol importado usa 240 B por nivel (árbol validado hasta profundidad 40). `inspectOptionalSimplify` usa 48 B por nivel y tiene presupuesto explícito. El dibujo de Ayuda usa 256 B más 192 B del helper de texto antes de entrar en LVGL. Estos recorridos pueden acumular varios KiB de pila; no se suman como si todas las rutas fuesen simultáneas ni se presentan como high-water físico. La tarea existente tiene 65.536 B de pila. No se añadió código marcado IRAM. La geometría de Measurement, su layout y su cursor no asignan heap; el dibujo no crea payloads ni AST. Esto no incluye las reservas internas de tareas de dibujo de LVGL, utilizadas también por Ayuda.

En el pool LVGL fijo de 64 KiB, `data-resources/final-v3/fixed-pool-100` pasa 100 ciclos después de 4 warmups, con 60 evaluaciones para llenar las 50 entradas de historial antes de cada ciclo. No se reinicia Giac. HOME conserva 67 objetos, 3 timers, 19.352 B libres y 0 handles; el mínimo muestreado en modales es 6.128 B, con bloque mayor mínimo de 5.520 B. El callback nuevo de Ayuda añade 64 B transitorios mientras Toolbox está abierto. El monitor informa 57.696 B de pool utilizable; no confundirlo con el tamaño configurado bruto de 65.536 B.

La instrumentación C++ complementaria encontró un calentamiento legítimo de 1.232 B que terminaba en el ciclo absoluto 49; se conserva ese intento. Una ejecución posterior del candidato final con **54 warmups y 100 ciclos medidos** queda plana: HOME 20.439.772 B y Calculation 20.457.622 B en todos los ciclos. Incluye el gran script de pruebas precargado y el estado global C++ de Giac; **no es la huella del dispositivo** ni mide malloc de C. Las fuentes alcanzables permanecen estables: HOME 25 payloads / 32 contribuciones / 12 identidades; Calculation 27 / 36 / 12. Los datos están en `data-resources/final-v3/lifecycle-steady-100`.

Cuatro poblaciones separadas llenan el historial (60 evaluaciones, capacidad 50): exactos 0 payloads / 0 contribuciones / 0 IDs; medidas nuevas 50 / 50 / 50; G repetida 50 / 50 / 1; mezcla 34 / 34 / 18. Es válido que el historial contenga más de 16 identidades en total: el límite se aplica a **cada resultado**. Los tamaños brutos de C++ de esos procesos incluyen scripts diferentes; no se utilizan como delta por payload. Evidencia: `data-resources/final-v3/history-populations`.

Se recuperan los 27 escenarios de fallo persistente real de asignaciones C++: creación de medida, propagación, resultado, Ans, historial, FORMAT, Toolbox, favoritos y variantes angulares. Cuatro escenarios adicionales prueban la apertura de Ayuda con cero intentos de asignación C y C++ en la transición; no se cuentan como cuatro malloc fallidos. Se preservan nominal/Ans y se completa una operación sana posterior. La saturación de ID, resolver ausente y correlaciones inválidas se ejercitan donde corresponde, sin inventar asignaciones en esos caminos. Evidencia: `data-resources/final-v3/faults.json`, `help-recovery` y shim de allocator de firmware.

Las medidas emparejadas baseline/candidato ejecutan cinco pares con orden alternado y las mismas entradas reales, 20 warmups y 50 evaluaciones por proceso. Medianas de los cocientes candidato/baseline: 2+2 **0,982**, unidad exacta **1,001**, temperatura **1,068**. Incluyen arranque, aserciones y teardown; el host compartía carga con otros gates. No acreditan una regresión de operador del 6,8 % ni una mejora escalar del 1,8 %. Se retienen distribuciones y carga conocida en `paired-product-busy-host.json`.

El overlay de perfil separa análisis, conversión, composición, evaluación y FORMAT con entradas emparejadas. Como ejemplos de medianas host (µs): medida de una fuente 70,05 / 55,85 / 4,50 / 452,95 / 104,40; dos fuentes 115,90 / 57,50 / 4,30 / 522,70 / 107,00; par CODATA correlacionado 519,00 / 485,30 / 6,45 / 1.706,15 / 187,75. Evaluación incluye Giac, publicación e historial; las fases de recorridos separados **no se suman**. Los picos C++ observados de evaluación de esos ejemplos son 9.497, 10.230 y 15.932 B, excluyendo familias C. Tabla completa y límites del contador: `data-resources/final-v3/profiles/profile-summary.json`.

Los recorridos adicionales de 8 y 16 fuentes dan, en el mismo orden de fases, 358,15 / 57,90 / 3,15 / 1.278,00 / 184,05 µs y 939,80 / 68,40 / 3,10 / 2.361,15 / 319,55 µs. El rechazo de 17 fuentes mide 839,70 µs de análisis y 1.320,20 µs de evaluación, preserva el anterior y permite 2+2 después; no publica display ni ofrece FORMAT para el resultado rechazado. La recuperación sana queda fuera de ese tiempo. Son 48 combinaciones de scope sobre 11 cargas lógicas entre ambas tablas, no 48 expresiones independientes. Evidencia: `data-resources/final-v3/many-source-profiles`, `SUMMARY.md` y `manifest.json`.

El perfil aislado del kernel (`C-kernel/performance-final-angle/profile.json`, mismo ScientificUncertainty final) usa 300 warmups y 20.000 operaciones por fila. Fusionar 2/8/16 fuentes tarda 112/259/376 ns; varianza de 2/8/16 fuentes, 18/57/77 ns; covarianza CODATA de dos, 30 ns. Cada fusión que crea payload realiza una reserva C++ de 280 B en ese ABI host; varianza, copia, cancelación exacta y rechazo de la fuente 17 no reservan. Son 18 escenarios distintos, no 360.000 casos únicos. No se extrapolan nanosegundos de MinGW al ESP32.

Heap interno, fragmentación de PSRAM, high-water de pila y latencias reales en WROOM/CAM quedan pendientes del cierre físico. Las comprobaciones de bounds/layout, perfiles de compilador y ciclos host no sustituyen esas mediciones.

## Web y reproducibilidad

La UI WASM se construyó localmente desde una copia ASCII del mismo código congelado; `E-final/web-build/*-source.json` registra hashes de entrada. El paquete local está en `C:/.codex-cache/scientific-quantities-01/web-source/out/wasm/dist/release`. No hay despliegue. La prueba científica usa teclas y Toolbox reales, con observadores de solo lectura; prueba temperatura, cuatro formatos, correlación, identidad, error/recuperación y favoritos persistidos vía IDBFS.

**WASM-MATH sigue siendo una API escalar.** No se le atribuye soporte de incertidumbre ni se enlaza accidentalmente la UI/Quantity como extensión de su contrato. WebKit acumulado se informa separado de recorridos nuevos en contextos independientes.

El paquete final `numos-emulator.c3c9f798a869.wasm` pasa los recorridos científicos y los de temperaturas en Chromium, Firefox y WebKit × shell/componente: seis superficies por suite. La regresión angular comprueba nominal y sensibilidad de radianes explícitos, grados explícitos, argumentos adimensionales DEG, reutilización tras cambiar a RAD y un producto con otra medida independiente. Evidencia: `E-final/web-final-v3/summary.json` y `E-final/web-temperatures-final-v3/results.json`.

Las regresiones anteriores también pasan: cantidades en seis superficies (12 registros por las observaciones antes/después de recarga), FORMAT 42 fixtures, viewport 18 y cuatro locales × seis superficies = 24. Un recorrido **separado** de WebKit acumulado pasa ocho repeticiones de los mismos fixtures de locales en un único proceso; no se añade como cobertura funcional nueva. El resto usa contextos independientes por superficie; los locales usan además nuevos procesos. Los hashes del paquete antes/después coinciden. Evidencia: `E-final/web-regression-final-v3/summary.json`.

WASM-MATH pasa en tres navegadores, incluida `sin(pi/180)` en worker y modo directo con comparación independiente `Math.sin` mediante la API `approximate` existente. El toolchain es Emscripten 6.0.3. Se compararon los comandos normalizados de 201 unidades vendor antes de reutilizar sus bibliotecas; el target escalar recompiló GiacEngine y reutilizó tres objetos propios sin cambios. Se conservó el primer fallo de fixture que intentó una entrada `evalf` no admitida; la corrección usa la API pública, sin ampliar el contrato. Evidencia, hashes y comandos: `E-final/wasm-math-final-v3/summary.json`.

Comandos reproducibles principales, desde la raíz y sin conexión para datos/tests:

```powershell
python scripts/generate-physical-constants.py --check
python scripts/test-physical-constants.py
python scripts/test-scientific-standalone.py --help
python scripts/build-scientific-candidate.py --profile native
python scripts/test-scientific-product.py --bin C:/.piobuild/numOS-scientific-build/native/emulator_pc/program.exe --out out/scientific-quantities-01/recheck/product
python scripts/test-scientific-visuals.py --bin C:/.piobuild/numOS-scientific-build/native/emulator_pc/program.exe --out out/scientific-quantities-01/recheck/visuals
python scripts/build-scientific-candidate.py --profile firmware
python scripts/build-scientific-gallery.py
```

Los comandos efectivos, fixtures, hashes y salidas de pruebas de larga duración están en sus carpetas de evidencia; scripts de mutaciones/fallos/perfil aceptan `--help`. Compilar exige los toolchains y dependencias ya instalados; «offline» significa que la compilación no descarga valores científicos ni consulta NIST.

## Límites, intentos y alternativas

No se implementan correlaciones manuales del usuario, distribuciones, UI Type A/B, grados de libertad, Welch–Satterthwaite, intervalos asimétricos, Monte Carlo, aritmética intervalar sustitutiva, complejos/matrices inciertos, ajuste experimental, tutor metrológico ni incertidumbre en Equations/Calculus/Grapher. Se conservan límites Quantity de 192 nodos, profundidad 16, 64 llamadas escalares y 2.000 caracteres del nominal. Rechazar overflow/underflow es deliberado; no se promete todo el rango abstracto de las expresiones exactas en la capa binary64.

La correlación publicada y los valores CODATA tienen redondeo finito: identidades entre constantes recomendadas diferentes pueden dejar residuos numéricos; no se impone una cancelación algebraica no sustentada por esos datos. La política local de no linealidad puede rechazar casos que una propagación de distribuciones resolvería.

Se conservan los intentos que detectaron doble validación del presupuesto de llamadas al llegar a 16 fuentes, prefijos de referencias, constness del helper de delimitadores y una ubicación antigua de la categoría Units. Se corrigieron antes de los gates finales. Los errores de rutas Unicode de herramientas host se resolvieron con rutas relativas/copias ASCII; no se ocultaron logs fallidos. La búsqueda antigua `milli-g` favorece el alias base por duplicación entre variantes; `millistandard gravity` selecciona el prefijo y su escala exacta está probada. No se cambia esa política global de búsqueda en esta fase.

Se descartó propagar solo un u escalar porque perdería dependencia; reservar 16 contribuciones dentro de toda Quantity penalizaría exactos/historial; reconstruir desde texto redondeado rompería identidad y precisión. El payload compartido e inmutable evita esas pérdidas, acota memoria y permite un futuro contrato de fuentes sistemáticas sin sustituir el modelo. Monte Carlo exigiría distribuciones y un presupuesto de ejecución explícitos, ajenos a esta fase.

## Archivos, evidencia y publicación

Los checkpoints son `A-baseline`, `B-data`, `C-kernel`, `D-product` y `E-final`. `E-final/final-manifest.json` lista **cada archivo modificado/nuevo con SHA-256**, estado Git, artefactos, preservación y hashes de evidencia; `E-final/candidate-source.zip` conserva el delta de fuentes del candidato sobre `A-baseline/HEAD.zip`, sin archivos de fuentes tipográficas, y `E-final/working.patch` el diff de archivos seguidos. El índice permanece sin cambios.

Grupos de producción: Quantity; ScientificUncertainty; ScientificReporting; PhysicalConstants y datos generados; MathAST/CursorController/MathRenderer; CalculationEngine/GiacEngine/CalculationFormat/CalculationApp; Toolbox y registro generado; diagnósticos NativeHal; allowlist PlatformIO. El manifiesto enumera también scripts nuevos, tests, inventarios y los ajustes mínimos de fixtures anteriores.

El candidato se publicó mediante `scripts/publish-emulator-windows.ps1`, después de todos los gates. Está disponible en `C:\.piobuild\numOS\emulator_pc\program.exe`, con sus cuatro DLL verificadas. SHA-256 publicado: `57a0203257a29528a8454ac49badacac638b181147832ce15d02b062adee6570`, 18.287.972 B. `previous-program.exe` conserva la versión inicial, SHA-256 `bde24d4216b8ebb00d90436f5552e691eeaab7d8eeb62f63606b6f32d1dfa087`, 18.143.683 B. No se cerró una sesión del usuario para reemplazarla.

El lanzador habitual arrancó el ejecutable publicado con una prueba aislada: medida 10±1 m, Ans+Ans → 20±2 m, memoria A−A → 0 m con u=0 y finalmente 2+3 → 5. Terminó correctamente y emitió `SCIENTIFIC_RELEASE_LAUNCHER_VALIDATED`. Logs, fixture y hashes de DLL en `E-final/publication/`.

Para probarlo: ejecutar `./scripts/run-emulator-windows.ps1`, abrir Calculation y Toolbox → Medidas → Valor ± incertidumbre; completar ambos slots, añadir una unidad y evaluar. FORMAT → Incertidumbre permite alternar las cuatro presentaciones sin cambiar la medida.

Checklist físico posterior (no ejecutado en esta fase): comprobar hashes y partición antes de un flash autorizado; medir heap interno/PSRAM y high-water de pilas en WROOM/CAM; recorrer medidas y temperaturas mediante teclado real; probar panel/touch y formatos largos; repetir presión de memoria e historial, HOME y modos térmicos; confirmar ausencia de regresión de cursor/baselines bajo carga de render y tareas en sus núcleos. Las medidas host y marcos de compilador no sustituyen esos picos en placa.

```text
SCIENTIFIC_QUANTITIES_01_IMPLEMENTADO: SÍ
CONSTANTES_CODATA: PASS
PROPAGACION: PASS
CORRELACIONES: PASS
NO_LINEALIDAD: PASS
UI_MEDIDAS: PASS
HARDWARE: NO EJECUTADO
```

PASS se refiere al contrato acotado descrito y a sus gates host, web y compilación. No declara validación física ni conformidad completa con GUM. Sin flash, commit, push o despliegue web.
