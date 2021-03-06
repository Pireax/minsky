/*
  @copyright Steve Keen 2013
  @author Russell Standish
  This file is part of Minsky.

  Minsky is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Minsky is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Minsky.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "variableType.h"
#include <ecolab_epilogue.h>
using namespace classdesc;

#include <string>
#include <sstream>
#include <regex>
using namespace std;

namespace minsky
{
  string VariableType::typeName(int t)
  {return enumKey<Type>(t);}

  string Units::str() const
  {
    ostringstream s;
    s<<*this;
    return s.str();
  }

  Units::Units(const string& x)
  {
    if (x.empty()) return;
    // split into numerator and denominator components if / found
    auto divPos=x.find('/');
    vector<string> components;
    components.push_back(x.substr(0,divPos));
    if (divPos!=string::npos)
      components.push_back(x.substr(divPos+1));
    
    if (components.back().find('/')!=string::npos)
      throw runtime_error("too many division signs: "+x);
    
    int coef=1;
    for (auto& i: components)
      {
        if (i.empty())
          throw runtime_error("empty numerator or denominator: "+x);
        const char* b=i.c_str();
        for (const char* j=b; *j!='\0';)
          {
            if (*j=='^'||isspace(*j))
              {
                string name(b,j); // stash end of unit name
                if (name.empty())
                  throw runtime_error("empty unit name: "+x);
                while (isspace(*j)) ++j;
                if (*j=='^')
                  {
                    auto k=j+1;
                    int v=strtol(k,const_cast<char**>(&j),10);
                    if (j==k)
                      throw runtime_error("invalid exponent: "+x);
                    (*this)[name]+=coef*v;
                  }
                else
                  (*this)[name]+=coef;
                if ((*this)[name]==0)
                  erase(name);
                while (isspace(*j)) ++j;
                b=j; // update to next unit name
              }
            else
              ++j;
          }
        if (b==i.c_str()) // we haven't found any exponents
          emplace(i,coef);
        coef*=-1;
      }
  }


}
