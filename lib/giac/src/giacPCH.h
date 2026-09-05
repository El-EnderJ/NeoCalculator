/*
 * Modified for NumOS / NeoCalculator in 2026 for ESP32-S3 and host integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef NSPIRE_NEWLIB
#include "os.h"
#endif
#include <vector>
#ifdef USTL
#include <umap.h>
#include <umultimap.h>
#include <ulist.h>
#else
#include <map>
#include <list>
#endif
#include <string>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "first.h"
#include "index.h"
/*
 *  Copyright (C) 2000,2014 B. Parisse, Institut Fourier, 38402 St Martin d'Heres
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

using namespace std;
#include <cmath>
#include <stdexcept>
#include <cstdlib>
#include <map>
#include <errno.h>
#include "gen.h"
#include "vecteur.h"
#include "identificateur.h"
#include "symbolic.h"
#include "poly.h"
#include "usual.h"
#include "series.h"
#include "sym2poly.h"
#include "moyal.h"
#include "subst.h"
#include "gausspol.h"
#include "identificateur.h"
#include "ifactor.h"
#include "prog.h"
#include "rpn.h"
#include "plot.h"
#include "pari.h"
#include "tex.h"
#include "unary.h"
#include "intg.h"
#include "ti89.h"
#include "solve.h"
#include "alg_ext.h"
#include "lin.h"
#include "derive.h"
#include "series.h"
#include "misc.h"
#include "derive.h"
#include "desolve.h"
#include "ezgcd.h"
#include "gauss.h"
#include "help.h"
#include "maple.h"
#include "mathml.h"
#include "ifactor.h"
#include "index.h"
#include "intg.h"
#include "intgab.h"

//#include "windows.h"



#include "global.h"
#include "permu.h"
#include "plot3d.h"
#include "risch.h"
#include "solve.h"

#include "symbolic.h"
//#include "threaded.h"
#include "static_extern.h"
