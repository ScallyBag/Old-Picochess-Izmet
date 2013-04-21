/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2012 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2012 Jean-Francois Romang
  Copyright (C) 2012-2013 Shivkumar Shivaji

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
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stack>
#include <iomanip>
#include <algorithm>
#include <unistd.h>
#include <deque>


#include "evaluate.h"
#include "notation.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"
#include "ucioption.h"
#include "dgtnix.h"
#include "movegen.h"
#include "book.h"
#include "platform.h"

using namespace std;

namespace DGT
{
  //Global declarations
  Search::LimitsType limits, resetLimits;
  Color computerPlays;
  vector<Move> game;
  
  ofstream pgnFile;
  int plyCount = 0;
  bool rewritePGN = false;
  bool boardReversed = false;
  const char* StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"; // FEN string of the initial position, normal chess
  const char* bookPath = "/opt/picochess/books/";
  bool setupPosition = false; // If a custom position is setup
  bool customPosition = false;
  deque<string> fenQueue;
  u_int MAX_FEN_QUEUE_SIZE = 3;
  char* customStartFEN;

  volatile enum PlayMode
  {
      // Kibitz mode is game mode + commentary
    GAME, ANALYSIS, BOOK, TRAINING, KIBITZ
  } playMode;

  volatile enum ClockMode
  {
    FIXEDTIME, INFINITE, TOURNAMENT, BLITZ, BLITZFISCHER, SPECIAL
  } clockMode;
  int fixedTime, blitzTime, fischerInc, wTime, bTime;
  bool computerMoveFENReached = false;
  volatile bool searching = false;
  string ponderHitFEN = "";

  void
  addToFenQueue (string fen)
  {
    fenQueue.push_back (fen);
    if (fenQueue.size () > MAX_FEN_QUEUE_SIZE)
      {
        fenQueue.pop_front ();
      }
  }

  void
  resetClock ()
  {
    limits = resetLimits;
    if (clockMode == BLITZ)
      {
        wTime = bTime = blitzTime;
        fischerInc = 0;
      }
    if (clockMode == BLITZFISCHER)
      {
        wTime = bTime = blitzTime;
      }
    if (clockMode == FIXEDTIME)
      {
        limits.movetime = fixedTime;
      }
    if (clockMode == INFINITE)
      {
        limits.infinite = true;
      }
  }
  void printTimeOnClock (int wClockTime, int bClockTime, bool wDots, bool bDots);

  char*
  getStartFEN ()
  {
    char * fen;

    if (customPosition)
      {
        fen = customStartFEN;
        //        cout<<"custom Start FEN: "<< fen << endl;
      }
    else
      {
        fen = const_cast<char *> (StartFEN);
        //        cout << "Regular Start FEN" << fen << endl;
      }
    return fen;

  }

  /// Give the current board setup as FEN string
  /// char  :  tomove = 'w' or 'b' : the side to move (white is default)

  string
  getDgtFEN (char tomove = 'w')
  {
    const char *board = dgtnixGetBoard ();
    char FEN[90];
    int pos = 0;
    int empty = 0;

    for (int sq = 0; sq < 64; sq++)
      {
        if (board[sq] != 32)
          {
            if (empty > 0)
              {
                FEN[pos] = empty + 48;
                pos++;
                empty = 0;
              }
            FEN[pos] = char(board[sq]);
            pos++;
          }
        else empty++;
        if ((sq + 1) % 8 == 0)
          {
            if (empty > 0)
              {
                FEN[pos] = empty + 48;
                pos++;
                empty = 0;
              }
            if (sq < 63)
              {
                FEN[pos] = '/';
                pos++;
              }
            empty = 0;
          }
      }

    // FEN data fields
    FEN[pos++] = ' ';
    FEN[pos++] = tomove; // side to move
    FEN[pos++] = ' ';
    // possible castlings
    FEN[pos++] = 'K';
    FEN[pos++] = 'Q';
    FEN[pos++] = 'k';
    FEN[pos++] = 'q';
    FEN[pos++] = ' ';
    FEN[pos++] = '-';
    FEN[pos++] = ' ';
    FEN[pos++] = '0';
    FEN[pos++] = ' ';
    FEN[pos++] = '1';

    // Mark the end of the string
    FEN[pos] = char(0);

    return string (FEN);
  }

  string
  stripFen (string fen)
  {
    istringstream fen_stream (fen);
    string strippedFen;
    fen_stream >> strippedFen;
    return strippedFen;
  }

  void
  clearGame ()
  {
    UCI::loop ("stop"); //stop the current search
    ponderHitFEN == "";
    computerMoveFENReached = false;
    searching = false;
    game.clear (); //reset the game
    TT.clear ();
    resetClock ();
    if (clockMode == BLITZ || clockMode == BLITZFISCHER)
      printTimeOnClock (wTime, bTime, true, true);
    else
      dgtnixPrintMessageOnClock ("newgam", false, false);
    plyCount = 0;
    pgnFile << "\n";
  }

