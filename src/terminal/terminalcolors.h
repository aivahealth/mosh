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

#ifndef TERMINAL_COLORS_HPP
#define TERMINAL_COLORS_HPP

#include <cstdint>
#include <climits>
#include <string>
#include <vector>

namespace Terminal {
inline bool valid_default_color( const std::string& color, bool complete = true )
{
  unsigned int digits = 0, components = 1;
  for ( unsigned char c : color ) {
    if ( c == '/' ) {
      if ( digits == 0 || ++components > 3 ) {
        return false;
      }
      digits = 0;
    } else if ( ( c >= '0' && c <= '9' ) || ( c >= 'a' && c <= 'f' ) || ( c >= 'A' && c <= 'F' ) ) {
      if ( ++digits > 4 ) {
        return false;
      }
    } else {
      return false;
    }
  }
  return !complete || ( components == 3 && digits > 0 );
}

struct DefaultColor
{
  unsigned int slot;
  std::string color;
};

/* Split local terminal replies from keystrokes without waiting for a reply.
   Only incomplete escape prefixes are held, for at most 50 ms of inactivity. */
class ColorReplyParser
{
private:
  std::string pending;
  bool paste = false;
  uint64_t deadline = 0;

public:
  std::vector<DefaultColor> updates;

  int wait_time( uint64_t now ) const
  {
    return pending.empty() ? INT_MAX : ( now >= deadline ? 0 : int( deadline - now ) );
  }

  std::string expire( uint64_t now )
  {
    if ( wait_time( now ) != 0 ) {
      return std::string();
    }
    std::string result;
    result.swap( pending );
    return result;
  }

  std::string feed( const std::string& input, uint64_t now )
  {
    std::string output = expire( now );
    updates.clear();
    for ( char c : input ) {
      pending += c;
      deadline = now + 50;
      while ( !pending.empty() ) {
        const std::string marker = paste ? "\033[201~" : "\033[200~";
        if ( marker.compare( 0, pending.size(), pending ) == 0 ) {
          if ( marker == pending ) {
            output += pending;
            pending.clear();
            paste = !paste;
          }
          break;
        }
        bool candidate = false;
        if ( !paste ) {
          for ( unsigned int slot : { 10u, 11u } ) {
            const std::string prefix = slot == 10 ? "\033]10;rgb:" : "\033]11;rgb:";
            if ( prefix.compare( 0, pending.size(), pending ) == 0 ) {
              candidate = true;
              break;
            }
            if ( pending.compare( 0, prefix.size(), prefix ) != 0 ) {
              continue;
            }
            std::string color = pending.substr( prefix.size() );
            bool complete = false;
            if ( !color.empty() && color.back() == '\007' ) {
              color.pop_back();
              complete = true;
            } else if ( color.size() >= 2 && color.compare( color.size() - 2, 2, "\033\\" ) == 0 ) {
              color.resize( color.size() - 2 );
              complete = true;
            }
            if ( complete && valid_default_color( color ) ) {
              updates.push_back( { slot, color } );
              pending.clear();
              candidate = true;
              break;
            }
            /* Validate a possibly unfinished RGB value, including half an ST. */
            if ( !complete ) {
              if ( !color.empty() && color.back() == '\033' ) {
                color.pop_back();
                candidate = valid_default_color( color );
              } else {
                candidate = valid_default_color( color, false );
              }
            }
            break;
          }
        }
        if ( candidate ) {
          break;
        }
        output += pending[0];
        pending.erase( 0, 1 );
      }
    }
    return output;
  }
};
}

#endif
