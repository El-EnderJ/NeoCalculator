// SPDX-License-Identifier: GPL-3.0-or-later
// Compiled inside the existing math boundary TU for identical native/WASM linkage.
namespace tutor {
namespace {
struct CatalogEntry {
    const char *key;
    const char *english;
    const char *schema;
    const char *spanish;
    const char *french;
};
constexpr CatalogEntry catalog[] = {
    {"domain.nonzero", "A denominator cannot be zero. Keep this restriction throughout the solution.", "", "Un denominador no puede ser cero. Conserva esta restricción durante toda la resolución.", "Un denominateur ne peut pas etre nul. Conserver cette condition pendant la resolution."},
    {"values.substitute", "Replace stored variables with their saved values for this solve.", "", "Sustituye las variables almacenadas por los valores guardados para esta resolución.", nullptr},
    {"algebra.expand", "Expand the products and powers so we can collect like terms.", "", "Desarrolla los productos y las potencias para poder agrupar los términos semejantes.", nullptr},
    {"algebra.collect", "Collect like terms by combining the coefficients of each power.", "", "Reduce los términos semejantes sumando los coeficientes de cada potencia.", nullptr},
    {"equation.swap", "Swap the two sides to put {0} on the left.", "v", "Intercambia los miembros para situar {0} a la izquierda.", "Echanger les membres pour placer {0} a gauche."},
    {"equation.add", "Add {0} to both sides to isolate the term containing {1}.", "ev", "Suma {0} a ambos miembros para dejar aislado el término que contiene {1}.", "Ajouter {0} aux deux membres pour isoler le terme contenant {1}."},
    {"equation.subtract", "Subtract {0} from both sides to isolate the term containing {1}.", "ev", "Resta {0} a ambos miembros para dejar aislado el término que contiene {1}.", "Soustraire {0} des deux membres pour isoler le terme contenant {1}."},
    {"equation.divide", "Divide both sides by {0} to isolate {1}.", "ev", "Divide ambos miembros entre {0} para despejar {1}.", "Diviser les deux membres par {0} pour isoler {1}."},
    {"rational.clear", "Multiply both sides by the common denominator to remove the fractions. Keep the original restrictions.", "", "Multiplica ambos miembros por el denominador común para eliminar las fracciones. Conserva las restricciones originales.", nullptr},
    {"polynomial.factor", "Factor the left side so we can use the zero-product rule.", "", "Factoriza el miembro izquierdo para aplicar la propiedad del producto nulo.", nullptr},
    {"square.branches", "Take the square root with both signs to include every solution.", "", "Considera la raíz cuadrada con ambos signos para incluir todas las soluciones.", nullptr},
    {"quadratic.formula", "Use the quadratic formula to find every value of {0} that satisfies the equation.", "v", "Aplica la fórmula general de la ecuación de segundo grado para hallar todos los valores de {0} que satisfacen la ecuación.", nullptr},
    {"product.zero", "A product is zero when at least one factor is zero. Set each factor to zero and solve each case.", "", "Un producto es cero si al menos uno de sus factores es cero. Iguala cada factor a cero y resuelve cada caso.", nullptr},
    {"candidate.excluded", "Reject this candidate: it makes an original denominator zero.", "", "Descarta este candidato: anula un denominador de la ecuación original.", nullptr},
    {"terminal.isolated", "{0} is already isolated. This equation gives its solution directly.", "v", "La incógnita {0} ya está despejada. Esta ecuación da su solución directamente.", "{0} est deja isolee. Cette equation donne directement sa solution."},
    {"terminal.identity", "Both sides are equal for every value in the selected domain.", "", "Ambos miembros son iguales para cualquier valor del dominio elegido.", nullptr},
    {"terminal.conditional", "Both sides are equal for every value satisfying the displayed denominator restrictions.", "", "Ambos miembros son iguales para cualquier valor que cumpla las restricciones indicadas sobre los denominadores.", nullptr},
    {"terminal.contradiction", "The equality is false: no value can satisfy it.", "", "La igualdad es falsa: ningún valor puede satisfacerla.", nullptr},
    {"terminal.no_real", "The discriminant is negative. No real square root exists, so there are no real solutions.", "", "El discriminante es negativo y no tiene raíz cuadrada real. Por tanto, no hay soluciones reales.", nullptr},
    {"terminal.complete", "These are all solutions under the displayed restrictions. Each also satisfies the original equation.", "", "Estas son todas las soluciones que cumplen las restricciones indicadas. Todas satisfacen también la ecuación original.", nullptr},
    {"system.swap", "Swap equations {0} and {1} to use a nonzero coefficient for elimination.", "rr", "Intercambia las ecuaciones {0} y {1} para usar un coeficiente distinto de cero en la eliminación.", nullptr},
    {"system.scale", "Divide equation {0} by {1} to make the coefficient of {2} equal to 1.", "rev", "Divide la ecuación {0} entre {1} para que el coeficiente de {2} sea 1.", nullptr},
    {"system.add", "Add {0} times equation {1} to equation {2} to eliminate {3}.", "errv", "Suma a la ecuación {2} la ecuación {1} multiplicada por {0} para eliminar {3}.", nullptr},
    {"system.subtract", "Subtract {0} times equation {1} from equation {2} to eliminate {3}.", "errv", "Resta a la ecuación {2} la ecuación {1} multiplicada por {0} para eliminar {3}.", nullptr},
    {"system.unique", "Each variable is isolated. These values form one simultaneous solution tuple.", "", "Cada incógnita está despejada. Estos valores forman una única solución conjunta del sistema.", nullptr},
    {"system.family", "A zero row adds no constraint. Free variables may vary, but must satisfy the remaining equations together.", "", "Una fila nula no añade restricciones. Las variables libres pueden variar, pero deben satisfacer conjuntamente las ecuaciones restantes.", nullptr},
    {"system.inconsistent", "A row requires zero to equal a nonzero value. The equations have no simultaneous solution.", "", "Una fila exige que cero sea igual a un valor distinto de cero. El sistema es incompatible: no tiene solución conjunta.", nullptr},
    {"status.unsupported", "A checked method is not available for this equation family. The ordinary Giac result remains available.", "", "El tutor aún no dispone de un método comprobado para esta familia de ecuaciones. El resultado de Giac sigue disponible.", nullptr},
    {"status.partial", "The explanation reached its resource limit. The verified steps shown are partial; completeness is not established.", "", "La explicación ha alcanzado su límite de recursos. Los pasos mostrados están verificados, pero la explicación es parcial y no se ha demostrado que incluya todas las soluciones.", nullptr},
    {"status.check_failed", "The tutor could not verify this derivation. Its instructions are withheld; the ordinary result remains available.", "", "El tutor no ha podido verificar esta derivación y no muestra sus instrucciones. El resultado de Giac sigue disponible.", nullptr},
    {"status.reconciliation", "The checked derivation and the Giac answer adapter disagree. The ordinary result has not been changed.", "", "La derivación comprobada y el resultado de Giac no coinciden. El resultado de Giac no se ha modificado.", nullptr},
    {"product.repeated", "A square is zero only when its base is zero. Solve this one case; the repeated root is counted once.", "", "Un cuadrado solo es cero cuando su base es cero. Resuelve este único caso; la raíz repetida se cuenta una sola vez.", nullptr},
    {"terminal.excluded_all", "Every candidate violates an original denominator restriction, so there are no solutions.", "", "Todos los candidatos incumplen alguna restricción de los denominadores originales. No hay soluciones.", nullptr},
    {"system.add_unit", "Add equation {0} to equation {1} to eliminate {2}.", "rrv", "Suma la ecuación {0} a la ecuación {1} para eliminar {2}.", nullptr},
    {"system.subtract_unit", "Subtract equation {0} from equation {1} to eliminate {2}.", "rrv", "Resta la ecuación {0} a la ecuación {1} para eliminar {2}.", nullptr},
    {"view.alternative", "Case {0}", "i", "Caso {0}", "Cas {0}"},
    {"view.active_alternative", "Case {0}: current step", "i", "Caso {0}: paso actual", nullptr},
    {"view.rejected_alternative", "Case {0}: excluded", "i", "Caso {0}: excluido", nullptr},
    {"view.equation", "Equation {0}", "i", "Ecuación {0}", "Equation {0}"},
    {"view.formula_unavailable", "This formula cannot be displayed in the current view.", "", "Esta fórmula no se puede mostrar en la vista actual.", nullptr},
    {"equation.add_collect", "Add the displayed term to both sides to collect the {0} terms on the left.", "v", "Suma el término indicado a ambos miembros para agrupar los términos con {0} a la izquierda.", nullptr},
    {"equation.subtract_collect", "Subtract the displayed term from both sides to collect the {0} terms on the left.", "v", "Resta el término indicado a ambos miembros para agrupar los términos con {0} a la izquierda.", nullptr},
    {"equation.add_zero_right", "Add {0} to both sides to put zero on the right.", "e", "Suma {0} a ambos miembros para dejar cero a la derecha.", nullptr},
    {"equation.subtract_zero_right", "Subtract {0} from both sides to put zero on the right.", "e", "Resta {0} a ambos miembros para dejar cero a la derecha.", nullptr},
    {"equation.add_displayed_isolate", "Add the displayed amount to both sides to isolate the term containing {0}.", "v", "Suma la cantidad indicada a ambos miembros para dejar aislado el término que contiene {0}.", nullptr},
    {"equation.subtract_displayed_isolate", "Subtract the displayed amount from both sides to isolate the term containing {0}.", "v", "Resta la cantidad indicada a ambos miembros para dejar aislado el término que contiene {0}.", nullptr},
    {"equation.add_displayed_zero_right", "Add the displayed expression to both sides to put zero on the right.", "", "Suma la expresión indicada a ambos miembros para dejar cero a la derecha.", nullptr},
    {"equation.subtract_displayed_zero_right", "Subtract the displayed expression from both sides to put zero on the right.", "", "Resta la expresión indicada a ambos miembros para dejar cero a la derecha.", nullptr},
    {"equation.add_system", "Add {0} to both sides to collect the variable terms on the left.", "e", "Suma {0} a ambos miembros para agrupar los términos con incógnitas a la izquierda.", nullptr},
    {"equation.subtract_system", "Subtract {0} from both sides to collect the variable terms on the left.", "e", "Resta {0} a ambos miembros para agrupar los términos con incógnitas a la izquierda.", nullptr},
    {"equation.add_displayed_system", "Add the displayed expression to both sides to collect the variable terms on the left.", "", "Suma la expresión indicada a ambos miembros para agrupar los términos con incógnitas a la izquierda.", nullptr},
    {"equation.subtract_displayed_system", "Subtract the displayed expression from both sides to collect the variable terms on the left.", "", "Resta la expresión indicada a ambos miembros para agrupar los términos con incógnitas a la izquierda.", nullptr},
    {"equation.divide_square", "Divide both sides by {0} to isolate the square of {1}.", "ev", "Divide ambos miembros entre {0} para aislar el cuadrado de {1}.", nullptr},
    {"equation.divide_displayed", "Divide both sides by the displayed coefficient to isolate {0}.", "v", "Divide ambos miembros entre el coeficiente indicado para despejar {0}.", nullptr},
    {"equation.divide_displayed_square", "Divide both sides by the displayed coefficient to isolate the square of {0}.", "v", "Divide ambos miembros entre el coeficiente indicado para aislar el cuadrado de {0}.", nullptr},
    {"square.zero", "Zero has just one square root: zero.", "", "Cero tiene una sola raíz cuadrada: cero.", nullptr},
    {"system.scale_displayed", "Divide equation {0} by the displayed coefficient to make the coefficient of {1} equal to 1.", "rv", "Divide la ecuación {0} entre el coeficiente indicado para que el coeficiente de {1} sea 1.", nullptr},
    {"system.add_scaled", "Add the displayed multiple of equation {0} to equation {1} to eliminate {2}.", "rrv", "Suma el múltiplo indicado de la ecuación {0} a la ecuación {1} para eliminar {2}.", nullptr},
    {"system.subtract_scaled", "Subtract the displayed multiple of equation {0} from equation {1} to eliminate {2}.", "rrv", "Resta el múltiplo indicado de la ecuación {0} a la ecuación {1} para eliminar {2}.", nullptr},
    {"view.step", "Step {0} of {1}", "ii", "Paso {0} de {1}", "Etape {0} sur {1}"},
    {"view.summary", "Summary {0} of {1}", "ii", "Resumen {0} de {1}", "Resume {0} sur {1}"},
    {"view.start", "Starting equations", "", "Ecuaciones iniciales", "Equations initiales"},
    {"view.solutions", "Solutions", "", "Soluciones", "Solutions"},
    {"view.no_solution", "No solution", "", "Sin solución", "Aucune solution"},
    {"view.family", "Solution family", "", "Familia de soluciones", "Famille de solutions"},
    {"view.already", "Already solved", "", "Ya está resuelta", "Deja resolue"},
    {"view.identity", "An identity", "", "Una identidad", "Une identite"},
    {"view.guided_hint", "<> Step  ^v Scroll  EXE Summary  VAR Pan  BACK", "", "<> Paso ^v Despl. EXE Resumen VAR Horiz. BACK", nullptr},
    {"view.summary_hint", "<> Step  ^v Scroll  EXE Guided  VAR Pan  BACK", "", "<> Paso ^v Despl. EXE Guiada VAR Horiz. BACK", nullptr},
    {"view.before", "Before", "", "Antes", "Avant"},
    {"view.after", "After", "", "Después", "Apres"},
    {"view.restrictions", "Restrictions ({0})", "i", "Restricciones ({0})", "Restrictions ({0})"},
    {"view.operand", "Amount used on both sides", "", "Cantidad aplicada a ambos miembros", nullptr},
    {"view.unchanged_case", "Case {0}: unchanged", "i", "Caso {0}: sin cambios", "Cas {0} : inchange"},
    {"view.solution_number", "Solution {0}", "i", "Solución {0}", "Solution {0}"},
    {"view.system_together", "One simultaneous solution", "", "Una solución conjunta", nullptr},
    {"view.verified", "Verified", "", "Verificado", "Verifie"},
    {"view.partial_short", "Partial", "", "Parcial", "Partiel"},
    {"view.coefficients", "Identify the coefficients", "", "Identifica los coeficientes", nullptr},
    {"view.discriminant", "Compute the discriminant", "", "Calcula el discriminante", nullptr},
    {"view.quadratic_formula", "Use the quadratic formula", "", "Aplica la fórmula general", nullptr},
    {"view.substitution", "Substitute the checked values", "", "Sustituye los valores comprobados", nullptr},
    {"view.match_coefficients", "Write the equation with zero on the right, then match its coefficients with the standard form.", "", "Escribe la ecuación con cero a la derecha e identifica sus coeficientes comparándola con la forma general.", "Ecrire l'equation avec zero a droite, puis identifier les coefficients dans la forme standard."},
    {"view.compute_discriminant", "The discriminant determines which square root the formula uses.", "", "El discriminante determina la raíz cuadrada que aparece en la fórmula.", nullptr},
    {"view.use_formula", "Insert the coefficients and discriminant. The two signs give the possible solutions.", "", "Sustituye los coeficientes y el discriminante. Los dos signos dan las posibles soluciones.", nullptr},
    {"view.read_roots", "Evaluate the minus and plus choices to obtain the solutions shown.", "", "Calcula las dos opciones, con el signo menos y con el signo más, para obtener las soluciones indicadas.", nullptr},
    {"view.standard_form", "Standard form", "", "Forma general", nullptr},
    {"view.coefficient_values", "Checked coefficients", "", "Coeficientes comprobados", nullptr},
    {"view.definition", "Definition", "", "Definición", nullptr},
    {"view.evaluated", "With these values", "", "Con estos valores", nullptr},
    {"view.starting_system", "Find values that satisfy all these equations together.", "", "Busca valores que satisfagan todas estas ecuaciones a la vez.", nullptr},
    {"view.start_equation", "Starting equation", "", "Ecuación inicial", nullptr},
    {"view.original", "Original equation", "", "Ecuación original", nullptr},
    {"view.current", "Current equation", "", "Ecuación actual", nullptr},
    {"view.unavailable", "This teaching page could not be prepared. The ordinary result is available with BACK.", "", "No se ha podido preparar esta página. Pulsa BACK para volver al resultado de Giac.", nullptr},
    {"view.expand", "Expand the expression", "", "Desarrolla la expresión", nullptr},
    {"view.collect", "Collect like terms", "", "Reduce los términos semejantes", nullptr},
    {"view.balance", "Balance both sides", "", "Opera en ambos miembros", nullptr},
    {"view.divide", "Divide both sides", "", "Divide ambos miembros", "Diviser les deux membres"},
    {"view.clear", "Clear the denominators", "", "Elimina los denominadores", nullptr},
    {"view.factor", "Factor the polynomial", "", "Factoriza el polinomio", nullptr},
    {"view.square_roots", "Consider both square roots", "", "Considera ambas raíces cuadradas", nullptr},
    {"view.cases", "Use the zero-product property", "", "Aplica la propiedad del producto nulo", nullptr},
    {"view.check_candidate", "Check the original domain", "", "Comprueba el dominio original", nullptr},
    {"view.row_swap", "Swap the equations", "", "Intercambia las ecuaciones", nullptr},
    {"view.row_scale", "Scale one equation", "", "Multiplica o divide una ecuación", nullptr},
    {"view.row_add", "Eliminate a variable", "", "Elimina una incógnita", nullptr},
    {"view.saved_values", "Use the saved values", "", "Usa los valores guardados", nullptr},
    {"view.domain", "Preserve the domain", "", "Conserva el dominio", nullptr},
    {"view.solve_first", "Solve the current equations first.", "", "Resuelve primero las ecuaciones actuales.", nullptr},
    {"view.zero_right", "Equivalent form with zero on the right", "", "Forma equivalente con cero a la derecha", "Forme equivalente avec zero a droite"},
    {"abs.range", "An absolute value cannot be negative. The other side must satisfy this condition.", "", "Un valor absoluto no puede ser negativo. El otro miembro debe cumplir esta condición.", "Une valeur absolue ne peut pas etre negative. L autre membre doit respecter cette condition."},
    {"radical.range", "The principal square root is nonnegative, so the other side must also be nonnegative.", "", "La raíz cuadrada principal es mayor o igual que cero. El otro miembro también debe serlo.", nullptr},
    {"radical.domain", "For a real square root, the expression under the root must be nonnegative.", "", "Para que exista una raíz cuadrada real, su radicando debe ser mayor o igual que cero.", nullptr},
    {"abs.cases", "An absolute value equals the other side when its inside expression equals that value or its opposite. Solve both cases under the sign condition.", "", "El valor absoluto coincide con el otro miembro cuando la expresión interior es igual a ese valor o a su opuesto. Resuelve ambos casos conservando la condición de signo.", nullptr},
    {"abs.zero", "An absolute value is zero exactly when the expression inside it is zero. Only one case is needed.", "", "Un valor absoluto es cero exactamente cuando la expresión interior es cero. Basta con un caso.", nullptr},
    {"radical.square", "Square both sides to remove the root. Squaring can introduce extra candidates; check each one in the original equation.", "", "Eleva ambos miembros al cuadrado para eliminar la raíz. Pueden aparecer candidatos que no satisfagan la ecuación original; comprueba cada uno en ella.", "Elever les deux membres au carre pour eliminer la racine. Verifier chaque candidat dans l equation initiale."},
    {"abs.impossible", "An absolute value is nonnegative, so it cannot equal this negative value. There is no real solution.", "", "Un valor absoluto es mayor o igual que cero y no puede ser igual a este valor negativo. No hay solución real.", nullptr},
    {"radical.impossible", "The principal square root is nonnegative, so it cannot equal this negative value. There is no real solution.", "", "La raíz cuadrada principal es mayor o igual que cero y no puede ser igual a este valor negativo. No hay solución real.", nullptr},
    {"candidate.original_valid", "This candidate satisfies every sign and domain condition and the original equation.", "", "Este candidato satisface todas las condiciones de signo y dominio, y también la ecuación original.", nullptr},
    {"candidate.original_invalid", "Reject this candidate: substituting it into the original equation gives unequal sides.", "", "Descarta este candidato: al sustituirlo en la ecuación original, los miembros no son iguales.", nullptr},
    {"candidate.sign_invalid", "Reject this candidate: the isolated other side is negative.", "", "Descarta este candidato: al sustituirlo, el otro miembro resulta negativo, pero debe ser mayor o igual que cero.", nullptr},
    {"candidate.radicand_invalid", "Reject this candidate: the original radicand is negative.", "", "Descarta este candidato: el radicando original es negativo.", nullptr},
    {"candidate.duplicate", "This value is already an accepted solution from another case. Count it only once.", "", "Este valor ya es una solución aceptada de otro caso. Cuéntalo una sola vez.", nullptr},
    {"abs.add", "Add the displayed term to both sides to isolate the absolute-value term.", "", "Suma el término indicado a ambos miembros para aislar el término con valor absoluto.", nullptr},
    {"abs.subtract", "Subtract the displayed term from both sides to isolate the absolute-value term.", "", "Resta el término indicado a ambos miembros para aislar el término con valor absoluto.", nullptr},
    {"abs.divide", "Divide both sides by the displayed nonzero coefficient to isolate the absolute value.", "", "Divide ambos miembros entre el coeficiente indicado, distinto de cero, para aislar el valor absoluto.", nullptr},
    {"radical.add", "Add the displayed term to both sides to isolate the square-root term.", "", "Suma el término indicado a ambos miembros para aislar el término con raíz cuadrada.", nullptr},
    {"radical.subtract", "Subtract the displayed term from both sides to isolate the square-root term.", "", "Resta el término indicado a ambos miembros para aislar el término con raíz cuadrada.", nullptr},
    {"radical.divide", "Divide both sides by the displayed nonzero coefficient to isolate the square root.", "", "Divide ambos miembros entre el coeficiente indicado, distinto de cero, para aislar la raíz cuadrada.", nullptr},
    {"view.sign", "Preserve the sign condition", "", "Conserva la condición de signo", nullptr},
    {"view.abs_cases", "Split into cases", "", "Separa en casos", nullptr},
    {"view.square_both", "Square both sides", "", "Eleva ambos miembros al cuadrado", nullptr},
    {"view.original_check", "Check the original equation", "", "Comprueba la ecuación original", nullptr},
    {"view.empty_case", "This case has no real solution", "", "Este caso no tiene solución real", nullptr},
    {"view.subcase", "Case {0}.{1}", "ii", "Caso {0}.{1}", "Cas {0}.{1}"},
    {"view.active_subcase", "Case {0}.{1}: current step", "ii", "Caso {0}.{1}: paso actual", nullptr},
    {"view.rejected_subcase", "Case {0}.{1}: rejected candidate", "ii", "Caso {0}.{1}: candidato descartado", nullptr},
    {"view.candidates", "Candidates", "", "Candidatos", "Candidats"},
    {"view.read_candidates", "These values solve this case equation. Check each candidate in the original equation and its conditions.", "", "Estos valores satisfacen la ecuación de este caso. Comprueba cada candidato en la ecuación original y verifica que cumpla sus condiciones.", nullptr},
    {"log.domain", "A real logarithm is defined only when its argument is positive.", "", "El logaritmo real solo está definido cuando su argumento es positivo.", "Un logarithme reel exige un argument positif."},
    {"base.valid", "The base must be positive and different from one.", "", "La base debe ser positiva y distinta de uno.", nullptr},
    {"exponential.range", "A real exponential value is always positive. The other side must be positive too.", "", "Una función exponencial real siempre toma valores positivos. El otro miembro también debe ser positivo.", "Une valeur exponentielle reelle est toujours positive."},
    {"exponential.injective", "With the same valid base, equal exponential values have equal exponents.", "", "Con una misma base válida, dos valores exponenciales iguales tienen exponentes iguales.", nullptr},
    {"exponential.inverse", "Take the natural logarithm of both sides. It is the inverse of the real exponential function.", "", "Aplica el logaritmo natural a ambos miembros. Es la función inversa de la exponencial real.", nullptr},
    {"exponential.base_inverse", "Take natural logarithms of both sides, then divide by the logarithm of the base.", "", "Aplica el logaritmo natural a ambos miembros y divide después entre el logaritmo de la base.", nullptr},
    {"exponential.exact_power", "Express the other side as an exact power of the same base.", "", "Expresa el otro miembro como una potencia exacta de la misma base.", nullptr},
    {"log.inverse", "Exponentiate both sides. The natural logarithm and exponential are inverse functions on the valid real domain.", "", "Aplica la función exponencial a ambos miembros. El logaritmo natural y la exponencial son funciones inversas en el dominio real válido.", "La fonction exponentielle inverse le logarithme naturel sur son domaine reel."},
    {"log.base_inverse", "Use each side as the exponent of the valid base. This inverts the logarithm on its positive domain.", "", "Usa cada miembro como exponente de la base válida. Esta operación invierte el logaritmo en su dominio positivo.", nullptr},
    {"log.injective", "Logarithms with the same valid base are one-to-one on positive arguments, so their arguments must be equal.", "", "Los logaritmos con la misma base válida son inyectivos para argumentos positivos: si sus valores son iguales, sus argumentos también lo son.", nullptr},
    {"terminal.positive_range", "A real exponential value is always positive. It cannot equal zero or a negative value, so there is no real solution.", "", "Una función exponencial real siempre toma valores positivos. No puede valer cero ni un número negativo, por lo que no hay solución real.", nullptr},
    {"transcendental.add", "Add the displayed term to both sides to isolate the function.", "", "Suma el término indicado a ambos miembros para aislar la función.", nullptr},
    {"transcendental.subtract", "Subtract the displayed term from both sides to isolate the function.", "", "Resta el término indicado a ambos miembros para aislar la función.", nullptr},
    {"transcendental.divide", "Divide both sides by the displayed nonzero coefficient to isolate the function.", "", "Divide ambos miembros entre el coeficiente indicado, distinto de cero, para aislar la función.", nullptr},
    {"candidate.log_domain", "Reject this candidate: a logarithm argument is not positive.", "", "Descarta este candidato: el argumento de un logaritmo no es positivo.", nullptr},
    {"view.injective", "Equate the arguments or exponents", "", "Iguala los argumentos o exponentes", nullptr},
    {"view.inverse", "Use the inverse function", "", "Usa la función inversa", nullptr},
    {"view.exact_power", "Recognize a common base", "", "Busca una base común", nullptr},
    {"view.add_amount", "Add {0} to both sides.", "e", "Suma {0} a ambos miembros.", "Ajouter {0} aux deux membres."},
    {"view.subtract_amount", "Subtract {0} from both sides.", "e", "Resta {0} a ambos miembros.", "Soustraire {0} des deux membres."},
    {"view.divide_amount", "Divide both sides by {0}.", "e", "Divide ambos miembros entre {0}.", "Diviser les deux membres par {0}."},
    {"trig.range", "Sine and cosine values lie between -1 and 1. The target is inside this range.", "", "El seno y el coseno toman valores entre -1 y 1. El valor buscado está dentro de ese intervalo.", nullptr},
    {"trig.principal", "Find the principal inverse angle in the standard real inverse range.", "", "Usa la función trigonométrica inversa para obtener el ángulo principal.", nullptr},
    {"trig.sine_families", "Sine has the same value at supplementary angles and repeats every full turn. Include both families, with any integer k.", "", "El seno tiene el mismo valor en ángulos suplementarios y se repite cada vuelta completa. Incluye ambas familias, con cualquier entero k.", nullptr},
    {"trig.cosine_families", "Cosine has the same value at opposite angles and repeats every full turn. Include both families, with any integer k.", "", "El coseno tiene el mismo valor en ángulos opuestos y se repite cada vuelta completa. Incluye ambas familias, con cualquier entero k.", nullptr},
    {"trig.tangent_families", "Tangent repeats every half turn. Its principal inverse lies strictly between the poles, so this family is defined for every integer k.", "", "La tangente se repite cada media vuelta. Su ángulo principal está estrictamente entre los polos, así que esta familia está definida para cualquier entero k.", nullptr},
    {"family.subtract", "Balance every family relation to remove the constant. The period is unchanged.", "", "Opera en ambos miembros de cada relación para eliminar el término constante. El periodo no cambia.", nullptr},
    {"family.divide", "Divide the whole relation by the nonzero coefficient, including both the offset and the period.", "", "Divide toda la relación entre el coeficiente distinto de cero, incluidos el término independiente y el periodo.", nullptr},
    {"family.normalize", "A negative period can be reversed because k ranges over all integers. Merge families only when their offsets differ by an exact integer number of periods.", "", "Se puede invertir el signo del periodo porque k recorre todos los enteros. Une dos familias solo si sus términos independientes difieren en un número entero exacto de periodos.", nullptr},
    {"family.finish", "These periodic families contain all real solutions. Every integer k is allowed; the complete families satisfy the original equation.", "", "Estas familias periódicas contienen todas las soluciones reales. El parámetro k puede tomar cualquier valor entero y todas las familias satisfacen la ecuación original.", nullptr},
    {"trig.impossible", "Sine and cosine always lie between -1 and 1 for real angles. This target is outside that range, so there is no real solution.", "", "Para ángulos reales, el seno y el coseno siempre están entre -1 y 1. El valor buscado queda fuera de ese intervalo, por lo que no hay solución real.", nullptr},
    {"view.periodic", "Periodic solution families", "", "Soluciones periódicas", nullptr},
    {"view.integer", "k is any integer.", "", "El parámetro k puede tomar cualquier valor entero.", "k est un entier quelconque."},
    {"view.family_number", "Family {0}", "i", "Familia {0}", nullptr},
    {"view.principal", "Principal angle", "", "Ángulo principal", nullptr},
    {"view.range", "Check the range", "", "Comprueba los valores posibles", nullptr},
    {"view.representatives", "The ordinary result lists representatives. These checked families include every period.", "", "El resultado de Giac muestra representantes. Estas familias comprobadas abarcan todos los periodos.", nullptr},
    {"trig.tangent_domain", "Tangent accepts every finite real target. Its argument must avoid the poles where cosine is zero.", "", "La tangente puede tomar cualquier valor real finito. Su argumento debe evitar los polos, donde el coseno es cero.", nullptr},
    {"view.degree_convention", "Angles are in degrees. An inverse marked rad is evaluated in radians before the exact degree conversion.", "", "Los ángulos están en grados. Una inversa marcada con rad se evalúa en radianes antes de la conversión exacta a grados.", "Les angles sont en degres ; rad indique une inverse en radians avant conversion."},
    {"view.representatives_title", "Giac representatives", "", "Representantes de Giac", "Representants de Giac"},
    {"view.periodic_agreement", "Giac independently returned the same complete solution set.", "", "Giac ha obtenido de forma independiente el mismo conjunto completo de soluciones.", "Giac a obtenu independamment le meme ensemble complet."},
    {"view.periodic_independent", "Results contains Giac's independent periodic answer. Exact set comparison is not established.", "", "Resultados contiene la respuesta periódica independiente de Giac. No se ha establecido la igualdad exacta de los conjuntos.", nullptr},
    {"view.periodic_results", "Periodic solutions", "", "Soluciones periódicas", "Solutions periodiques"},
    {"view.or", "or", "", "o", "ou"},

    {"view.steps", "Steps", "", "Pasos", nullptr},
    {"view.result", "Result", "", "Resultado", nullptr},
    {"view.result_hint", "Arrows Scroll   EXE Equations   TOOLBOX Steps", "", "^v Despl. <> Horiz. EXE Ecuac. TOOLBOX Pasos", nullptr},
    {"view.periodic_hint", "Arrows Scroll/Pan   BACK Equations   TOOLBOX Steps", "", "^v Despl. <> Horiz. BACK Ecuac. TOOLBOX Pasos", nullptr},
    {"view.result_pages_hint", "VAR Page   Arrows Scroll/Pan   BACK Equations", "", "VAR Pág. ^v Despl. <> Horiz. BACK TOOLBOX Pasos", nullptr},
    {"view.recovery_hint", "BACK Equations   HOME", "", "BACK Ecuaciones   HOME Inicio", nullptr},
    {"view.solve_again", "Solve again", "", "Resuelve de nuevo", nullptr},
    {"view.invalid_equation", "Invalid equation", "", "Ecuación no válida", nullptr},
    {"view.unresolved", "Unresolved / unsupported", "", "Sin resolver / no admitido", nullptr},
    {"view.no_memory", "Not enough memory", "", "Memoria insuficiente", nullptr},
    {"view.solve_failed", "Solve failed", "", "No se ha podido resolver", nullptr},
    {"view.solve_error", "The equation could not be solved. Return to the equations to review the input or try again.", "", "No se ha podido resolver la ecuación. Vuelve a las ecuaciones para revisar los datos o intentarlo de nuevo.", nullptr},
    {"view.presentation_unavailable", "Presentation unavailable", "", "Vista no disponible", nullptr},
    {"view.no_complex", "No solutions in the complex domain.", "", "No hay soluciones en el dominio complejo.", nullptr},
    {"view.no_real", "No solutions in the real domain.", "", "No hay soluciones en el dominio real.", nullptr},
    {"view.conditional_family", "Conditional family", "", "Familia con restricciones", nullptr},
    {"view.dependent_system", "Dependent system / family", "", "Sistema dependiente / familia", nullptr},
    {"view.tuple2", "Tuple order: x, y", "", "Orden de los valores: x, y", nullptr},
    {"view.tuple3", "Tuple order: x, y, z", "", "Orden de los valores: x, y, z", nullptr},
    {"view.family_relations", "Parameters must satisfy these relations.", "", "Los parámetros deben satisfacer estas relaciones.", nullptr},
    {"view.unresolved_exclusions", "Only where the original equations are defined; exclusions remain unresolved.", "", "Solo donde estén definidas las ecuaciones originales; las exclusiones siguen sin determinarse.", nullptr},
    {"view.conditional_identity", "Conditional identity", "", "Identidad con restricciones", nullptr},
    {"view.conditional_explanation", "Equality holds where the original expression is defined. Domain exclusions remain unresolved; this is not an unrestricted solution set.", "", "La igualdad se cumple donde está definida la expresión original. Las exclusiones del dominio siguen sin determinarse; no es un conjunto de soluciones sin restricciones.", nullptr},
    {"view.all_values", "Identity / all values", "", "Identidad / todos los valores", nullptr},
    {"view.every_complex", "Every complex x satisfies the equation.", "", "Cualquier valor complejo de x satisface la ecuación.", nullptr},
    {"view.every_real", "Every real x satisfies the equation.", "", "Cualquier valor real de x satisface la ecuación.", nullptr},
    {"view.unresolved_result", "Unresolved result", "", "Resultado sin resolver", nullptr},
    {"view.numerical_candidates", "Numerical candidates", "", "Candidatos numéricos", nullptr},
    {"view.numerical_note", "Numerical candidates from Giac. Completeness is not established.", "", "Candidatos numéricos de Giac. No se ha demostrado que incluyan todas las soluciones.", nullptr},
    {"view.candidate_number", "Candidate {0}", "i", "Candidato {0}", nullptr},
    {"view.exact_text", "Exact results (text)", "", "Resultados exactos (texto)", nullptr},
    {"view.display_limit", "Display limit: remaining technical text is not shown.", "", "Límite de visualización: no se muestra el resto del texto técnico.", nullptr},
    {"view.page_real", "Solution {0}/{1} | Real", "ii", "Solución {0}/{1} | Real", nullptr},
    {"view.page_complex", "Solution {0}/{1} | Complex", "ii", "Solución {0}/{1} | Complejo", nullptr},
    {"view.count_real_one", "1 solution | x | Real", "", "1 solución | x | Real", nullptr},
    {"view.count_real_many", "{0} solutions | x | Real", "i", "{0} soluciones | x | Real", nullptr},
    {"view.count_complex_one", "1 solution | x | Complex", "", "1 solución | x | Complejo", nullptr},
    {"view.count_complex_many", "{0} solutions | x | Complex", "i", "{0} soluciones | x | Complejo", nullptr},
    {"view.language", "Language", "", "Idioma", nullptr},
    {"view.english", "English", "", "English", nullptr},
    {"view.spanish", "Español", "", "Español", nullptr},
    {"view.solving", "Solving with Giac...", "", "Resolviendo con Giac...", nullptr},
    {"view.wait", "Please wait", "", "Espera un momento", nullptr},
    {"view.settings_hint", "Navigate   Left/Right Adjust   BACK Exit", "", "^v Elige  </> Cambia  BACK Vuelve", nullptr},
    {"substitution.define", "Use an auxiliary variable for the repeated expression. Its definition preserves the original domain.", "", "Sustituye la expresión que se repite por una variable auxiliar. Su definición conserva el dominio original.", nullptr},
    {"substitution.rewrite", "Rewrite the equation in the auxiliary variable using the checked power identities.", "", "Reescribe la ecuación en la variable auxiliar mediante las identidades de potencias comprobadas.", nullptr},
    {"substitution.auxiliary", "Solve the auxiliary equation completely. These are values of the auxiliary variable, not yet solutions for the original unknown.", "", "Resuelve por completo la ecuación auxiliar. Estos son valores de la variable auxiliar, no las soluciones de la incógnita original.", nullptr},
    {"substitution.reject_square", "Discard this auxiliary value: a real square cannot be negative.", "", "Descarta este valor auxiliar: un cuadrado real no puede ser negativo.", nullptr},
    {"substitution.reject_exp", "Discard this auxiliary value: a real exponential is strictly positive, so zero and negative values are impossible.", "", "Descarta este valor auxiliar: una exponencial real es positiva y no puede valer cero ni tomar valores negativos.", nullptr},
    {"substitution.reject_trig", "Discard this auxiliary value: sine and cosine take values only between -1 and 1, including both endpoints.", "", "Descarta este valor auxiliar: el seno y el coseno solo toman valores entre −1 y 1, incluidos ambos extremos.", nullptr},
    {"substitution.pullback", "Return to the original unknown. Solve the displayed equation for this auxiliary value, keeping every preimage.", "", "Vuelve a la incógnita original. Resuelve la ecuación indicada para este valor auxiliar sin perder ninguna preimagen.", nullptr},
    {"substitution.finish", "Every auxiliary value has been accounted for. Combine the checked preimages, counting each solution only once and keeping the original restrictions.", "", "Se han considerado todos los valores auxiliares. Reúne sus preimágenes comprobadas, cuenta cada solución una sola vez y conserva las restricciones originales.", nullptr},
    {"substitution.square", "Consider both signs of the square root. When the square is zero, both signs give the same case.", "", "Considera ambos signos de la raíz cuadrada. Si el cuadrado es cero, ambos signos dan el mismo caso.", nullptr},
    {"view.auxiliary", "Auxiliary values", "", "Valores de la variable auxiliar", nullptr},
    {"view.pullback", "Return to the original unknown", "", "Vuelve a la incógnita original", nullptr},
    {"view.substitution_range", "Possible auxiliary values", "", "Valores posibles de la auxiliar", nullptr},
    {"view.define_substitution", "Define an auxiliary variable", "", "Define una variable auxiliar", nullptr},
    {"view.auxiliary_equation", "Auxiliary equation", "", "Ecuación auxiliar", nullptr},
    {"view.auxiliary_number", "Auxiliary value {0}", "i", "Valor auxiliar {0}", nullptr},
    {"view.return_equation", "Equation for this auxiliary value", "", "Ecuación para este valor auxiliar", nullptr},
    {"substitution.finish_empty", "Every auxiliary value has been considered. None has a valid real preimage, so the original equation has no real solution.", "", "Se han considerado todos los valores auxiliares. Ninguno tiene una preimagen real válida, por lo que la ecuación original no tiene solución real.", nullptr},
    {"view.periodic_groups_hint", "Arrows Scroll/Pan  VAR Page  BACK  TOOLBOX Steps", "", "^v Despl. <> Horiz. VAR Página BACK TOOLBOX Pasos", nullptr},
    {"view.periodic_page", "Periodic solutions {0}/{1}", "ii", "Soluciones periódicas {0}/{1}", nullptr},
};
static_assert(sizeof(catalog) / sizeof(catalog[0]) == unsigned(Message::Count), "catalog coverage");
char schemaType(ParameterKind p) {
    return p == ParameterKind::Expression ? 'e'
           : p == ParameterKind::Variable ? 'v'
           : p == ParameterKind::Row      ? 'r'
                                          : 'i';
}
void hashBytes(uint64_t &hash, const std::string &text) {
    for (unsigned char c : text) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    hash ^= 255;
    hash *= 1099511628211ull;
}
std::string quote(const std::string &s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\')
            out += '\\';
        if (c == '\n')
            out += "\\n";
        else if (c >= 32)
            out += char(c);
    }
    return out + '"';
}
} // namespace
const char *ruleId(Rule r) {
    static constexpr const char *ids[] = {
        "domain.nonzero",     "values.substitute", "algebra.expand",    "algebra.collect",
        "equation.swap",      "equation.add",      "equation.divide",   "rational.clear",
        "polynomial.factor",  "square.branches",   "quadratic.formula", "product.zero",
        "candidate.excluded", "terminal.isolated", "terminal.identity", "terminal.contradiction",
        "terminal.finish",    "system.swap",       "system.scale",      "system.add",
        "system.finish", "condition.range", "abs.cases", "radical.candidates", "terminal.nonnegative",
        "candidate.original_valid", "candidate.original_invalid", "candidate.duplicate", "domain.radicand_nonnegative", "domain.positive", "domain.base", "exponential.range", "exponential.injective", "exponential.inverse", "exponential.exact_power", "logarithm.inverse", "logarithm.injective", "terminal.positive_range", "trig.range", "trig.principal", "trig.families", "family.shift", "family.divide", "family.normalize", "family.finish", "trig.impossible",
        "substitution.define", "substitution.rewrite", "substitution.solve_auxiliary", "substitution.reject_range", "substitution.pullback", "substitution.finish", "square.preimage"};
    return unsigned(r) < sizeof(ids) / sizeof(ids[0]) ? ids[unsigned(r)] : "unknown";
}
const char *messageKey(Message k) {
    return unsigned(k) < unsigned(Message::Count) ? catalog[unsigned(k)].key : "unknown";
}
bool hasTranslation(Message k, Locale locale) {
    locale = baseLocale(locale);
    if(unsigned(k)>=unsigned(Message::Count))return false;
    const auto& e=catalog[unsigned(k)];
    const char* value=locale==Locale::Spanish?e.spanish:locale==Locale::French?e.french:e.english;
    return value && value[0];
}
const char *messageSchema(Message k) {
    return unsigned(k)<unsigned(Message::Count)?catalog[unsigned(k)].schema:nullptr;
}
const char *messageFallback(Message k, Locale locale) {
    locale = baseLocale(locale);
    if(unsigned(k)>=unsigned(Message::Count)||catalog[unsigned(k)].schema[0])return "";
    const auto& e=catalog[unsigned(k)];
    return locale==Locale::Spanish&&e.spanish?e.spanish:locale==Locale::French&&e.french?e.french:e.english;
}
bool validateMessage(Message k, const Vector<Parameter> &p) {
    if (unsigned(k) >= unsigned(Message::Count))
        return false;
    const auto *schema = catalog[unsigned(k)].schema;
    if (std::strlen(schema) != p.size())
        return false;
    for (size_t i = 0; i < p.size(); ++i)
        if (schema[i] != schemaType(p[i].kind) || p[i].value.empty() ||
            p[i].value.size() > Limits::expressionBytes)
            return false;
    return true;
}
bool validateCatalogs() {
    for (const auto &entry : catalog) {
        if(!entry.english || !entry.english[0] || !entry.spanish || !entry.spanish[0])return false;
        for (const char *value : {entry.english, entry.spanish, entry.french}) {
            if (!value)
                continue;
            std::array<unsigned, 8> count{};
            for (size_t i = 0; value[i]; ++i)
                if (value[i] == '{') {
                    if (value[i + 1] < '0' || value[i + 1] > '7' || value[i + 2] != '}')
                        return false;
                    ++count[value[i + 1] - '0'];
                    i += 2;
                }
            for (size_t i = 0; i < count.size(); ++i)
                if (count[i] != (i < std::strlen(entry.schema) ? 1u : 0u))
                    return false;
        }
    }
    return true;
}
std::string explain(Message k, const Vector<Parameter> &p, Locale locale) {
    locale = baseLocale(locale);
    if (!validateMessage(k, p))
        return "[invalid message parameters]";
    const auto &entry = catalog[unsigned(k)];
    const char *value = entry.english;
    if (locale == Locale::Spanish && entry.spanish)
        value = entry.spanish;
    if (locale == Locale::French && entry.french)
        value = entry.french;
    // Explicit per-message English fallback. Parameter values are never localized.
    std::string out;
    for (size_t i = 0; value[i]; ++i) {
        if (value[i] == '{') {
            out += p[value[i + 1] - '0'].value;
            i += 2;
        } else {
            out += value[i];
            if (locale == Locale::Pseudo && std::strchr("aeiou", value[i]))
                out += value[i];
        }
    }
    return locale == Locale::Pseudo ? "[!! " + out + " !!]" : out;
}
namespace {
bool equalTree(const EngineResultNode& a, const EngineResultNode& b, unsigned depth=0) {
    if(depth>Limits::depth || a.kind!=b.kind || a.text!=b.text || a.children.size()!=b.children.size() ||
       a.rows!=b.rows || a.columns!=b.columns || a.leftClosed!=b.leftClosed || a.rightClosed!=b.rightClosed || a.fallbackReason!=b.fallbackReason) return false;
    for(size_t i=0;i<a.children.size();++i) if(!equalTree(a.children[i],b.children[i],depth+1))return false;
    return true;
}
void hashTree(uint64_t& h,const EngineResultNode& n,unsigned depth=0) {
    hashBytes(h,std::to_string(unsigned(n.kind))); hashBytes(h,n.text);
    if(depth>Limits::depth)return;
    for(const auto& c:n.children)hashTree(h,c,depth+1);
}
size_t treeBytes(const EngineResultNode& n,unsigned depth=0) {
    if(depth>Limits::depth)return Limits::retainedBytes;
    size_t bytes=n.text.capacity()+1+n.children.capacity()*sizeof(EngineResultNode);
    for(const auto& c:n.children)bytes+=treeBytes(c,depth+1);
    return bytes;
}
}
bool sameCondition(const Condition& a,const Condition& b) {
    return a.nonzero==b.nonzero && a.source.equation==b.source.equation && a.source.side==b.source.side && a.source.children==b.source.children &&
        a.kind==b.kind && a.role==b.role && a.sourceState==b.sourceState && a.verification==b.verification &&
        bool(a.expression)==bool(b.expression) && (!a.expression || equalTree(*a.expression,*b.expression));
}
bool sameFamilies(const Vector<PeriodicFamily>& a,const Vector<PeriodicFamily>& b) {
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i){const auto& x=a[i];const auto& y=b[i];
        if(x.variable!=y.variable||x.lhs!=y.lhs||x.offset!=y.offset||x.period!=y.period||
           x.binderScope!=y.binderScope||x.binderId!=y.binderId||x.theoremStep!=y.theoremStep||
           x.theoremBranch!=y.theoremBranch||x.domain!=y.domain||x.degrees!=y.degrees||x.originalCheck!=y.originalCheck)return false;
    }return true;
}
uint64_t fingerprint(const State &s) {
    uint64_t h = 1469598103934665603ull;
    hashBytes(h, std::to_string(unsigned(s.conclusion)));
    for (const auto &b : s.branches) {
        hashBytes(h, std::to_string(b.id));
        hashBytes(h, std::to_string(unsigned(b.status)));
        if (b.origin || b.originalCheck != Verdict::Unknown) {
            hashBytes(h, std::to_string(b.origin)); hashBytes(h, std::to_string(unsigned(b.originalCheck)));
        }
        for (const auto &e : b.equations) {
            hashBytes(h, e.lhs);
            hashBytes(h, e.rhs);
        }
    }
    if(s.periodicStage)hashBytes(h,std::to_string(s.periodicStage));
    for(const auto& f:s.families){
        for(const auto* text:{&f.variable,&f.lhs,&f.offset,&f.period})hashBytes(h,*text);
        for(auto n:{f.binderScope,uint64_t(f.binderId),uint64_t(f.theoremStep),uint64_t(f.theoremBranch),uint64_t(f.domain),uint64_t(f.degrees),uint64_t(f.originalCheck)})hashBytes(h,std::to_string(n));
    }
    for (const auto &c : s.conditions) {
        hashBytes(h, c.nonzero);
        if (c.kind != ConditionKind::Nonzero || c.role != ConditionRole::Denominator || c.sourceState || c.verification != Verdict::Unknown || c.expression) {
            hashBytes(h, std::to_string(unsigned(c.kind))); hashBytes(h, std::to_string(unsigned(c.role)));
            hashBytes(h, std::to_string(c.sourceState)); hashBytes(h, std::to_string(unsigned(c.verification)));
            if (c.expression) hashTree(h, *c.expression);
        }
        hashBytes(h, std::to_string(c.source.equation));
        hashBytes(h, std::to_string(c.source.side));
        for (auto i : c.source.children)
            hashBytes(h, std::to_string(i));
    }
    return h;
}
uint64_t fingerprint(const Snapshot &s) {
    uint64_t h = 1469598103934665603ull;
    for (const auto &e : s.authored) {
        hashBytes(h, e.lhs);
        hashBytes(h, e.rhs);
    }
    for (const auto &v : s.variables)
        hashBytes(h, v);
    for (const auto &b : s.bindings) {
        hashBytes(h, b.variable);
        hashBytes(h, b.value);
    }
    for (const auto &b : s.contextValues) {
        hashBytes(h, b.variable);
        hashBytes(h, b.value);
    }
    hashBytes(h, std::to_string(s.inputEpoch));
    hashBytes(h, std::to_string(s.engineGeneration));
    hashBytes(h, s.complex ? "complex" : "real");
    hashBytes(h, s.degrees ? "deg" : "rad");
    if(s.parentScope){hashBytes(h,std::to_string(s.parentScope));hashBytes(h,std::to_string(s.childRole));}
    return h;
}
size_t retainedBytes(const Derivation &d) {
    size_t n = sizeof(d) + d.diagnostic.capacity() + d.states.capacity() * sizeof(State) +
               d.steps.capacity() * sizeof(Step);
    auto eqs = [&](const Vector<Equation> &e) {
        n += e.capacity() * sizeof(Equation);
        for (const auto &x : e)
            n += x.lhs.capacity() + x.rhs.capacity() + 2;
    };
    eqs(d.input.authored);
    n += d.input.variables.capacity() * sizeof(std::string) +
         (d.input.bindings.capacity() + d.input.contextValues.capacity()) * sizeof(Binding);
    for (const auto &v : d.input.variables)
        n += v.capacity() + 1;
    for (const auto &b : d.input.bindings)
        n += b.variable.capacity() + b.value.capacity() + 2;
    for (const auto &b : d.input.contextValues)
        n += b.variable.capacity() + b.value.capacity() + 2;
    for (const auto &s : d.states) {
        n += s.families.capacity()*sizeof(PeriodicFamily);
        for(const auto& f:s.families)n+=f.variable.capacity()+f.lhs.capacity()+f.offset.capacity()+f.period.capacity()+4;
        n += s.branches.capacity() * sizeof(Branch) + s.conditions.capacity() * sizeof(Condition);
        for (const auto &b : s.branches)
            eqs(b.equations);
        for (const auto &c : s.conditions) {
            n += c.nonzero.capacity() + 1 + c.source.children.capacity();
            // Conservative accounting includes shared tree payload per state.
            if(c.expression)n+=sizeof(EngineResultNode)+32+treeBytes(*c.expression);
        }
    }
    for (const auto &s : d.steps) {
        n += s.operand.capacity() + 1 + s.auxiliaries.capacity() * sizeof(std::string) +
             s.affected.capacity() * sizeof(Path) + s.parameters.capacity() * sizeof(Parameter) +
             s.prerequisites.capacity() + s.introduced.capacity() + s.discharged.capacity() +
             s.substeps.capacity() * sizeof(uint16_t);
        for (const auto &a : s.auxiliaries)
            n += a.capacity() + 1;
        for (const auto &p : s.parameters)
            n += p.value.capacity() + 1;
        for (const auto &p : s.affected)
            n += p.children.capacity();
    }
    if(d.composition){
        const auto& c=*d.composition;
        // One composition layer only. Reject cycles before recursive accounting.
        if(c.children.size()>3||c.preimages.size()>2)return Limits::retainedBytes+1;
        size_t steps=d.steps.size();
        for(const auto& child:c.children){if(child.composition)return Limits::retainedBytes+1;steps+=child.steps.size();}
        if(steps>Limits::steps)return Limits::retainedBytes+1;
        n+=sizeof(c)+32+c.definition.capacity()+1+c.children.capacity()*sizeof(Derivation)+c.preimages.capacity()*sizeof(Preimage);
        for(const auto& text:c.coefficients)n+=text.capacity()+1;
        for(const auto& p:c.preimages)n+=p.value.capacity()+1;
        for(const auto& child:c.children)n+=retainedBytes(child)-sizeof(Derivation);
        for(const auto* conditions:{&c.originalConditions,&c.auxiliaryConditions}){
            n+=conditions->capacity()*sizeof(Condition);
            for(const auto& condition:*conditions){n+=condition.nonzero.capacity()+1+condition.source.children.capacity();if(condition.expression)n+=sizeof(EngineResultNode)+32+treeBytes(*condition.expression);}
        }
    }
    return n; // Conservative retained C++ capacity accounting; allocator overhead is measured
              // separately.
}
std::string replayJson(const Derivation &d, Locale locale) {
    std::ostringstream o;
    auto strings = [&](const auto &values) {
        o << '[';
        bool first = true;
        for (const auto &v : values) {
            if (!first)
                o << ',';
            first = false;
            o << quote(v);
        }
        o << ']';
    };
    auto numbers = [&](const auto &values) {
        o << '[';
        bool first = true;
        for (auto v : values) {
            if (!first)
                o << ',';
            first = false;
            o << unsigned(v);
        }
        o << ']';
    };
    auto equations = [&](const auto &values) {
        o << '[';
        bool first = true;
        for (const auto &e : values) {
            if (!first)
                o << ',';
            first = false;
            o << '[' << quote(e.lhs) << ',' << quote(e.rhs) << ']';
        }
        o << ']';
    };
    auto bindings = [&](const auto &values) {
        o << '[';
        bool first = true;
        for (const auto &b : values) {
            if (!first)
                o << ',';
            first = false;
            o << '[' << quote(b.variable) << ',' << quote(b.value) << ']';
        }
        o << ']';
    };
    auto path = [&](const Path &p) {
        o << "{\"equation\":" << unsigned(p.equation) << ",\"side\":" << unsigned(p.side)
          << ",\"children\":";
        numbers(p.children);
        o << '}';
    };
    o << "{\"version\":1,\"snapshot\":" << fingerprint(d.input)
      << ",\"epoch\":" << d.input.inputEpoch << ",\"generation\":" << d.input.engineGeneration
      << ",\"complex\":" << d.input.complex << ",\"degrees\":" << d.input.degrees
      << ",\"authored\":";
    equations(d.input.authored);
    o << ",\"variables\":";
    strings(d.input.variables);
    o << ",\"bindings\":";
    bindings(d.input.bindings);
    o << ",\"contextValues\":";
    bindings(d.input.contextValues);
    o << ",\"status\":" << unsigned(d.status) << ",\"validity\":" << unsigned(d.validity)
      << ",\"completeness\":" << unsigned(d.completeness)
      << ",\"candidates\":" << unsigned(d.candidates)
      << ",\"reconciliation\":" << unsigned(d.reconciliation)
      << ",\"bytes\":" << d.metrics.retainedBytes
      << ",\"vectorHeapBytes\":" << d.metrics.vectorHeapBytes
      << ",\"peakVectorHeapBytes\":" << d.metrics.peakVectorHeapBytes
      << ",\"calls\":" << d.metrics.symbolicCalls << ",\"micros\":" << d.metrics.elapsedMicros
      << ",\"diagnostic\":" << quote(d.diagnostic) << ",\"states\":[";
    for (size_t i = 0; i < d.states.size(); ++i) {
        if (i)
            o << ',';
        const auto &s = d.states[i];
        o << "{\"id\":" << i << ",\"fingerprint\":" << s.fingerprint
          << ",\"conclusion\":" << unsigned(s.conclusion) << ",\"conditions\":[";
        for (size_t j = 0; j < s.conditions.size(); ++j) {
            if (j)
                o << ',';
            o << quote(s.conditions[j].nonzero);
        }
        o << "],\"conditionPaths\":[";
        for (size_t j = 0; j < s.conditions.size(); ++j) {
            if (j)
                o << ',';
            path(s.conditions[j].source);
        }
        o << "]";
        bool typed=false;for(const auto& c:s.conditions)typed=typed || bool(c.expression);
        if(typed) {
            o << ",\"typedConditions\":[";
            for(size_t j=0;j<s.conditions.size();++j) {
                if(j)o<<',';const auto& c=s.conditions[j];
                o<<"{\"kind\":"<<unsigned(c.kind)<<",\"role\":"<<unsigned(c.role)<<",\"sourceState\":"<<c.sourceState
                 <<",\"verification\":"<<unsigned(c.verification)<<",\"expression\":"<<quote(c.nonzero)<<'}';
            }
            o<<']';
        }
        if(s.periodicStage){
            o<<",\"periodicStage\":"<<unsigned(s.periodicStage)<<",\"families\":[";
            for(size_t j=0;j<s.families.size();++j){if(j)o<<',';const auto& f=s.families[j];
                o<<"{\"variable\":"<<quote(f.variable)<<",\"lhs\":"<<quote(f.lhs)<<",\"offset\":"<<quote(f.offset)<<",\"period\":"<<quote(f.period)
                 <<",\"binderScope\":"<<f.binderScope<<",\"binderId\":"<<f.binderId<<",\"domain\":"<<unsigned(f.domain)<<",\"degrees\":"<<f.degrees
                 <<",\"theoremStep\":"<<f.theoremStep<<",\"theoremBranch\":"<<unsigned(f.theoremBranch)<<",\"originalCheck\":"<<unsigned(f.originalCheck)<<'}';
            }o<<']';
        }
        o << ",\"branches\":[";
        for (size_t j = 0; j < s.branches.size(); ++j) {
            if (j)
                o << ',';
            const auto &b = s.branches[j];
            o << "{\"id\":" << unsigned(b.id)
              << ",\"rejected\":" << (b.status == BranchStatus::Rejected) << ",\"equations\":";
            equations(b.equations);
            if(b.origin)o<<",\"origin\":"<<unsigned(b.origin)<<",\"status\":"<<unsigned(b.status)<<",\"originalCheck\":"<<unsigned(b.originalCheck);
            o << '}';
        }
        o << "]}";
    }
    o << "],\"steps\":[";
    for (size_t i = 0; i < d.steps.size(); ++i) {
        if (i)
            o << ',';
        const auto &s = d.steps[i];
        o << "{\"rule\":" << quote(ruleId(s.rule)) << ",\"before\":" << s.before
          << ",\"after\":" << s.after << ",\"relation\":" << unsigned(s.relation)
          << ",\"verdict\":" << unsigned(s.verification) << ",\"operand\":" << quote(s.operand)
          << ",\"key\":" << quote(messageKey(s.explanation))
          << ",\"text\":" << quote(explain(s.explanation, s.parameters, locale))
          << ",\"group\":" << s.group << ",\"branch\":" << unsigned(s.branch)
          << ",\"row\":" << unsigned(s.row) << ",\"otherRow\":" << unsigned(s.otherRow)
          << ",\"parameters\":[";
        for (size_t j = 0; j < s.parameters.size(); ++j) {
            if (j)
                o << ',';
            o << quote(s.parameters[j].value);
        }
        o << "],\"parameterKinds\":[";
        for (size_t j = 0; j < s.parameters.size(); ++j) {
            if (j)
                o << ',';
            o << unsigned(s.parameters[j].kind);
        }
        o << "],\"auxiliaries\":";
        strings(s.auxiliaries);
        o << ",\"prerequisites\":";
        numbers(s.prerequisites);
        o << ",\"introduced\":";
        numbers(s.introduced);
        o << ",\"discharged\":";
        numbers(s.discharged);
        if(s.caseStart!=UINT16_MAX)o<<",\"caseStart\":"<<s.caseStart<<",\"caseBranch\":"<<unsigned(s.caseBranch);
        o << ",\"substeps\":";
        numbers(s.substeps);
        o << ",\"affected\":[";
        for (size_t j = 0; j < s.affected.size(); ++j) {
            if (j)
                o << ',';
            path(s.affected[j]);
        }
        o << "]}";
    }
    o << "]";
    if(d.input.parentScope)o<<",\"parentScope\":"<<d.input.parentScope<<",\"childRole\":"<<unsigned(d.input.childRole);
    if(d.composition){const auto& c=*d.composition;
        o<<",\"composition\":{\"scope\":"<<c.scope<<",\"auxiliaryId\":"<<c.auxiliaryId<<",\"kind\":"<<unsigned(c.kind)<<",\"sourceSide\":"<<unsigned(c.sourceSide)<<",\"definition\":"<<quote(c.definition)<<",\"coefficients\":";
        strings(c.coefficients);o<<",\"preimages\":[";
        for(size_t i=0;i<c.preimages.size();++i){if(i)o<<',';const auto& p=c.preimages[i];o<<"{\"value\":"<<quote(p.value)<<",\"child\":"<<unsigned(p.child)<<",\"range\":"<<unsigned(p.range)<<'}';}
        o<<"],\"auxiliaryConditions\":[";
        for(size_t i=0;i<c.auxiliaryConditions.size();++i){if(i)o<<',';const auto& p=c.auxiliaryConditions[i];o<<"{\"expression\":"<<quote(p.nonzero)<<",\"kind\":"<<unsigned(p.kind)<<",\"role\":"<<unsigned(p.role)<<",\"sourceState\":"<<p.sourceState<<",\"verification\":"<<unsigned(p.verification)<<",\"path\":";path(p.source);o<<'}';}
        o<<"],\"children\":[";
        for(size_t i=0;i<c.children.size();++i){if(i)o<<',';o<<replayJson(c.children[i],locale);}o<<"]}";
    }
    o<<'}';return o.str();
}

} // namespace tutor
