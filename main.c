/*
    Copyright © 2008, 2009 Sam Chapin

    This file is part of Gospel.

    Gospel is free software: you can redistribute it and/or modify
    it under the terms of version 3 of the GNU General Public License
    as published by the Free Software Foundation.

    Gospel is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Gospel.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "core.h"
#include "objects.h"
#include "death.h"
#include <unistd.h>

int main(int argc, char **argv) {
  setupInterpreter();
  if (argc > 2) die("Too many arguments.");
  vector a = newActor(oInternals, oObject, oDynamicEnvironment);
  waitFor(enqueueMessage(a, sLoadFile_, newVector(1, string("canon.gs"))));
  waitFor(enqueueMessage(a, sLoadFile_, newVector(1, string(argc == 2 ? argv[1] : "repl.gs"))));
  return 0;
}

