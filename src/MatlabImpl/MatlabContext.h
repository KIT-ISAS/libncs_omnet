//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#ifndef MATLABIMPL_MATLABCONTEXT_H_
#define MATLABIMPL_MATLABCONTEXT_H_

#include <omnetpp.h>
#include <set>

using namespace omnetpp;

class MatlabContext: public cObject {
  public:

    class MatlabToken {
        friend class MatlabContext;
      public:
        MatlabToken();

      private:
        MatlabToken(const bool generate);
        bool isValid() const;

        long id;
        static long nextTokenId;
    };

    static MatlabToken requestToken();
    static void returnToken(const MatlabToken token);

  private:
    static void initializeMatlab();
    static void deinitializeMatlab();

  private:
    static bool matlabInitialized;
    static std::set<long> activeTokens;
};

#endif /* MATLABIMPL_MATLABCONTEXT_H_ */
