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

#ifndef __LIBNCS_OMNET_ORACLECCCOORDINATOR_H_
#define __LIBNCS_OMNET_ORACLECCCOORDINATOR_H_

#include <omnetpp.h>

#include <memory>

#include <CoCC/CoCCUDPTransport.h>

#include "OracleCCSerumHandler.h"
#include "OracleCCUDPTransport.h"

using namespace omnetpp;
using namespace inet;

// forward declarations to break loops
class OracleCCSerumHandler;
class OracleCCUDPTransport;

/**
 * OracleCC
 *
 * Implements the same numerical approach as CoCC,
 * but with a centralized instance having access to
 * the knowledge of all control and CC instances.
 */
class OracleCCCoordinator : public cSimpleModule {

  public:
    static void addEdge(OracleCCSerumHandler * const handler, const InterfaceEntry * const ie, OracleCCUDPTransport * const transport, void * const transportHandle, const bool inbound);
    static void endPath(OracleCCUDPTransport * const transport, void * const transportHandle);

  protected:
    virtual void initialize();
    virtual void handleMessage(cMessage * const msg);

    /* Flow Structure */

    struct FlowHop {
        std::shared_ptr<FlowHop> prev, next; // may be NULL for first/last link in path
        const InterfaceEntry * inbound = nullptr; // same applies here
        const InterfaceEntry * outbound = nullptr; // same applies here
        OracleCCSerumHandler * handler = nullptr;
    };

    struct Flow {
        OracleCCUDPTransport * transport = nullptr;
        void * transportHandle = nullptr;
        std::shared_ptr<FlowHop> firstHop;

        double flowTargetQM = -1;
    };

    /* Graph Structure */

    struct Router;

    struct Link {
        Router * from = nullptr; // may be NULL for first link in path
        Router * to = nullptr; // may be NULL for last link in path
        const InterfaceEntry * fromIE = nullptr; // same applies here
        const InterfaceEntry * toIE = nullptr; // same applies here
        std::vector<Flow*> flows;

        double linkTargetQM = 1;

        Flow* findFlow(OracleCCUDPTransport const * transport, void * const transportHandle);
    };

    struct Router {
        OracleCCSerumHandler * handler = nullptr;
        std::vector<Link*> outboundLinks;
        std::vector<Link*> inboundLinks;

        Link* findFlowLink(OracleCCUDPTransport const * transport, void * const transportHandle, const bool inbound);
    };

    class FinderVisitor : public cVisitor {
      public:
        virtual void visit(cObject *obj) override;

        OracleCCCoordinator* coord = nullptr;
    };

    std::vector<std::unique_ptr<Flow>> flows;
    std::vector<std::unique_ptr<Link>> links;
    std::vector<std::unique_ptr<Router>> routers;

    simtime_t lastQMUpdate = SIMTIME_ZERO;

    Flow* findOrAddFlow(OracleCCUDPTransport * const transport, void * const transportHandle);

    Router* graphFindOrAddRouter(OracleCCSerumHandler * const handler);
    Link* graphFindLink(const InterfaceEntry * const fromIE, const InterfaceEntry * const toIE);
    Link* graphAddLink(Router * const from, Router * const to, const InterfaceEntry * const fromIE, const InterfaceEntry * const toIE);

  public:
    double getFlowTargetQM(OracleCCUDPTransport * const transport, void * const transportHandle);

  protected:
    void computeOracle();
    void computeLink(Link * const l);
    void recomputePath(Flow * const f, Link * const l, const bool inbound);


    double targetUtilization;
    CoCCUDPTransport::CoexistenceMode coexistenceMode;
    double newtonPrecision;
    simtime_t minUpdateInterval;

  public:
    static OracleCCCoordinator* findCoordinator();
};

#endif
