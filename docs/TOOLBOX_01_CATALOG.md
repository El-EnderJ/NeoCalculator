# Toolbox catalogue (generated)

Previews use the same AST structure with display-only parameter letters. Insertions contain empty slots. C=Calculation, E=Equations, D=Calculus, G=Grapher. Keyboard entries remain resolvable for existing favorites but are absent from browsing and search. Arguments are in cursor traversal order. No selection capture.

| ID:variant | EN / ES | Recipe | Apps | Discovery | Preview parameters | Fixture arguments | Expected | Help / limits |
|---|---|---|---|---|---|---|---|---|
| 101:0 | Fraction / Fracción | Fraction(0) | CEDG | Keyboard / existing favorites | ab | 6, 2 | 3 | Numerator, then denominator. |
| 102:0 | Power / Potencia | Power(0) | CEDG | Keyboard / existing favorites | ab | 2, 3 | 8 | Base, then exponent. No operand is captured. |
| 103:0 | Absolute value / Valor absoluto | Abs(0) | CED | Toolbox | x | -3 | 3 | Magnitude of a real or complex value. |
| 104:0 | Parentheses / Paréntesis | Paren(0) | CEDG | Keyboard / existing favorites | x | 2 | 2 | Group an expression. |
| 105:0 | Square brackets / Corchetes | Paren(1) | CED | Keyboard / existing favorites | x | 2 | 2 | Scalar grouping, not a list or matrix. |
| 110:0 | Square root / Raíz cuadrada | Root(0) | CEDG | Keyboard / existing favorites | x | 9 | 3 | Enter the radicand. |
| 111:0 | Indexed root / Raíz de índice editable | NthRoot(0) | CED | Toolbox | xn | 3, 8 | 2 | Index, then radicand. RIGHT changes slot. |
| 112:0 | Power of ten / Potencia de diez | PowerTen(0) | CEDG | Keyboard / existing favorites | x | 3 | 1000 | Enter an exponent on the fixed base ten. |
| 120:0 | Logarithm / Logaritmo | LogBase(0) | CEDG | Toolbox | ax | 2, 8 | 3 | Base, then argument. Base must be positive and not 1. |
| 120:1 | Decimal logarithm / Logaritmo decimal | Function(Log) | CEDG | Toolbox | x | 100 | 2 | Base ten logarithm. Enter the argument. |
| 120:2 | Natural logarithm / Logaritmo natural | Function(Ln) | CEDG | Toolbox | x | 1 | 0 | Base e logarithm. Enter the argument. |
| 130:0 | Sine / Seno | Function(Sin) | CEDG | Keyboard / existing favorites | x | 0 | 0 | Uses the current angle mode. |
| 131:0 | Cosine / Coseno | Function(Cos) | CEDG | Keyboard / existing favorites | x | 0 | 1 | Uses the current angle mode. |
| 132:0 | Tangent / Tangente | Function(Tan) | CEDG | Keyboard / existing favorites | x | 0 | 0 | Uses the current angle mode. |
| 133:0 | Inverse sine / Arcoseno | Function(ArcSin) | CED | Toolbox | x | 0 | 0 | Uses the current angle mode. |
| 134:0 | Inverse cosine / Arcocoseno | Function(ArcCos) | CED | Toolbox | x | 1 | 0 | Uses the current angle mode. |
| 135:0 | Inverse tangent / Arcotangente | Function(ArcTan) | CED | Toolbox | x | 0 | 0 | Uses the current angle mode. |
| 140:0 | Hyperbolic sine / Seno hiperbólico | Call(Sinh) | CED | Toolbox | x | 0 | 0 | Enter the argument. |
| 141:0 | Hyperbolic cosine / Coseno hiperbólico | Call(Cosh) | CED | Toolbox | x | 0 | 1 | Enter the argument. |
| 142:0 | Hyperbolic tangent / Tangente hiperbólica | Call(Tanh) | CED | Toolbox | x | 0 | 0 | Enter the argument. |
| 143:0 | Inverse hyperbolic sine / Arcoseno hiperbólico | Call(Asinh) | CED | Toolbox | x | 0 | 0 | Enter the argument. |
| 144:0 | Inverse hyperbolic cosine / Arcocoseno hiperbólico | Call(Acosh) | CED | Toolbox | x | 1 | 0 | Enter the argument. |
| 145:0 | Inverse hyperbolic tangent / Arcotangente hiperbólica | Call(Atanh) | CED | Toolbox | x | 0 | 0 | Enter the argument. |
| 150:0 | Real part / Parte real | Call(Real) | CED | Toolbox | z | 2+3i | 2 | Enter the argument. |
| 151:0 | Imaginary part / Parte imaginaria | Call(Imag) | CED | Toolbox | z | 2+3i | 3 | Enter the argument. |
| 152:0 | Conjugate / Conjugado | Call(Conj) | CED | Toolbox | z | 2+3i | 2-3*i | Enter the argument. |
| 153:0 | Argument / Argumento | Call(Arg) | CED | Toolbox | z | 1 | 0 | Enter the argument. |
| 160:0 | Floor / Redondeo inferior | Call(Floor) | CED | Toolbox | x | 2.8 | 2 | Enter the argument. |
| 161:0 | Ceiling / Redondeo superior | Call(Ceil) | CED | Toolbox | x | 2.2 | 3 | Enter the argument. |
| 162:0 | Round to integer / Redondeo al entero | Call(Round) | CED | Toolbox | x | 2.3 | 2 | Enter the argument. |
| 163:0 | Greatest common divisor / Máximo común divisor | Call(Gcd) | CED | Toolbox | ab | 12, 8 | 4 | First integer, then second integer. |
| 164:0 | Least common multiple / Mínimo común múltiplo | Call(Lcm) | CED | Toolbox | ab | 6, 4 | 12 | First integer, then second integer. |
| 165:0 | Integer remainder / Resto entero | Call(Remainder) | CED | Toolbox | ab | 17, 5 | 2 | Dividend, then nonzero divisor. Signed integer remainder. |
| 166:0 | Combinations / Combinaciones | Call(Binomial) | CED | Toolbox | nk | 5, 2 | 10 | Total n, then chosen k; integers 0 <= k <= n. |
| 171:0 | Derivative / Derivada | Call(Diff) | CED | Toolbox | xx | x^2, x | 2*x | Expression, then variable x, y or z. |
| 167:0 | Factorial / Factorial | Function(Factorial) | CED | Toolbox | x | 5 | 120 | Enter a nonnegative integer inside the operand slot. |
| 170:0 | Definite integral / Integral definida | Integral(0) | CED | Toolbox | abxx | 0, 1, x, x | 1/2 | Lower bound, upper bound, integrand, variable. |
| 180:0 | Pi constant / Constante pi | Constant(Pi) | CEDG | Toolbox |  |  | pi | Insert the mathematical constant. |
| 181:0 | Euler's number / Número de Euler | Constant(E) | CEDG | Toolbox |  |  | exp(1) | Insert the mathematical constant. |
| 182:0 | Imaginary unit / Unidad imaginaria | Constant(Imag) | CED | Toolbox |  |  | i | Insert the mathematical constant. |
| 183:0 | Variable x / Variable x | Variable(120) | CEDG | Keyboard / existing favorites |  |  | x | Free mathematical variable; stored values use VAR. |
| 184:0 | Variable y / Variable y | Variable(121) | CEDG | Keyboard / existing favorites |  |  | y | Free mathematical variable; stored values use VAR. |
| 185:0 | Variable z / Variable z | Variable(122) | CED | Keyboard / existing favorites |  |  | z | Free mathematical variable; stored values use VAR. |
| 200:0 | a (lowercase) / a (minúscula) | Variable(97) | CED | Toolbox |  |  | a | Letter a, a free variable. Constants e and i are in Special Characters. |
| 200:1 | A (uppercase) / A (mayúscula) | Variable(65) | CED | Toolbox |  |  | 0 | Stored variable A. Its value follows STO. |
| 201:0 | b (lowercase) / b (minúscula) | Variable(98) | CED | Toolbox |  |  | b | Letter b, a free variable. Constants e and i are in Special Characters. |
| 201:1 | B (uppercase) / B (mayúscula) | Variable(66) | CED | Toolbox |  |  | 0 | Stored variable B. Its value follows STO. |
| 202:0 | c (lowercase) / c (minúscula) | Variable(99) | CED | Toolbox |  |  | c | Letter c, a free variable. Constants e and i are in Special Characters. |
| 202:1 | C (uppercase) / C (mayúscula) | Variable(67) | CED | Toolbox |  |  | 0 | Stored variable C. Its value follows STO. |
| 203:0 | d (lowercase) / d (minúscula) | Variable(100) | CED | Toolbox |  |  | d | Letter d, a free variable. Constants e and i are in Special Characters. |
| 203:1 | D (uppercase) / D (mayúscula) | Variable(68) | CED | Toolbox |  |  | 0 | Stored variable D. Its value follows STO. |
| 204:0 | e (lowercase) / e (minúscula) | Variable(101) | CED | Toolbox |  |  | 𝑒 | Letter e, a free variable. Constants e and i are in Special Characters. |
| 204:1 | E (uppercase) / E (mayúscula) | Variable(69) | CED | Toolbox |  |  | 0 | Stored variable E. Its value follows STO. |
| 205:0 | f (lowercase) / f (minúscula) | Variable(102) | CED | Toolbox |  |  | f | Letter f, a free variable. Constants e and i are in Special Characters. |
| 205:1 | F (uppercase) / F (mayúscula) | Variable(70) | CED | Toolbox |  |  | 0 | Stored variable F. Its value follows STO. |
| 206:0 | g (lowercase) / g (minúscula) | Variable(103) | CED | Toolbox |  |  | g | Letter g, a free variable. Constants e and i are in Special Characters. |
| 206:1 | G (uppercase) / G (mayúscula) | Variable(71) | CED | Toolbox |  |  | G | Letter G, a free variable. Constants e and i are in Special Characters. |
| 207:0 | h (lowercase) / h (minúscula) | Variable(104) | CED | Toolbox |  |  | h | Letter h, a free variable. Constants e and i are in Special Characters. |
| 207:1 | H (uppercase) / H (mayúscula) | Variable(72) | CED | Toolbox |  |  | H | Letter H, a free variable. Constants e and i are in Special Characters. |
| 208:0 | i (lowercase) / i (minúscula) | Variable(105) | CED | Toolbox |  |  | 𝑖 | Letter i, a free variable. Constants e and i are in Special Characters. |
| 208:1 | I (uppercase) / I (mayúscula) | Variable(73) | CED | Toolbox |  |  | 𝐼 | Letter I, a free variable. Constants e and i are in Special Characters. |
| 209:0 | j (lowercase) / j (minúscula) | Variable(106) | CED | Toolbox |  |  | j | Letter j, a free variable. Constants e and i are in Special Characters. |
| 209:1 | J (uppercase) / J (mayúscula) | Variable(74) | CED | Toolbox |  |  | J | Letter J, a free variable. Constants e and i are in Special Characters. |
| 210:0 | k (lowercase) / k (minúscula) | Variable(107) | CED | Toolbox |  |  | k | Letter k, a free variable. Constants e and i are in Special Characters. |
| 210:1 | K (uppercase) / K (mayúscula) | Variable(75) | CED | Toolbox |  |  | K | Letter K, a free variable. Constants e and i are in Special Characters. |
| 211:0 | l (lowercase) / l (minúscula) | Variable(108) | CED | Toolbox |  |  | l | Letter l, a free variable. Constants e and i are in Special Characters. |
| 211:1 | L (uppercase) / L (mayúscula) | Variable(76) | CED | Toolbox |  |  | L | Letter L, a free variable. Constants e and i are in Special Characters. |
| 212:0 | m (lowercase) / m (minúscula) | Variable(109) | CED | Toolbox |  |  | m | Letter m, a free variable. Constants e and i are in Special Characters. |
| 212:1 | M (uppercase) / M (mayúscula) | Variable(77) | CED | Toolbox |  |  | M | Letter M, a free variable. Constants e and i are in Special Characters. |
| 213:0 | n (lowercase) / n (minúscula) | Variable(110) | CED | Toolbox |  |  | n | Letter n, a free variable. Constants e and i are in Special Characters. |
| 213:1 | N (uppercase) / N (mayúscula) | Variable(78) | CED | Toolbox |  |  | N | Letter N, a free variable. Constants e and i are in Special Characters. |
| 214:0 | o (lowercase) / o (minúscula) | Variable(111) | CED | Toolbox |  |  | o | Letter o, a free variable. Constants e and i are in Special Characters. |
| 214:1 | O (uppercase) / O (mayúscula) | Variable(79) | CED | Toolbox |  |  | O | Letter O, a free variable. Constants e and i are in Special Characters. |
| 215:0 | p (lowercase) / p (minúscula) | Variable(112) | CED | Toolbox |  |  | p | Letter p, a free variable. Constants e and i are in Special Characters. |
| 215:1 | P (uppercase) / P (mayúscula) | Variable(80) | CED | Toolbox |  |  | P | Letter P, a free variable. Constants e and i are in Special Characters. |
| 216:0 | q (lowercase) / q (minúscula) | Variable(113) | CED | Toolbox |  |  | q | Letter q, a free variable. Constants e and i are in Special Characters. |
| 216:1 | Q (uppercase) / Q (mayúscula) | Variable(81) | CED | Toolbox |  |  | Q | Letter Q, a free variable. Constants e and i are in Special Characters. |
| 217:0 | r (lowercase) / r (minúscula) | Variable(114) | CED | Toolbox |  |  | r | Letter r, a free variable. Constants e and i are in Special Characters. |
| 217:1 | R (uppercase) / R (mayúscula) | Variable(82) | CED | Toolbox |  |  | R | Letter R, a free variable. Constants e and i are in Special Characters. |
| 218:0 | s (lowercase) / s (minúscula) | Variable(115) | CED | Toolbox |  |  | s | Letter s, a free variable. Constants e and i are in Special Characters. |
| 218:1 | S (uppercase) / S (mayúscula) | Variable(83) | CED | Toolbox |  |  | S | Letter S, a free variable. Constants e and i are in Special Characters. |
| 219:0 | t (lowercase) / t (minúscula) | Variable(116) | CED | Toolbox |  |  | t | Letter t, a free variable. Constants e and i are in Special Characters. |
| 219:1 | T (uppercase) / T (mayúscula) | Variable(84) | CED | Toolbox |  |  | T | Letter T, a free variable. Constants e and i are in Special Characters. |
| 220:0 | u (lowercase) / u (minúscula) | Variable(117) | CED | Toolbox |  |  | u | Letter u, a free variable. Constants e and i are in Special Characters. |
| 220:1 | U (uppercase) / U (mayúscula) | Variable(85) | CED | Toolbox |  |  | U | Letter U, a free variable. Constants e and i are in Special Characters. |
| 221:0 | v (lowercase) / v (minúscula) | Variable(118) | CED | Toolbox |  |  | v | Letter v, a free variable. Constants e and i are in Special Characters. |
| 221:1 | V (uppercase) / V (mayúscula) | Variable(86) | CED | Toolbox |  |  | V | Letter V, a free variable. Constants e and i are in Special Characters. |
| 222:0 | w (lowercase) / w (minúscula) | Variable(119) | CED | Toolbox |  |  | w | Letter w, a free variable. Constants e and i are in Special Characters. |
| 222:1 | W (uppercase) / W (mayúscula) | Variable(87) | CED | Toolbox |  |  | W | Letter W, a free variable. Constants e and i are in Special Characters. |
| 223:0 | x (lowercase) / x (minúscula) | Variable(120) | CEDG | Toolbox |  |  | x | Letter x, a free variable. Constants e and i are in Special Characters. |
| 223:1 | X (uppercase) / X (mayúscula) | Variable(88) | CED | Toolbox |  |  | X | Letter X, a free variable. Constants e and i are in Special Characters. |
| 224:0 | y (lowercase) / y (minúscula) | Variable(121) | CEDG | Toolbox |  |  | y | Letter y, a free variable. Constants e and i are in Special Characters. |
| 224:1 | Y (uppercase) / Y (mayúscula) | Variable(89) | CED | Toolbox |  |  | Y | Letter Y, a free variable. Constants e and i are in Special Characters. |
| 225:0 | z (lowercase) / z (minúscula) | Variable(122) | CED | Toolbox |  |  | z | Letter z, a free variable. Constants e and i are in Special Characters. |
| 225:1 | Z (uppercase) / Z (mayúscula) | Variable(90) | CED | Toolbox |  |  | Z | Letter Z, a free variable. Constants e and i are in Special Characters. |
| 300:0 | Alpha / Alfa | Symbol(945) | CED | Toolbox |  |  | α | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 300:1 | Capital Alpha / Alfa mayúscula | Symbol(913) | CED | Toolbox |  |  | Α | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 301:0 | Beta / Beta | Symbol(946) | CED | Toolbox |  |  | β | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 301:1 | Capital Beta / Beta mayúscula | Symbol(914) | CED | Toolbox |  |  | Β | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 302:0 | Gamma / Gamma | Symbol(947) | CED | Toolbox |  |  | γ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 302:1 | Capital Gamma / Gamma mayúscula | Symbol(915) | CED | Toolbox |  |  | Γ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 303:0 | Delta / Delta | Symbol(948) | CED | Toolbox |  |  | δ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 303:1 | Capital Delta / Delta mayúscula | Symbol(916) | CED | Toolbox |  |  | Δ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 304:0 | Epsilon / Épsilon | Symbol(949) | CED | Toolbox |  |  | ε | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 304:1 | Capital Epsilon / Épsilon mayúscula | Symbol(917) | CED | Toolbox |  |  | Ε | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 305:0 | Zeta / Zeta | Symbol(950) | CED | Toolbox |  |  | ζ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 305:1 | Capital Zeta / Zeta mayúscula | Symbol(918) | CED | Toolbox |  |  | Ζ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 306:0 | Eta / Eta | Symbol(951) | CED | Toolbox |  |  | η | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 306:1 | Capital Eta / Eta mayúscula | Symbol(919) | CED | Toolbox |  |  | Η | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 307:0 | Theta / Theta | Symbol(952) | CED | Toolbox |  |  | θ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 307:1 | Capital Theta / Theta mayúscula | Symbol(920) | CED | Toolbox |  |  | Θ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 308:0 | Iota / Iota | Symbol(953) | CED | Toolbox |  |  | ι | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 308:1 | Capital Iota / Iota mayúscula | Symbol(921) | CED | Toolbox |  |  | Ι | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 309:0 | Kappa / Kappa | Symbol(954) | CED | Toolbox |  |  | κ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 309:1 | Capital Kappa / Kappa mayúscula | Symbol(922) | CED | Toolbox |  |  | Κ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 310:0 | Lambda / Lambda | Symbol(955) | CED | Toolbox |  |  | λ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 310:1 | Capital Lambda / Lambda mayúscula | Symbol(923) | CED | Toolbox |  |  | Λ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 311:0 | Mu / Mu | Symbol(956) | CED | Toolbox |  |  | μ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 311:1 | Capital Mu / Mu mayúscula | Symbol(924) | CED | Toolbox |  |  | Μ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 312:0 | Nu / Nu | Symbol(957) | CED | Toolbox |  |  | ν | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 312:1 | Capital Nu / Nu mayúscula | Symbol(925) | CED | Toolbox |  |  | Ν | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 313:0 | Xi / Xi | Symbol(958) | CED | Toolbox |  |  | ξ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 313:1 | Capital Xi / Xi mayúscula | Symbol(926) | CED | Toolbox |  |  | Ξ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 314:0 | Omicron / Ómicron | Symbol(959) | CED | Toolbox |  |  | ο | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 314:1 | Capital Omicron / Ómicron mayúscula | Symbol(927) | CED | Toolbox |  |  | Ο | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 315:0 | Pi (variable) / Pi (variable) | Symbol(960) | CED | Toolbox |  |  | 𝜋 | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 315:1 | Capital Pi / Pi mayúscula | Symbol(928) | CED | Toolbox |  |  | Π | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 316:0 | Rho / Rho | Symbol(961) | CED | Toolbox |  |  | ρ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 316:1 | Capital Rho / Rho mayúscula | Symbol(929) | CED | Toolbox |  |  | Ρ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 317:0 | Sigma / Sigma | Symbol(963) | CED | Toolbox |  |  | σ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 317:1 | Capital Sigma / Sigma mayúscula | Symbol(931) | CED | Toolbox |  |  | Σ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 318:0 | Tau / Tau | Symbol(964) | CED | Toolbox |  |  | τ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 318:1 | Capital Tau / Tau mayúscula | Symbol(932) | CED | Toolbox |  |  | Τ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 319:0 | Upsilon / Ípsilon | Symbol(965) | CED | Toolbox |  |  | υ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 319:1 | Capital Upsilon / Ípsilon mayúscula | Symbol(933) | CED | Toolbox |  |  | Υ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 320:0 | Phi / Phi | Symbol(966) | CED | Toolbox |  |  | φ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 320:1 | Capital Phi / Phi mayúscula | Symbol(934) | CED | Toolbox |  |  | Φ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 321:0 | Chi / Chi | Symbol(967) | CED | Toolbox |  |  | χ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 321:1 | Capital Chi / Chi mayúscula | Symbol(935) | CED | Toolbox |  |  | Χ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 322:0 | Psi / Psi | Symbol(968) | CED | Toolbox |  |  | ψ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 322:1 | Capital Psi / Psi mayúscula | Symbol(936) | CED | Toolbox |  |  | Ψ | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 323:0 | Omega / Omega | Symbol(969) | CED | Toolbox |  |  | ω | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 323:1 | Capital Omega / Omega mayúscula | Symbol(937) | CED | Toolbox |  |  | Ω | Greek letter, a free variable. RIGHT opens the case variants. Constant pi is in Special Characters. |
| 400:0 | Theta variant / Variante de theta | Symbol(977) | CED | Toolbox |  |  | ϑ | Alternate Greek letter; a distinct free variable. |
| 401:0 | Phi variant / Variante de phi | Symbol(981) | CED | Toolbox |  |  | ϕ | Alternate Greek letter; a distinct free variable. |
| 402:0 | Epsilon variant / Variante de épsilon | Symbol(1013) | CED | Toolbox |  |  | ϵ | Alternate Greek letter; a distinct free variable. |
| 403:0 | Pi variant / Variante de pi | Symbol(982) | CED | Toolbox |  |  | ϖ | Alternate Greek letter; a distinct free variable. |
| 404:0 | Final sigma / Sigma final | Symbol(962) | CED | Toolbox |  |  | ς | Alternate Greek letter; a distinct free variable. |
| 410:0 | Infinity / Infinito | Infinity(BarePositiveInfinity) | C | Toolbox |  |  | +infinity | Positive infinity, without a written sign. RIGHT opens sign variants. |
| 410:1 | Positive infinity / Infinito positivo | Infinity(PositiveInfinity) | C | Toolbox |  |  | +infinity | Explicit positive infinity. Same value as infinity without a sign. |
| 410:2 | Negative infinity / Infinito negativo | Infinity(NegativeInfinity) | C | Toolbox |  |  | -infinity | Negative infinity. Its sign belongs to the inserted atom. |
| 410:3 | Both infinities / Ambos infinitos | Infinity(BothInfinities) | C | Toolbox |  |  | [-infinity, +infinity] | Both values, negative and positive infinity. The compact sign is not a single real number. |
| 750:0 | Value ± standard uncertainty / Valor ± incertidumbre estándar | Measurement(0) | C | Toolbox | vu | 12, 1 | 12 | Standard uncertainty u, k=1; not a guaranteed interval. Separate new measurements are independent. Reuse Ans or memory to keep the same source. Apply a unit to both slots together. |
