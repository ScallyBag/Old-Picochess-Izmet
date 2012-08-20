/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2012 Marco Costalba, Joona Kiiski, Tord Romstad

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

#include <iostream>
#include <sstream>
#include <string>

#include "evaluate.h"
#include "notation.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"
#include "ucioption.h"
#include "dgtnix.h"
#include "movegen.h"

using namespace std;

extern void benchmark(const Position& pos, istream& is);

namespace {

  // FEN string of the initial position, normal chess
  const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

  // Keep track of position keys along the setup moves (from start position to the
  // position just before to start searching). This is needed by draw detection
  // where, due to 50 moves rule, we need to check at most 100 plies back.
  StateInfo StateRingBuf[102], *SetupState = StateRingBuf;

  void set_option(istringstream& up);
  void set_position(Position& pos, istringstream& up);
  void go(Position& pos, istringstream& up);
}


/// Wait for a command from the user, parse this text string as an UCI command,
/// and call the appropriate functions. Also intercepts EOF from stdin to ensure
/// that we exit gracefully if the GUI dies unexpectedly. In addition to the UCI
/// commands, the function also supports a few debug commands.

void UCI::loop(const string& args) {

  Position pos(StartFEN, false, Threads.main_thread()); // The root position
  string cmd, token;

  while (token != "quit")
  {
      if (!args.empty())
          cmd = args;

      else if (!getline(cin, cmd)) // Block here waiting for input
          cmd = "quit";

      istringstream is(cmd);

      is >> skipws >> token;

      if (token == "quit" || token == "stop")
      {
          Search::Signals.stop = true;
          Threads.wait_for_search_finished(); // Cannot quit while threads are running
      }

      else if (token == "ponderhit")
      {
          // The opponent has played the expected move. GUI sends "ponderhit" if
          // we were told to ponder on the same move the opponent has played. We
          // should continue searching but switching from pondering to normal search.
          Search::Limits.ponder = false;

          if (Search::Signals.stopOnPonderhit)
          {
              Search::Signals.stop = true;
              Threads.main_thread()->wake_up(); // Could be sleeping
          }
      }

      else if (token == "go")
          go(pos, is);

      else if (token == "ucinewgame")
          TT.clear();

      else if (token == "isready")
          cout << "readyok" << endl;

      else if (token == "position")
          set_position(pos, is);

      else if (token == "setoption")
          set_option(is);

      else if (token == "d")
          pos.print();

      else if (token == "flip")
          pos.flip();

      else if (token == "eval")
          cout << Eval::trace(pos) << endl;

      else if (token == "bench")
          benchmark(pos, is);

      else if (token == "key")
          cout << "key: " << hex     << pos.key()
               << "\nmaterial key: " << pos.material_key()
               << "\npawn key: "     << pos.pawn_key() << endl;

      else if (token == "uci")
          cout << "id name "     << engine_info(true)
               << "\n"           << Options
               << "\nuciok"      << endl;

      else if (token == "perft" && (is >> token)) // Read depth
      {
          stringstream ss;

          ss << Options["Hash"]    << " "
             << Options["Threads"] << " " << token << " current perft";

          benchmark(pos, ss);
      }

      else if (token == "dgt" && (is >> token)) // Read USB port
      {
    	  DGT::loop(token);
      }

      else
          cout << "Unknown command: " << cmd << endl;

      if (!args.empty()) // Command line arguments have one-shot behaviour
      {
          Threads.wait_for_search_finished();
          break;
      }
  }
}


namespace {

  // set_position() is called when engine receives the "position" UCI
  // command. The function sets up the position described in the given
  // fen string ("fen") or the starting position ("startpos") and then
  // makes the moves given in the following move list ("moves").

  void set_position(Position& pos, istringstream& is) {

    Move m;
    string token, fen;

    is >> token;

    if (token == "startpos")
    {
        fen = StartFEN;
        is >> token; // Consume "moves" token if any
    }
    else if (token == "fen")
        while (is >> token && token != "moves")
            fen += token + " ";
    else
        return;

    pos.from_fen(fen, Options["UCI_Chess960"], Threads.main_thread());

    // Parse move list (if any)
    while (is >> token && (m = move_from_uci(pos, token)) != MOVE_NONE)
    {
        pos.do_move(m, *SetupState);

        // Increment pointer to StateRingBuf circular buffer
        if (++SetupState - StateRingBuf >= 102)
            SetupState = StateRingBuf;
    }
  }


