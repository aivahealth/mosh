/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
*/


#include <cstdio>
#include <cstdlib>
#include <string>
#include "src/terminal/terminalcolors.h"
#include "src/statesync/completeterminal.h"
#include "src/statesync/user.h"

static void check( bool condition, const char* message )
{
  if ( !condition ) {
    fprintf( stderr, "%s\n", message );
    exit( EXIT_FAILURE );
  }
}

int main()
{
  using Terminal::valid_default_color;
  for ( const char* color : { "f/A/0", "ebeb/dbdb/b2b2", "12/345/abcd" } ) {
    check( valid_default_color( color ), "valid RGB rejected" );
  }
  for ( const char* color : { "", "/a/b", "a//b", "a/b/", "a/b/c/d", "abcde/b/c", "gg/aa/bb",
                              "aa/bb/cc\033]52;c;bad", "rgb:aa/bb/cc" } ) {
    check( !valid_default_color( color ), "invalid RGB accepted" );
  }
  const std::string fg = "ebeb/dbdb/b2b2", bg = "2828/2828/2828";
  const std::string fg_reply = "\033]10;rgb:" + fg + "\033\\";
  const std::string bg_reply = "\033]11;rgb:" + bg + "\033\\";
  Terminal::Complete terminal( 80, 24 );
  check( terminal.act( "\033]10;?\033\\\033]11;?\007" ).empty(), "missing colors should not reply" );
  check( terminal.act( Parser::DefaultColor( 10, fg ) ) == fg_reply, "late foreground response" );
  check( terminal.act( Parser::DefaultColor( 11, bg ) ) == bg_reply, "late background response" );
  check( terminal.act( "\033]10;?\007\033]11;?\033\\" ) == fg_reply + bg_reply, "BEL/ST queries" );
  check( terminal.act( Parser::DefaultColor( 11, "not a color" ) ).empty(), "invalid update replied" );
  check( terminal.act( Parser::DefaultColor( 99, bg ) ).empty(), "invalid slot replied" );
  check( terminal.act( "\033]11;?\007" ) == bg_reply, "invalid update changed cache" );
  check( terminal.act( "\033]10;?;extra\007\033]11;#fff\007\033]4;0;?\007" ).empty(), "nonqueries replied" );
  terminal.act( Parser::DefaultColor( 11, "ffff/ffff/ffff" ) );
  check( terminal.act( "\033]11;?\007" ) == "\033]11;rgb:ffff/ffff/ffff\033\\", "cache refresh" );
  Terminal::Complete pending( 80, 24 );
  for ( int i = 0; i < 1000; ++i ) {
    pending.act( "\033]10;?\007" );
  }
  check( pending.act( Parser::DefaultColor( 10, fg ) ).size() == fg_reply.size() * 16, "pending bound" );

  terminal.act( "\033]52;p;YWJj\007" );
  const auto clipboard = terminal.get_fb().get_clipboard();
  check( clipboard == Terminal::Framebuffer::title_type( { L'p', L';', L'Y', L'W', L'J', L'j' } ),
         "OSC 52 clipboard selection changed" );
  const auto clipboard_sequence = terminal.get_fb().get_clipboard_seqnum();
  terminal.act( "\033]11;?\007\033]52;p;YWJj\007" );
  check( terminal.get_fb().get_clipboard() == clipboard
         && terminal.get_fb().get_clipboard_seqnum() != clipboard_sequence, "duplicate clipboard update lost" );

  Network::UserStream input, output;
  input.push_back( Parser::UserByte( 'a' ) );
  input.push_back( Parser::DefaultColor( 10, fg ) );
  input.push_back( Parser::Resize( 90, 30 ) );
  input.push_back( Parser::DefaultColor( 11, bg ) );
  output.apply_string( input.init_diff() );
  check( input == output, "color protocol round trip" );
  output.subtract( &input );
  check( output.empty(), "color protocol acknowledgement" );

  Terminal::ColorReplyParser parser;
  std::string text;
  size_t updates = 0;
  const std::string stream = "hello" + fg_reply + "\033]11;rgb:" + bg + "\007" + "\033[A" + "世界";
  for ( char c : stream ) {
    text += parser.feed( std::string( 1, c ), 100 );
    updates += parser.updates.size();
  }
  check( text == "hello\033[A世界" && updates == 2, "fragmented replies and Unicode keys" );
  check( parser.feed( "\033", 200 ).empty(), "ESC should be pending" );
  check( parser.expire( 249 ).empty(), "premature ESC expiry" );
  check( parser.expire( 250 ) == "\033", "missing response blocked ESC" );
  const std::string paste = "\033[200~" + fg_reply + "paste\033[201~";
  text.clear();
  for ( char c : paste ) {
    text += parser.feed( std::string( 1, c ), 300 );
    check( parser.updates.empty(), "paste interpreted as terminal reply" );
  }
  check( text == paste, "bracketed paste modified" );
  for ( const std::string bad : { "\033]10;rgb:gg/aa/bb\007", "\033]11;rgb:abcde/a/b\033\\",
                                  "\033]52;c;YWJj\007", "\033]10;rgb:a//b\007" } ) {
    check( parser.feed( bad, 400 ) == bad && parser.updates.empty(), "unrelated/malformed input lost" );
  }
  check( parser.feed( "\033]10;rgb:", 500 ).empty(), "partial reply" );
  check( parser.expire( 550 ) == "\033]10;rgb:", "partial reply not released" );
  check( parser.feed( fg_reply, 900 ).empty() && parser.updates.size() == 1, "delayed response rejected" );
  return EXIT_SUCCESS;
}
