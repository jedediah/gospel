/*
    Copyright © 2008 Sam Chapin

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

int main(int argc, char **argv) {
  setupInterpreter();
  loadFile("canon.gs", oLobby, oDynamicEnvironment);
  for (int i = argc; --i;) loadFile(argv[i], oLobby, oDynamicEnvironment);
  REPL();
}