  // set_option() is called when engine receives the "setoption" UCI command. The
  // function updates the UCI option ("name") to the given value ("value").

  void set_option(istringstream& is) {

    string token, name, value;

    is >> token; // Consume "name" token

    // Read option name (can contain spaces)
    while (is >> token && token != "value")
        name += string(" ", !name.empty()) + token;

    // Read option value (can contain spaces)
    while (is >> token)
        value += string(" ", !value.empty()) + token;

    if (Options.count(name))
        Options[name] = value;
    else
        cout << "No such option: " << name << endl;
  }


  // go() is called when engine receives the "go" UCI command. The function sets
  // the thinking time and other parameters from the input string, and then starts
  // the search.

  void go(Position& pos, istringstream& is) {

    Search::LimitsType limits;
    vector<Move> searchMoves;
    string token;

    while (is >> token)
    {
        if (token == "wtime")
            is >> limits.time[WHITE];
        else if (token == "btime")
            is >> limits.time[BLACK];
        else if (token == "winc")
            is >> limits.inc[WHITE];
        else if (token == "binc")
            is >> limits.inc[BLACK];
        else if (token == "movestogo")
            is >> limits.movestogo;
        else if (token == "depth")
            is >> limits.depth;
        else if (token == "nodes")
            is >> limits.nodes;
        else if (token == "movetime")
            is >> limits.movetime;
        else if (token == "infinite")
            limits.infinite = true;
        else if (token == "ponder")
            limits.ponder = true;
        else if (token == "searchmoves")
            while (is >> token)
                searchMoves.push_back(move_from_uci(pos, token));
    }

    Threads.start_searching(pos, limits, searchMoves);
  }
}

//--------------------------------------------------------------------
// Give the current board setup as FEN string
// char  :  tomove = 'w' or 'b' : the side to move
//
// char     : tomove         : the side to move (white is default)
//--------------------------------------------------------------------
string getDgtFEN(char tomove = 'w')
{
	const char *board = dgtnixGetBoard();
	char  FEN[90];
	int   pos = 0;
	int   empty = 0;

	for (int sq = 0; sq < 64; sq++)
	{
		if (board[sq] != 32) {
			if (empty > 0) {
				FEN[pos] = empty+48;
				pos++;
				empty=0;
			}
			FEN[pos] = char(board[sq]);
			pos++;
		}
		else empty++;
		if ((sq+1) % 8 == 0) {
			if (empty > 0) {
				FEN[pos] = empty+48;
				pos++;
				empty=0;
			}
			if (sq < 63) {
				FEN[pos] = '/';
				pos++;
			}
			empty = 0;
		}
	}

	// FEN data fields
	FEN[pos++] = ' '; FEN[pos++] = tomove; // side to move
	FEN[pos++] = ' ';
	// possible castelings
	FEN[pos++] = 'K'; FEN[pos++] = 'Q';
	FEN[pos++] = 'k'; FEN[pos++] = 'q';
	FEN[pos++] = ' '; FEN[pos++] = '-';
	FEN[pos++] = ' '; FEN[pos++] = '0';
	FEN[pos++] = ' '; FEN[pos++] = '1';

	// Mark the end of the string
	FEN[pos] = char(0);

        return string(FEN);
	//printf("FEN %s\n", FEN);
}

