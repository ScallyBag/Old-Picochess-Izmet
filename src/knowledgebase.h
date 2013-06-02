/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2013 Marco Costalba, Joona Kiiski, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#if !defined(KNOWLEDGEBASE_H_INCLUDED)
#define KNOWLEDGEBASE_H_INCLUDED

#include <map>
#include <string>

#include "position.h"
#include "types.h"

typedef bool (*KnowledgeProbeFunction)(const Position& pos, Value &v);

class KnowledgeBases
{
  std::map<Key,KnowledgeProbeFunction> m;
  void add(const std::string& code,KnowledgeProbeFunction func);

public:
  KnowledgeBases();
 //~KnowledgeBases();

  KnowledgeProbeFunction probe(Key key)
  { return m.count(key) ? m[key] : NULL; }
};

#endif // !defined(KNOWLEDGEBASE_H_INCLUDED)