  string
  computeCastlingRights (string fen)
  {
    Position customPos (fen, false, Threads.main_thread ());
    string outputFEN;
    bool canCastle = false;

    if (customPos.piece_on (SQ_E1) == W_KING && customPos.piece_on (SQ_H1) == W_ROOK)
      {
        canCastle = true;
        outputFEN.append ("K");
      }

    if (customPos.piece_on (SQ_E1) == W_KING && customPos.piece_on (SQ_A1) == W_ROOK)
      {
        canCastle = true;
        outputFEN.append ("Q");
      }

    if (customPos.piece_on (SQ_E8) == B_KING && customPos.piece_on (SQ_H8) == B_ROOK)
      {
        canCastle = true;
        outputFEN.append ("k");
      }

    if (customPos.piece_on (SQ_E8) == B_KING && customPos.piece_on (SQ_A8) == B_ROOK)
      {
        canCastle = true;
        outputFEN.append ("q");
      }

    if (!canCastle) outputFEN.append ("-");

    return outputFEN;

  }

  /// Change UCI parameters with special positions on the board

  void
  configure (string fen)
  {
    //    cout << "Fen received: "<< fen;
    //set skill level
    static string skillFENs[] = {
      "rnbqkbnr/pppppppp/q7/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/1q6/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/2q5/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/3q4/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/4q3/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/5q2/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/6q1/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/7q/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/8/q7/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/8/1q6/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/8/2q5/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/8/3q4/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/8/4q3/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/8/5q2/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/8/6q1/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/8/7q/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/8/8/q7/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/8/8/1q6/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/8/8/2q5/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/8/8/3q4/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "rnbqkbnr/pppppppp/8/8/4q3/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    };
    static vector<string> skill (skillFENs, skillFENs + 21);

    unsigned int idx = find (skill.begin (), skill.end (), fen) - skill.begin ();
    if (idx < skill.size ())
      {
        stringstream ss_uci, ss_dgt;
        ss_uci << "setoption name Skill Level value " << idx;
        ss_dgt << "lvl" << setw (3) << idx;
        UCI::loop (ss_uci.str ());
        dgtnixPrintMessageOnClock (ss_dgt.str ().c_str (), true, false);
      }

    /*
    Rank 6: 'Fixed time' or 'Medium Time' 
    1, 3, 5, 10, 15, 30, 60, 120 seconds 
    
    Rank 5: 'Tournament levels' 
    40/4 (40 moves in 4 minutes), 60/15, 60/30, 30/30, 30/60, , 40/40, 40/120, 40/150 
    
    Rank 4: 'Blitz' 
    1, 3, 5, 10, 15, 30, 60, 90  minutes
    
    Rank 3 : 'Blitz Fischer' + 'Special Lvl' 
    3+2, 3+5, 4+5, 5+1, 15+5, 20+10, opponents average, 1/2 opponents average 
    --------------------------------------------------------------------------
    I am thinking for a way to put the time control easily.
    One option is the following. If you put on the board only the white queen (i think it is the auxiliriary queen that comes with the dgt), the time control is the same for both sides. But if you put also the black queen it indicates handicap level.
    I think that the handicap level has no sense with Fixed time, that is, 6th rank.
    In the rest of levels if you put white queen on rank 3,4 or 5, so you can use the 6th rank to put the black queen indicating the time control for black.
    So the rules are 
    1) the rank of white queen indicates the type of control, the same for both if you only put the white queen.
    2) If you put the black queen (always on the 6th rank), you can put different time control for each color.
    3) the FixedTime levels has no option to specify different times for each color because this levels has no strict control.
    (Javier's idea)
    +control opponent's average rate (from 30% to 100%) with black queen on 6th rank
     */

    //set time control
    //fixed time per move modes : 1, 3, 5, 10, 15, 30, 60, 120 seconds
    if (fen == "rnbqkbnr/pppppppp/Q7/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("mov001", true, DGTNIX_RIGHT_DOT);
        fixedTime = 1000;
        clockMode = FIXEDTIME;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/1Q6/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("mov003", true, DGTNIX_RIGHT_DOT);
        fixedTime = 3000;
        clockMode = FIXEDTIME;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/2Q5/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("mov005", true, DGTNIX_RIGHT_DOT);
        fixedTime = 5000;
        clockMode = FIXEDTIME;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/3Q4/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("mov010", true, DGTNIX_RIGHT_DOT);
        fixedTime = 10000;
        clockMode = FIXEDTIME;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/4Q3/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("mov015", true, DGTNIX_RIGHT_DOT);
        fixedTime = 15000;
        clockMode = FIXEDTIME;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/5Q2/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("mov030", true, DGTNIX_RIGHT_DOT);
        fixedTime = 30000;
        clockMode = FIXEDTIME;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/6Q1/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("mov100", true, DGTNIX_RIGHT_DOT);
        fixedTime = 60000;
        clockMode = FIXEDTIME;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/7Q/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("mov200", true, DGTNIX_RIGHT_DOT);
        fixedTime = 120000;
        clockMode = FIXEDTIME;
        resetClock ();
      }
    //blitz modes : 1, 3, 5, 10, 15, 30, 60, 90  minutes
    if (fen == "rnbqkbnr/pppppppp/8/8/Q7/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("bli100", true, DGTNIX_RIGHT_DOT);
        blitzTime = 60000;
        clockMode = BLITZ;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/8/8/1Q6/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("bli300", true, DGTNIX_RIGHT_DOT);
        blitzTime = 180000;
        clockMode = BLITZ;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/8/8/2Q5/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("bli500", true, DGTNIX_RIGHT_DOT);
        blitzTime = 300000;
        clockMode = BLITZ;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/8/8/3Q4/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("bli000", true, DGTNIX_RIGHT_DOT | DGTNIX_RIGHT_1);
        blitzTime = 600000;
        clockMode = BLITZ;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/8/8/4Q3/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("bli500", true, DGTNIX_RIGHT_DOT | DGTNIX_RIGHT_1);
        blitzTime = 900000;
        clockMode = BLITZ;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/8/8/5Q2/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("bli030", true, DGTNIX_RIGHT_SEMICOLON);
        blitzTime = 1800000;
        clockMode = BLITZ;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/8/8/6Q1/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("bli100", true, DGTNIX_RIGHT_SEMICOLON);
        blitzTime = 3600000;
        clockMode = BLITZ;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/8/8/7Q/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("bli130", true, DGTNIX_RIGHT_SEMICOLON);
        blitzTime = 5400000;
        clockMode = BLITZ;
        resetClock ();
      }
    //'Blitz Fischer' + 'Special Lvl' 3+2, 3+5, 4+5, 5+1, 15+5, 20+10, opponents average, 1/2 opponents average
    if (fen == "rnbqkbnr/pppppppp/8/8/8/Q7/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("f 32  ", true, false);
        // The stockfish controls are all in milliseconds
        // 3 minutes => 3 * 60 * 1000 to convert milliseconds to seconds
        blitzTime = 3 * 60 * 1000;
        fischerInc = 2 * 1000;
        clockMode = BLITZFISCHER;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/8/8/8/1Q6/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("f 42  ", true, false);
        blitzTime = 4 * 60 * 1000;
        fischerInc = 2 * 1000;
        clockMode = BLITZFISCHER;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/8/8/8/2Q5/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("f 53  ", true, false);
        blitzTime = 5 * 60 * 1000;
        fischerInc = 3 * 1000;
        clockMode = BLITZFISCHER;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/8/8/8/3Q4/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("f 55  ", true, false);
        blitzTime = 5 * 60 * 1000;
        fischerInc = 5 * 1000;
        clockMode = BLITZFISCHER;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/8/8/8/4Q3/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("f155  ", true, false);
        blitzTime = 15 * 60 * 1000;
        fischerInc = 5 * 1000;
        clockMode = BLITZFISCHER;
        resetClock ();
      }
    if (fen == "rnbqkbnr/pppppppp/8/8/8/5Q2/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("f2510 ", true, false);
        blitzTime = 25 * 60 * 1000;
        fischerInc = 10 * 1000;
        clockMode = BLITZFISCHER;
        resetClock ();
      }

    // TODO: Fix FEN!
    if (fen == "rnbqkbnr/pppppppp/8/8/8/6Q1/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("f9030 ", true, false);
        blitzTime = 90 * 60 * 1000;
        fischerInc = 30 * 1000;
        clockMode = BLITZFISCHER;
        resetClock ();
      }


    // Select Game modes
    // White queen on a5
    if (fen == "rnbqkbnr/pppppppp/8/Q7/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("  book", true, false);
        playMode = BOOK;
        // Reset clock mode to fixed time if the current mode is infinite, this can cause bugs if switching from ANALYSIS mode
        if (clockMode == INFINITE)
          {
            clockMode = FIXEDTIME;
          }
        resetClock ();
      }
    // White queen on b5
    if (fen == "rnbqkbnr/pppppppp/8/1Q6/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("analyz", true, false);
        playMode = ANALYSIS;
        clockMode = INFINITE;
        resetClock ();
      }
    // White queen on c5
    if (fen == "rnbqkbnr/pppppppp/8/2Q5/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock (" train", true, false);
        playMode = TRAINING;
        clockMode = INFINITE;
        resetClock ();
      }
    // White queen on d5
    if (fen == "rnbqkbnr/pppppppp/8/3Q4/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("  game", true, false);
        playMode = GAME;
        // Reset clock mode to fixed time if the current mode is infinite, this can cause bugs if switching from ANALYSIS mode
        if (clockMode == INFINITE)
          {
            clockMode = FIXEDTIME;
          }
        resetClock ();
      }

    // White queen on e5
    if (fen == "rnbqkbnr/pppppppp/8/4Q3/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock ("chatty", true, false);
        playMode = KIBITZ;
        // Reset clock mode to fixed time if the current mode is infinite, this can cause bugs if switching from ANALYSIS mode
        if (clockMode == INFINITE)
          {
            clockMode = FIXEDTIME;
          }
        resetClock ();
      }

    if (setupPosition)
      {
        // Need only the raw position portion of fen
        string strippedFen = stripFen (fen);

        // Search for both white and black kings
        if (strippedFen.find ('K') != string::npos && strippedFen.find ('k') != string::npos)
          {
            // if white and black kings exist, see that the position has occurred twice in the last 3 FENs
            // If so, this is the new starting position!
            int matches = 0;
            for (deque<string>::iterator it = fenQueue.begin (); it != fenQueue.end (); ++it)
              {
                //                cout <<" Fen queue contains: "<<*it<<"\n";
                if (fen == *it)
                  {
                    ++matches;
                  }
                else if (matches >= 1)
                  {
                    // Read NON matching FEN to see which piece was removed
                    //Analyze stripped fen to see if the computer moves as white or black
                    string prevMatchingFen = stripFen (*it);
                    //Play vs computer if removing a king
                    // If not (main condition), then analysis mode is turned on
                    if (prevMatchingFen.find ('k') != string::npos && prevMatchingFen.find ('K') != string::npos)
                      {
                        clockMode = INFINITE;
                        playMode = ANALYSIS;
                      }
                    else
                      {
                        clockMode = FIXEDTIME;
                        playMode = GAME;
                      }

                  }
              }
            if (matches >= 1)
              {
                // match
                setupPosition = false;
                customPosition = true;
                string testFen = string (strippedFen);

                if (computerPlays == BLACK) testFen.append (" w ");
                else testFen.append (" b ");

                testFen.append (computeCastlingRights (strippedFen));
                testFen.append (" 0 1");

                customStartFEN = new char[strlen (testFen.c_str ())];
                strcpy (customStartFEN, testFen.c_str ());
                clearGame ();
              }

          }

        addToFenQueue (fen);
      }

    //choose opening book
    typedef map<string, string> BookMap;

    static const BookMap::value_type rawData[] = {
      BookMap::value_type ("rnbqkbnr/pppppppp/8/8/8/q7/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "nobook"),
      BookMap::value_type ("rnbqkbnr/pppppppp/8/8/8/1q6/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "fun"),
      BookMap::value_type ("rnbqkbnr/pppppppp/8/8/8/2q5/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "anand"),
      BookMap::value_type ("rnbqkbnr/pppppppp/8/8/8/3q4/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "korchnoi"),
      BookMap::value_type ("rnbqkbnr/pppppppp/8/8/8/4q3/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "larsen"),
      BookMap::value_type ("rnbqkbnr/pppppppp/8/8/8/5q2/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "pro"),
      BookMap::value_type ("rnbqkbnr/pppppppp/8/8/8/6q1/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "gm2001"),
      BookMap::value_type ("rnbqkbnr/pppppppp/8/8/8/7q/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "varied"),
      BookMap::value_type ("rnbqkbnr/pppppppp/8/8/7q/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "gm1950"),
      BookMap::value_type ("rnbqkbnr/pppppppp/8/8/6q1/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "performance"),
      BookMap::value_type ("rnbqkbnr/pppppppp/8/8/5q2/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "stfish")
    };

    // Warning need to increment the below number for more book additions to work!
    BookMap book (rawData, rawData + 11);
    BookMap::iterator it = book.find (fen);
    if (it != book.end ())
      {
        string s = it->second;
        UCI::loop (string ("setoption name Book File value ") + bookPath + s + ".bin");
        UCI::loop (string ("setoption name OwnBook value ")+(s.compare ("nobook") ? "true" : "false"));
        if (s.size () < 6) s.insert (s.begin (), 6 - s.size (), ' ');
        dgtnixPrintMessageOnClock (s.c_str (), true, false);
      }

    //board orientation
    if (fen == "RNBKQBNR/PPPPPPPP/8/8/8/8/pppppppp/rnbkqbnr w KQkq - 0 1" || fen == "8/8/8/8/8/8/8/q6q w KQkq - 0 1" || fen == "Q6Q/8/8/8/8/8/8/8 w KQkq - 0 1")
      {
        dgtnixSetOption (DGTNIX_BOARD_ORIENTATION, boardReversed ? DGTNIX_BOARD_ORIENTATION_CLOCKLEFT : DGTNIX_BOARD_ORIENTATION_CLOCKRIGHT);
        boardReversed = !boardReversed;
        sem_post (&dgtnixEventSemaphore); //trigger new game start
      }

    // Setup Custom position - white to move
    // White queens on a1 and h1
    // No beeps as it makes sounds for a FEN setup
    if (fen == "8/8/8/8/8/8/8/Q6Q w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock (" setup", false, false);
        setupPosition = true;
        computerPlays = BLACK;
        resetClock ();
      }

    // Setup Custom position - black to move
    // Black queens on a8 and h8
    if (fen == "q6q/8/8/8/8/8/8/8 w KQkq - 0 1")
      {
        dgtnixPrintMessageOnClock (" setup", false, false);
        setupPosition = true;
        computerPlays = WHITE;
        resetClock ();
      }

    //set side to play (simply remove the king of the side you are playing and put it back on the board)
    if (fen == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1BNR w KQkq - 0 1")
      {
        cout << "You play white" << endl;
        computerPlays = BLACK;
      }
    if (fen == "rnbq1bnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
      {
        cout << "You play black" << endl;
        computerPlays = WHITE;
      }

    //new game
    if (fen == StartFEN && !game.empty ())
      {
        clearGame ();
        customPosition = false;
      }

    //shutdown
    if (fen == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQQBNR w KQkq - 0 1"
        || fen == "8/8/8/8/8/8/8/3QQ3 w KQkq - 0 1")
      {
        UCI::loop ("stop"); //stop the current search
        if (!system ("shutdown -h now"))
          dgtnixPrintMessageOnClock ("pwroff", true, false);
      }
  }

  /// Prints a move on the dgt clock

  void
  printMoveOnClock (Move move, unsigned char beep = true)
  {
    //print the move on the clock
    string dgtMove = move_to_uci (move, false);
    dgtMove.insert (2, 1, ' ');
    if (dgtMove.length () < 6)
      dgtMove.append (" ");
    cout << '[' << dgtMove << ']' << endl;
    dgtnixPrintMessageOnClock (dgtMove.c_str (), beep, false);
  }

  /// Test if the given fen is playable in the current game.
  /// If true, return the move leading to this fen, else return MOVE_NONE

  Move
  isPlayable (const string& _fen)
  {
    Position pos (getStartFEN (), false, Threads.main_thread ()); // The root position
    stack<StateInfo> states;
    string fen = _fen.substr (0, _fen.find (' '));

    //First, we do all the game moves
    for (vector<Move>::iterator it = game.begin (); it != game.end (); ++it)
      {
        states.push (StateInfo ());
        pos.do_move (*it, states.top ());
      }

    //Check is the fen is playable in current game position
    for (MoveList<LEGAL> ml (pos); !ml.end (); ++ml)
      {
        StateInfo state;
        pos.do_move (ml.move (), state);
        if (pos.fen ().find (fen) != string::npos)
          return ml.move ();
        pos.undo_move (ml.move ());
      }

    //Next we check from the end of the game to the beginning if we reached a position already played
    //If this is the case, we takeback the moves and return MOVE_NONE
    bool continueSearch = true;
    for (vector<Move>::reverse_iterator rit = game.rbegin (); rit < game.rend () && continueSearch; ++rit)
      {
        pos.undo_move (*rit);
        if (pos.side_to_move () != computerPlays) continueSearch = false; //stop searching in the game when we reached a position where human has the move
        if ((pos.fen ().find (fen) != string::npos) && (pos.side_to_move () != computerPlays || clockMode == INFINITE)) //we found a position that was played
          {
            UCI::loop ("stop"); //stop the current search
            ponderHitFEN = "";
            cout << "Rolling back to position" << pos.fen() << endl;
            dgtnixPrintMessageOnClock (" undo ", true, false);
            pgnFile << "\n";
            sleep(1);
            printMoveOnClock(*(rit+2).base());
            rewritePGN = true;
            plyCount = 0;
            game.erase ((rit + 1).base (), game.end ()); //delete the moves from the game
            if (clockMode == INFINITE) {
                return MOVE_NULL;
            } else {
                return MOVE_NONE;
            }
          }
      }

    return MOVE_NONE;
  }

  string
  getDgtTimeString (int time)
  {
    if (time < 0) return string ("   ");
    stringstream oss;
    time /= 1000;
    if (time < 1200) //minutes.seconds mode
      {
        int minutes = time / 60;
        int seconds = time - minutes * 60;
        if (minutes >= 10) minutes -= 10;
        oss << minutes << setfill ('0') << setw (2) << seconds;
      }
    else //hours:minutes mode
      {
        int hours = time / 3600;
        int minutes = (time - (hours * 3600)) / 60;
        oss << hours << setfill ('0') << setw (2) << minutes;
      }
    return oss.str ();
  }

  /// Print time on dgt clock

  void
  printTimeOnClock (int wClockTime, int bClockTime, bool wDots = true, bool bDots = true)
  {
    string s;
    unsigned char dots = 0;
    if (!boardReversed)
      {
        s = getDgtTimeString (wClockTime) + getDgtTimeString (bClockTime);
        //white
        if (wClockTime < 1200000) //minutes.seconds mode
          {
            if (wDots) dots |= DGTNIX_LEFT_DOT;
            if (wClockTime >= 600000) dots |= DGTNIX_LEFT_1;
          }
        else if (wDots) dots |= DGTNIX_LEFT_SEMICOLON; //hours:minutes mode
        //black
        if (bClockTime < 1200000) //minutes.seconds mode
          {
            if (bDots) dots |= DGTNIX_RIGHT_DOT;
            if (bClockTime >= 600000) dots |= DGTNIX_RIGHT_1;
          }
        else if (bDots) dots |= DGTNIX_RIGHT_SEMICOLON; //hours:minutes mode
      }
    else
      {
        s = getDgtTimeString (bClockTime) + getDgtTimeString (wClockTime);
        //black
        if (bClockTime < 1200000) //minutes.seconds mode
          {
            if (bDots) dots |= DGTNIX_LEFT_DOT;
            if (bClockTime >= 600000) dots |= DGTNIX_LEFT_1;
          }
        else if (bDots) dots |= DGTNIX_LEFT_SEMICOLON; //hours:minutes mode
        //white
        if (wClockTime < 1200000) //minutes.seconds mode
          {
            if (wDots) dots |= DGTNIX_RIGHT_DOT;
            if (wClockTime >= 600000) dots |= DGTNIX_RIGHT_1;
          }
        else if (wDots) dots |= DGTNIX_RIGHT_SEMICOLON; //hours:minutes mode
      }
    dgtnixPrintMessageOnClock (s.c_str (), false, dots);
  }

  void*
  wakeUpEverySecond (void*)
  {
    while (true)
      {
        sleep (1);
        sem_post (&dgtnixEventSemaphore);
      }
    return NULL;
  }

  void
  fitStringToDgt (string& s)
  {
    // The string manipulation algorithm can be optimized to doing one call for erase and appending blanks if needed.
    // erase from end if more than 6 chars
    while (s.length () > 6)
      {
        s.erase (s.length () - 1, 1);
      }

    while (s.length () < 6)
      {
        s = " " + s;
      }

  }

  void
  display_top_book_moves (PolyglotBook& book, const Position& pos, const int num)
  {
    // Display top 3 moves in reverse order of strength so that the top move is on the clock. 3 Moves without delay is not that bad
    vector<Move> book_moves = book.probe_moves (pos, Options["Book File"], num);

    for (vector<Move>::reverse_iterator it = book_moves.rbegin (); it != book_moves.rend (); ++it)
      {
        // Dont beep when showing book moves, can be annoying
        printMoveOnClock (*it, false);
        //                            sleep(1);
      }
  }

  void*
  infiniteAnalysis (void *)
  {

    while (true)
      {
        //        cout <<"Infinite clock mode: "<< clockMode;
        //        cout <<"\n searching: "<<searching;
        //        cout <<"\n";
        if ((clockMode == INFINITE || playMode==KIBITZ) && searching)
          {
            sleep (2);
            // Dont show analysis if there is no longer a search
            if (!searching) continue;
            //            cout << "Infinite analysis!\n";

            string uci_score = Search::UciPvDgt.score;

            // Remove the words 'cp' from output and replace with just 'p' (centipawns) to save clock space
            if (uci_score[0] == 'c' && uci_score[1] == 'p')
              {
                uci_score.erase (0, 2);
              }
            // Some trimming on the 'mate' message to save space on the DGT clock. Simple 'm' is sufficient
            if (uci_score[0] == 'm' && uci_score[1] == 'a' && uci_score[2] == 't' && uci_score[3] == 'e')
              {
                uci_score.erase (1, 3);
              }

            replace (uci_score.begin (), uci_score.end (), '-', 'n'); //Replace minus sign with 'n' as minus sign is not available on DGT

            fitStringToDgt (uci_score);
            dgtnixPrintMessageOnClock (uci_score.c_str (), false, false);

            stringstream s_depth;
            s_depth << Search::UciPvDgt.depth;
            string depth_str = s_depth.str ();
            depth_str = 'd' + depth_str; // Add a 'd' in front on depth to make output clear
            fitStringToDgt (depth_str);

            if (playMode !=KIBITZ) {
                // Dont print depth while kibitzing
                dgtnixPrintMessageOnClock (depth_str.c_str (), false, false);
              }
            //Display the best move computer suggestion only in analysis mode
            if (playMode == ANALYSIS)
              {
                printMoveOnClock (Search::RootMoves[0].pv[0], false);
              }

          }
      }
    return NULL;

  }

  bool
  blink ()
  {
    return (Time::now () / 1000) % 2;
  } //returns alternatively true or false every second

  string
  getPgn (Position pos, Move move)
  {
    //    MoveList<LEGAL> ml(pos); //the legal move list
    std::string pgn;

    // Keep track of position keys along the setup moves (from start position to the
    // position just before to start searching). Needed by repetition draw detection.
    Search::StateStackPtr SetupStates = Search::StateStackPtr (new std::stack<StateInfo > ());

    // Write header if its the first move
    if (plyCount==0)
      {
        if (playMode == ANALYSIS)
          {
            pgn.append ("Analysis\n");
          }
        else if (computerPlays == WHITE)
          {
            pgn.append ("Stockfish - User\n");
          }
        else
          {
            pgn.append ("User - Stockfish\n");
          }
      }

    ++plyCount;
    if (plyCount % 2 == 1)
      {
        stringstream ss;
        ss << plyCount/2+1;
        pgn.append (ss.str ());
        pgn.append (". ");
      }
    pgn.append (move_to_san (pos, move));
    pgn.append (" ");
    
    // New line after every ten moves 
    if (plyCount % 20 == 0)
      {
        pgn.append("\n");
      }

    return pgn;
  }

  void
  loop (const string& args)
  {
    // Initialization
    pgnFile.open ("game.pgn");
    computerPlays = BLACK;
    fixedTime = 5000;
    clockMode = FIXEDTIME;
    playMode = GAME;
    resetClock (); //search defaults to 5 seconds per move
    Move playerMove = MOVE_NONE;
    static PolyglotBook book; // Defined static to initialize the PRNG only once
    Time::point searchStartTime = Time::now ();
    string computerMoveFEN = "";

    // DGT Board Initialization
    int BoardDescriptor;
    char port[256];
    dgtnixSetOption (DGTNIX_DEBUG, DGTNIX_DEBUG_WITH_TIME); //all debug informations are printed
    strncpy (port, args.c_str (), 256);
    BoardDescriptor = dgtnixInit (port);
    int err = dgtnix_errno;
    if (BoardDescriptor < 0)
      {
        cout << "Unable to connect to DGT board on port " << port << " : ";
        switch (BoardDescriptor)
          {
          case -1:
            cout << strerror (err) << endl;
            break;
          case -2:
            cout << "Not responding to the DGT_SEND_BRD message" << endl;
            break;
          default:
            cout << "Unrecognized response to the DGT_SEND_BRD message :"
                    << BoardDescriptor << endl;
          }
        exit (-1);
      }
    cout << "The board was found" << BoardDescriptor << endl;
    sleep (3);
    dgtnixUpdate ();
    dgtnixPrintMessageOnClock ("pic016", true, DGTNIX_RIGHT_DOT); //Display version number

    //Engine options
    UCI::loop ("setoption name Hash value 512");
    UCI::loop ("setoption name Emergency Base Time value 1300"); //keep 1 second on clock
    UCI::loop (string ("setoption name Book File value ") + bookPath + "varied.bin"); //default book
    UCI::loop (string ("setoption name OwnBook value true"));

    // Get the first board state
    string currentFEN = getDgtFEN ();
    configure (currentFEN); //useful for orientation

    // Start the wakeup thread
    pthread_t wakeUpThread;
    pthread_create (&wakeUpThread, NULL, wakeUpEverySecond, (void*) NULL);

    pthread_t infiniteThread;
    pthread_create (&infiniteThread, NULL, infiniteAnalysis, (void*) NULL);

    // Main DGT event loop
    while (true)
      {
        Position pos;
        sem_wait (&dgtnixEventSemaphore);
        string s = getDgtFEN ();

        //Display time on clock
        if (clockMode == FIXEDTIME && searching && limits.movetime >= 5000) //If we are in fixed time per move mode, display computer remaining time 
          {
            int remainingTime = limits.movetime - (Time::now () - searchStartTime);
            if (remainingTime >= 1000)
              {
                if (computerPlays == WHITE) printTimeOnClock (remainingTime, -1, blink (), false);
                else printTimeOnClock (-1, remainingTime, false, blink ());
              }
          }
        else if ((clockMode == BLITZ || clockMode == BLITZFISCHER) && (searching || (computerMoveFENReached && !isPlayable (s)))) //blitz mode and computer or player thinking
          {
            if (searching != (computerPlays == BLACK)) printTimeOnClock (wTime - (Time::now () - searchStartTime), bTime, blink (), true);
            else printTimeOnClock (wTime, bTime - (Time::now () - searchStartTime), true, blink ());
          }

        if (currentFEN != s)
          { //There is some change on the DGT board
            currentFEN = s;

            cout << currentFEN << endl;
            configure (currentFEN); //on board configuration

            pos.set(getStartFEN (), false, Threads.main_thread ()); // The root position

            if (searching && clockMode == INFINITE)
              {
                // stop search as a new board position has occurred
                Search::Signals.stop = true;
              }

            //Test if we reached the computer move fen
            if (!searching && !computerMoveFENReached && (computerMoveFEN.find (s.substr (0, s.find (' '))) != string::npos))
              {
                computerMoveFENReached = true;
                //Add fischer increment time to the player's clock
                if (computerPlays != WHITE) wTime += fischerInc;
                else bTime += fischerInc;
                searchStartTime = Time::now (); //the player starts thinking
              }

            //Test if we reach a playable position in the current game
            Move move = isPlayable (currentFEN);
            cout << "-------------------------Move:" << move << endl;
           
            if (move != MOVE_NONE || (!currentFEN.compare (getStartFEN ()) && (computerPlays == WHITE || clockMode == INFINITE)))
              {
                if (move == MOVE_NULL && clockMode == INFINITE)
                  {
                    // To support UNDO move operation in infinite analysis mode
                    move = MOVE_NONE;  
                  }
                
                //if(searching) UCI::loop("stop"); //stop the current search
                playerMove = move;

                //player has just moved : we need to update his remaining time
                if (!game.empty ())
                  {
                    if (computerPlays == WHITE) bTime -= (Time::now () - searchStartTime);
                    else wTime -= (Time::now () - searchStartTime);
                    searchStartTime = Time::now (); //needed if player undoes a move
                  }

                MoveList<LEGAL> ml (pos); //the legal move list

                // Keep track of position keys along the setup moves (from start position to the
                // position just before to start searching). Needed by repetition draw detection.
                Search::StateStackPtr SetupStates = Search::StateStackPtr (new std::stack<StateInfo > ());                 
                
                //Do all the game moves
                for (vector<Move>::iterator it = game.begin (); it != game.end (); ++it)
                  {
                    SetupStates->push (StateInfo ());
                    // In INFINITE analysis/training mode, every move is a player move and thus there is no need to write
                    // the move from the game move list
                    if (rewritePGN || (*it == game.back () && clockMode!=INFINITE))
                      {
                        Move m = *it;
                        pgnFile << getPgn( pos, m);
                        pgnFile.flush();
                      }
                    pos.do_move (*it, SetupStates->top ());
                    
                  }
                if (rewritePGN) rewritePGN = false;
                   
                if (move != MOVE_NONE)
                  {
                    SetupStates->push (StateInfo ());
                    if (!Search::UciPvDgt.score.empty ()) {
                        pgnFile << " ( { "<< Search::UciPvDgt.score<< " depth "<<Search::UciPvDgt.depth<< " } "<<Search::UciPvDgt.pv << " ) ";
                      }
                    pgnFile << getPgn( pos, playerMove);
                    pgnFile.flush();
                    pos.do_move (playerMove, SetupStates->top ()); //Do the board move
                  }

                //Add fischer increment time to the computer's clock
                if (computerPlays == WHITE) wTime += fischerInc;
                else bTime += fischerInc;

                //Check if we can find a move in the book
                Move bookMove = book.probe (pos, Options["Book File"], Options["Best Book Move"]);
                if (bookMove && Options["OwnBook"])
                  {
                    UCI::loop ("stop");
                    searching = false;
                    dgtnixPrintMessageOnClock ("  book", false, false); //don't play immediately, wait for 1 second
                    //do the moves in the game
                    if (playerMove != MOVE_NONE) game.push_back (playerMove);

                    if (playMode != GAME && playMode != BOOK && playMode != KIBITZ)
                      {
                        display_top_book_moves (book, pos, 3);
                      }

                    else
                      {
                        printMoveOnClock (bookMove);
                        game.push_back (bookMove);
                        if (playMode == BOOK)
                          {
                            pos.do_move (bookMove, SetupStates->top ());
                            sleep (3);
                            display_top_book_moves (book, pos, 3);
                          }

                      } // Show computer book moves in non game mode
                    // In book mode, only the the player's book moves are shown!

                    if (!Search::RootMoves.empty ()) Search::RootMoves[0].pv[1] = MOVE_NONE; //No pondering
                    goto finishSearch;
                  }
                  //Check for a draw : whether the position is drawn by material repetition, or the 50 moves rule.
                  //It does not detect stalemates
                else if (pos.is_draw())
                  dgtnixPrintMessageOnClock ("  draw", true, false);
                  /*//Check if there is a single legal move
                  else if(ml.size()==1)
                  {
                      //sleep(1); //don't play immediately, wait for 1 second
                                  printMoveOnClock(ml.move());
                                          //do the moves in the game
                                          if(playerMove!=MOVE_NONE) game.push_back(playerMove);
                                          game.push_back(ml.move());
                      goto finishSearch;
                  }*/
                else if (ml.size ()) //Launch the search if there are legal moves
                  {
                    searchStartTime = Time::now ();
                    if (ponderHitFEN.find (currentFEN.substr (0, currentFEN.find (' '))) != string::npos /*&& Search::Signals.stop == false*/)
                      {
                        cout << "ponderhit!!" << endl;
                        UCI::loop ("ponderhit");
                      }
                    else
                      {
                        UCI::loop ("stop");
                        //set time limits
                        if (clockMode == BLITZ || clockMode == BLITZFISCHER)
                          {
                            limits.time[WHITE] = max (wTime, 0);
                            limits.time[BLACK] = max (bTime, 0);
                            limits.inc[WHITE] = limits.inc[BLACK] = fischerInc;
                          }
                        limits.ponder = false;
                        ponderHitFEN = "";
                        cout << "launch serach!!" << endl;
                        Threads.start_thinking (pos, limits, vector<Move > (), SetupStates);
                      }
                    searching = true;
                  }
                else //no move to play : we are mate or stalemate
                  {
                    cout << "mate of stalemate!!" << endl;
                    if (pos.checkers()) dgtnixPrintMessageOnClock ("  mate", true, false);
                    else dgtnixPrintMessageOnClock ("stlmat", true, false);
                  }
              } // end if computerPlays == WHITE

          }

        //Check for finished search
        if (Search::Signals.stop == true && searching)
          {
            cout << "Finished search";
            searching = false;

            //update clock remaining time
            if (computerPlays == WHITE) wTime -= (Time::now () - searchStartTime);
            else bTime -= (Time::now () - searchStartTime);

            cout << "stopped with move " << move_to_uci (Search::RootMoves[0].pv[0], false) << endl;
            //do the moves in the game
            if (playerMove != MOVE_NONE) game.push_back (playerMove);

            if (clockMode != INFINITE)
              {
                printMoveOnClock (Search::RootMoves[0].pv[0]);
                game.push_back (Search::RootMoves[0].pv[0]);
              }

finishSearch:
            //set the FEN we are waiting ofr on the board
            pos.set(getStartFEN (), false, Threads.main_thread ()); // The root position
            // Keep track of position keys along the setup moves (from start position to the
            // position just before to start searching). Needed by repetition draw detection.
            Search::StateStackPtr SetupStates = Search::StateStackPtr (new std::stack<StateInfo > ());
            ;
            //Do all the game moves
            for (vector<Move>::iterator it = game.begin (); it != game.end (); ++it)
              {
                SetupStates->push (StateInfo ());
                pos.do_move (*it, SetupStates->top ());
              }
            computerMoveFEN = pos.fen();
            computerMoveFENReached = false;

            MoveList<LEGAL> ml (pos); //the legal move list
            //check for draw
            if (pos.is_draw())
              {
                sleep (3);
                dgtnixPrintMessageOnClock ("  draw", true, false);
              }
              //check for mate or stalemate
            else if (!ml.size ())
              {
                sleep (3);
                if (pos.checkers()) dgtnixPrintMessageOnClock ("  mate", true, false);
                else dgtnixPrintMessageOnClock ("stlmat", true, false);
              }
              //Ponder
            else if ((playMode == GAME || playMode == BOOK || playMode == KIBITZ) && !Search::RootMoves.empty () && Search::RootMoves[0].pv[1] != MOVE_NONE)
              {
                game.push_back (Search::RootMoves[0].pv[1]);
                pos.do_move (Search::RootMoves[0].pv[1], SetupStates->top ());
                ponderHitFEN = pos.fen();
                //Launch ponder search
                if (clockMode == BLITZ || clockMode == BLITZFISCHER)
                  {
                    limits.time[WHITE] = max (wTime, 0);
                    limits.time[BLACK] = max (bTime, 0);
                    limits.inc[WHITE] = limits.inc[BLACK] = fischerInc;
                  }
                limits.ponder = true;
                Threads.start_thinking(pos, limits, vector<Move > (), SetupStates);
                game.pop_back ();
              }
            else ponderHitFEN = "";
          }

      }

    dgtnixClose ();
  }

}