void DGT::loop(const string& args) {

	/*if(argc!=2)
	      {
	        cout << "usage: ./stockfish dgt port" << endl;
	        cout << "Port is the port to which the board is connected." << endl;
	        cout << "For usb connection, try : /dev/usb/tts/0, /dev/usb/tts/1, /dev/usb/tts/2 ..." << endl;
	        cout << "For serial, try : /dev/ttyS0, /dev/ttyS1, /dev/ttyS2 ..." << endl;
	        //cout << "For the virtual board /tmp/dgtnixBoard is the default but you can change it." << endl;
	        exit(1);
	      }*/

	    Position pos(StartFEN, false, Threads.main_thread()); // The root position

	    //compute the next legal fens
	    StateInfo state;

	    std::map<string,Move> legalFENs;
	    for (MoveList<LEGAL> ml(pos); !ml.end(); ++ml)
	    {
	        //StateInfo state;
	        pos.do_move(ml.move(),state);
	        //cout<<pos.to_fen()<<endl;
	        string str=pos.to_fen();
	        str=str.substr(0, str.find(' '));
	        legalFENs.insert(pair<string,Move>(str,ml.move()));
	        cout << str << endl;
	        pos.undo_move(ml.move());
	    }


	    int BoardDescriptor;
	    //int optionError=0;
	    char port[256];
	    /* all debug informations are printed */
	    dgtnixSetOption(DGTNIX_DEBUG, DGTNIX_DEBUG_WITH_TIME);
	    /* Initialize the driver with port argv[2] */
	    strncpy(port, args.c_str(), 256);
	    BoardDescriptor=dgtnixInit(port);
	    int err = dgtnix_errno;
	    if(BoardDescriptor < 0 )
	      {
	        cout << "Unable to connect to DGT board on port "  << port <<" : " ;
	        switch (BoardDescriptor)
	          {
	          case -1:
	            cout << strerror(err) << endl;
	            break;
	          case -2:
	            cout << "Not responding to the DGT_SEND_BRD message" << endl;
	            break;
	          default:
	            cout << "Unrecognized response to the DGT_SEND_BRD message :" <<  BoardDescriptor <<endl;
	          }
	        exit(-1);
	      }
	    cout << "The board was found" << BoardDescriptor << endl;


	    dgtnixPrintMessageOnClock(" hello", 1);
	    dgtnixUpdate();

	    bool searching=false;

	    Search::LimitsType limits, resetLimits;
	    limits.movetime=5000;
	    string currentFEN=getDgtFEN();
	    //pos.from_fen(currentFEN,false, Threads.main_thread());

	    //"8/8/8/8/8/8/8/PP6 w KQkq - 0 1"
	    while(true)
	    {
	        //cout << "DGT Loop" << endl;
	        //dgtnixPrintMessageOnClock("abcdeg", 1);

	        string s=getDgtFEN();
	        if(currentFEN!=s)
	        {
	            currentFEN=s;
	            //pos.from_fen(currentFEN,false, Threads.main_thread());

	            cout << currentFEN << endl;

	            //if(currentFEN=="8/8/8/8/8/8/8/PP6 w KQkq - 0 1")  { dgtnixPrintMessageOnClock("config", 1); }

	            if(currentFEN.find("/PP6 w")!=string::npos) //config mode
	            {
	                if(currentFEN=="8/8/8/8/8/8/8/PP6 w KQkq - 0 1")  { dgtnixPrintMessageOnClock("config", 1); }

	                //set skill level
	                if(currentFEN=="p7/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 00", 1); Options[string("Skill Level")] = string("0"); }
	                if(currentFEN=="1p6/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 01", 1); Options[string("Skill Level")] = string("1"); }
	                if(currentFEN=="2p5/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 02", 1); Options[string("Skill Level")] = string("2"); }
	                if(currentFEN=="3p4/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 03", 1); Options[string("Skill Level")] = string("3"); }
	                if(currentFEN=="4p3/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 04", 1); Options[string("Skill Level")] = string("4"); }
	                if(currentFEN=="5p2/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 05", 1); Options[string("Skill Level")] = string("5"); }
	                if(currentFEN=="6p1/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 06", 1); Options[string("Skill Level")] = string("6"); }
	                if(currentFEN=="7p/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 07", 1); Options[string("Skill Level")] = string("7"); }
	                if(currentFEN=="8/p7/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 08", 1); Options[string("Skill Level")] = string("8"); }
	                if(currentFEN=="8/1p6/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 09", 1); Options[string("Skill Level")] = string("9"); }
	                if(currentFEN=="8/2p5/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 10", 1); Options[string("Skill Level")] = string("10"); }
	                if(currentFEN=="8/3p4/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 11", 1); Options[string("Skill Level")] = string("11"); }
	                if(currentFEN=="8/4p3/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 12", 1); Options[string("Skill Level")] = string("12"); }
	                if(currentFEN=="8/5p2/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 13", 1); Options[string("Skill Level")] = string("13"); }
	                if(currentFEN=="8/6p1/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 14", 1); Options[string("Skill Level")] = string("14"); }
	                if(currentFEN=="8/7p/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 15", 1); Options[string("Skill Level")] = string("15"); }
	                if(currentFEN=="8/8/p7/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 16", 1); Options[string("Skill Level")] = string("16"); }
	                if(currentFEN=="8/8/1p6/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 17", 1); Options[string("Skill Level")] = string("17"); }
	                if(currentFEN=="8/8/2p5/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 18", 1); Options[string("Skill Level")] = string("18"); }
	                if(currentFEN=="8/8/3p4/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 19", 1); Options[string("Skill Level")] = string("19"); }
	                if(currentFEN=="8/8/4p3/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("lvl 20", 1); Options[string("Skill Level")] = string("20"); }

	                //set time control
	                if(currentFEN=="n7/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov  1", 1); limits=resetLimits; limits.movetime=1000; }
	                if(currentFEN=="1n6/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov  5", 1); limits=resetLimits; limits.movetime=5000; }
	                if(currentFEN=="2n5/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 10", 1); limits=resetLimits; limits.movetime=10000; }
	                if(currentFEN=="3n4/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 20", 1); limits=resetLimits; limits.movetime=20000; }
	                if(currentFEN=="4n3/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 40", 1); limits=resetLimits; limits.movetime=40000; }
	                if(currentFEN=="5n2/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 1m", 1); limits=resetLimits; limits.movetime=60000; }
	                if(currentFEN=="6n1/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 3m", 1); limits=resetLimits; limits.movetime=180000; }
	                if(currentFEN=="7n/8/8/8/8/8/8/PP6 w KQkq - 0 1") { dgtnixPrintMessageOnClock("mov 5m", 1); limits=resetLimits; limits.movetime=300000; }
	            }

	            //launch the search
	            string str=currentFEN.substr(0, currentFEN.find(' '));
	            if(legalFENs.count(str))
	            {

	                Position p(pos);
	                p.do_move(legalFENs.at(str),state);


	                Search::Signals.stop = true;
	                Threads.wait_for_search_finished(); // Cannot quit while threads are running

	                dgtnixPrintMessageOnClock("search", 0);

	                vector<Move> searchMoves;
	                Threads.start_searching(p, limits, searchMoves);
	                searching=true;
	            }



	            /*
	            Search::Signals.stop = true;
	            Threads.wait_for_search_finished(); // Cannot quit while threads are running

	            Search::LimitsType limits;
	            limits.movetime=3000;
	            vector<Move> searchMoves;

	            Threads.start_searching(pos, limits, searchMoves);*/

	        }

	        //check for pos update after search
	        if(Search::Signals.stop == true && searching)
	        {
	            searching=false;
	            cout << "stopped with move " <<  move_to_uci(Search::RootMoves[0].pv[0], false) << endl;

	            string str=currentFEN.substr(0, currentFEN.find(' ')); //do the player move
	            pos.do_move(legalFENs.at(str),state);
	            pos.do_move(Search::RootMoves[0].pv[0],state); //do the engine's move
	            legalFENs.clear();
	            for (MoveList<LEGAL> ml(pos); !ml.end(); ++ml)
	            {
	                //StateInfo state;
	                pos.do_move(ml.move(),state);
	                cout<<pos.to_fen()<<endl;
	                string str=pos.to_fen();
	                str=str.substr(0, str.find(' '));
	                legalFENs.insert(pair<string,Move>(str,ml.move()));
	                cout << str << endl;
	                pos.undo_move(ml.move());
	            }
	        }

	        //sleep
	        struct timespec tim, tim2;
	        tim.tv_sec = 0;
	        tim.tv_nsec = 30000000L;
	        nanosleep(&tim , &tim2);
	    }

	    dgtnixClose();
}

