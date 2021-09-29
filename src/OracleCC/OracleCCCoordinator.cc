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

#include "OracleCCCoordinator.h"

Define_Module(OracleCCCoordinator);

#define NEWTON_EPSILON 1E-8
#define NEWTON_ITER_LIMIT 10


void OracleCCCoordinator::initialize() {
    targetUtilization = par("targetUtilization").doubleValue();
    coexistenceMode = static_cast<CoCCUDPTransport::CoexistenceMode>(par("coexistenceMode").intValue());
    newtonPrecision = par("newtonPrecision").doubleValue();
    minUpdateInterval = par("minUpdateInterval").doubleValue();
}

void OracleCCCoordinator::handleMessage(cMessage * const msg) {
    ASSERT(false);
}

/*
 * Konzept Pfadaufzeichnung:
 * Für jeden Hop des Connect-Pakets wird Outbound-Interface+SerumHandler und Flow (über Staukontrollinstanz) gemeldet
 * Empfangende Staukontrollinstanz bildet Abschluss über endPath
 */
void OracleCCCoordinator::addEdge(OracleCCSerumHandler * const handler, const InterfaceEntry * const ie, OracleCCUDPTransport * const transport, void * const transportHandle, const bool inbound) {
    EV_STATICCONTEXT;

    EV_DEBUG << "add edge: " << (inbound ? "inbound" : "outbound") << " event. " << endl;
    EV_DEBUG << "handler: " << handler->getFullPath() << endl;
    EV_DEBUG << "interface: " << ie->getFullPath() << endl;
    EV_DEBUG << "transport: " << transport->getFullPath() << endl;

    OracleCCCoordinator* coord = findCoordinator();

    ASSERT(coord);

    Flow * const flow = coord->findOrAddFlow(transport, transportHandle);

    std::shared_ptr<FlowHop> tail = flow->firstHop;

    // search tail
    while (tail) {
        if (tail->inbound == ie || tail->outbound == ie) {
            EV_DEBUG << "duplicate event detected, discarding" << endl;

            return;
        }

        if (tail->next){
            tail = tail->next;
        } else {
            break;
        }
    };

    EV_DEBUG << "flow: " << flow << " with tail: " << tail.get() << endl;

    if (inbound) {
        auto newHop = std::make_shared<FlowHop>();

        newHop->prev = tail;
        newHop->inbound = ie;
        newHop->handler = handler;

        if (tail) {
            tail->next = newHop;
        } else {
            ASSERT(!flow->firstHop);

            flow->firstHop = newHop;
        }

        EV_DEBUG << "adding new hop: " << newHop.get() << endl;

        // add new flow edge to graph
        const InterfaceEntry * fromIE = nullptr;
        OracleCCSerumHandler * fromHandler = nullptr;
        Link * link = nullptr;

        if (tail) {
            fromIE = tail->outbound;
            fromHandler = tail->handler;
        }

        // try to find link if it is fully specified.
        // otherwise, this is the first hop, so fromIE is undefined
        if (fromIE) {
            link = coord->graphFindLink(fromIE, ie);

            EV_DEBUG << "link lookup with fromIE=" << fromIE->getFullPath() << endl;
            EV_DEBUG << "matched: " << link << endl;
        }

        if (!link) {
            // must create link first
            Router * fromRouter = nullptr;
            Router * const toRouter = coord->graphFindOrAddRouter(handler);

            if (fromHandler) {
                fromRouter = coord->graphFindOrAddRouter(fromHandler);
            }

            link = coord->graphAddLink(fromRouter, toRouter, fromIE, ie);

            EV_DEBUG << "added new link to graph from router " << fromRouter << " to router " << toRouter << endl;
            EV_DEBUG << "new link: " << link<< endl;
        }

        ASSERT(!link->findFlow(transport, transportHandle));

        link->flows.push_back(flow);

        EV_DEBUG << "added flow to link" << endl;
    } else {
        ASSERT(tail);

        tail->outbound = ie;

        EV_DEBUG << "set outbound interface for tail" << endl;
    }
}

void OracleCCCoordinator::endPath(OracleCCUDPTransport * const transport, void * const transportHandle) {
    EV_STATICCONTEXT;

    EV_DEBUG << "end path for transport: " << transport->getFullPath() << endl;

    OracleCCCoordinator* coord = findCoordinator();

    ASSERT(coord);

    Flow * const flow = coord->findOrAddFlow(transport, transportHandle);

    std::shared_ptr<FlowHop> tail = flow->firstHop;

    // search tail
    while (tail && tail->next) {
        tail = tail->next;
    }

    // add final link to graph
    const InterfaceEntry * fromIE = tail->outbound;
    OracleCCSerumHandler * fromHandler = tail->handler;

    // link can not exist yet (goes to end system), so prepare to create one
    Router * const fromRouter = coord->graphFindOrAddRouter(fromHandler);
    Link * const link = coord->graphAddLink(fromRouter, nullptr, fromIE, nullptr);

    link->flows.push_back(flow);
}

OracleCCCoordinator::Flow* OracleCCCoordinator::Link::findFlow(OracleCCUDPTransport const * transport, void * const transportHandle) {
    ASSERT(transport);

    for (auto it = flows.begin(); it != flows.end(); it++) {
        if ((*it)->transport == transport && (*it)->transportHandle == transportHandle) {
            return *it;
        }
    }

    return nullptr;
}

OracleCCCoordinator::Link* OracleCCCoordinator::Router::findFlowLink(OracleCCUDPTransport const * transport, void * const transportHandle, const bool inbound) {
    std::vector<Link*> &linkList = inbound ? inboundLinks : outboundLinks;

    for (auto it = linkList.begin(); it != linkList.end(); it++) {
        if((*it)->findFlow(transport, transportHandle)) {
            return *it;
        }
    }

    return nullptr;
}

OracleCCCoordinator::Flow* OracleCCCoordinator::findOrAddFlow(OracleCCUDPTransport * const transport, void * const transportHandle) {
    ASSERT(transport);

    for (auto it = flows.begin(); it != flows.end(); it++) {
        if ((*it)->transport == transport && (*it)->transportHandle == transportHandle) {
            return it->get();
        }
    }

    flows.push_back(std::unique_ptr<Flow>(new Flow()));

    Flow * const result = flows.back().get();

    result->transport = transport;
    result->transportHandle = transportHandle;

    return result;
}

double OracleCCCoordinator::getFlowTargetQM(OracleCCUDPTransport * const transport, void * const transportHandle) {
    computeOracle();

    Flow * const f = findOrAddFlow(transport, transportHandle);

    if (!f) {
        return 0;
    }

    return std::max(f->flowTargetQM, 0.0);
}

OracleCCCoordinator::Router* OracleCCCoordinator::graphFindOrAddRouter(OracleCCSerumHandler * const handler) {
    ASSERT(handler);

    for (auto it = routers.begin(); it != routers.end(); it++) {
        if ((*it)->handler == handler) {
            return it->get();
        }
    }

    routers.push_back(std::unique_ptr<Router>(new Router()));

    Router * const result = routers.back().get();

    result->handler = handler;

    return result;
}

OracleCCCoordinator::Link* OracleCCCoordinator::graphFindLink(const InterfaceEntry * const fromIE, const InterfaceEntry * const toIE) {
    // IEs may be NULL for Link instances but this method may not be used to find them
    // NULL represents an not further identified end system, thus those links must be identified by interface + transport
    ASSERT(fromIE);
    ASSERT(toIE);

    for (auto it = links.begin(); it != links.end(); it++) {
        if ((*it)->fromIE == fromIE && (*it)->toIE == toIE) {
            return it->get();
        }
    }

    return nullptr;
}

OracleCCCoordinator::Link* OracleCCCoordinator::graphAddLink(Router * const from, Router * const to, const InterfaceEntry * const fromIE, const InterfaceEntry * const toIE) {
    // one of them may be NULL, if not, the link must not exist yet
    ASSERT(fromIE == nullptr || toIE == nullptr || graphFindLink(fromIE, toIE) == nullptr);

    links.push_back(std::unique_ptr<Link>(new Link()));

    Link * const result = links.back().get();

    result->from = from;
    result->to = to;
    result->fromIE = fromIE;
    result->toIE = toIE;

    if (from) {
        from->outboundLinks.push_back(result);
    }
    if (to) {
        to->inboundLinks.push_back(result);
    }

    return result;
}

OracleCCCoordinator* OracleCCCoordinator::findCoordinator() {
    static OracleCCCoordinator * coord = nullptr;
    static cSimulation * sim = nullptr;

    if (!coord || cSimulation::getActiveSimulation() != sim) {
        OracleCCCoordinator::FinderVisitor v;

        sim = cSimulation::getActiveSimulation();

        sim->forEachChild(&v);

        coord = v.coord;
    }

    return coord;
}

void OracleCCCoordinator::FinderVisitor::visit(cObject *obj) {
    const std::string className("OracleCCCoordinator");

    if (!coord && obj) {
        if (className.compare(obj->getClassName()) == 0) {
            coord = dynamic_cast<OracleCCCoordinator*>(obj);
        } else {
            obj->forEachChild(this);
        }
    }
};

void OracleCCCoordinator::computeOracle() {
    if (lastQMUpdate + minUpdateInterval >= simTime()) {
        return; // already did an update at this point in time
    }

    // the oracle follows a waterfilling paradigm
    // it computes the QM for each individual link
    // the link with the smallest QM is the bottleneck of all affected flows
    // thus: fix those flows to bottleneck QM and recompute the QM of all their other links
    // repeat until all links have been computed

    std::vector<Link*> openLinks;

    // reset flows
    for (auto &f : flows) {
        f->flowTargetQM = -1;
    }

    // compute all links
    for (auto &l : links) {
        computeLink(l.get());

        openLinks.push_back(l.get());
    }

    while (openLinks.size() > 0) {
        // search bottleneck
        Link* bottleneck = openLinks.front();

        for (auto l : openLinks) {
            if (l->linkTargetQM < bottleneck->linkTargetQM) {
                bottleneck = l;
            }
        }

        EV_DEBUG << "bottleneck QM " << bottleneck->linkTargetQM << endl;

        // set flow QM to bottleneck QM and recompute affected links
        for (auto &f : bottleneck->flows) {
            // only if bottleneck has not already been found
            if (f->flowTargetQM < 0) {
                EV_DEBUG << "bottleneck found for flow " << f->transport->getFullPath() << endl;

                f->flowTargetQM = bottleneck->linkTargetQM;

                recomputePath(f, bottleneck, true);
                recomputePath(f, bottleneck, false);
            }
        }

        // remove bottleneck from open link list
        auto it = openLinks.begin();

        while (*it != bottleneck) {
            it++;
        }

        openLinks.erase(it);
    }

    lastQMUpdate = simTime();
}

void OracleCCCoordinator::computeLink(Link * const l) {
    if (l->from) {
        EV_DEBUG << "computing link " << l->from->handler->getParentModule()->getFullPath() << "-> ";
        if (l->to) {
            EV_DEBUG << l->to->handler->getParentModule()->getFullPath() << endl;
        } else {
            EV_DEBUG << "host" << endl;
        }

        // determine maximum target rate considering desired upper bound
        auto stats = l->from->handler->getMonitoringStatistics(l->fromIE);

        // determine non-control rate
        double beRate = 0;

        for (auto q : stats.queues) {
            if (!opp_strcmp("BE", q.name)) {
                beRate = std::max(q.avgEgressRate.filtered(), q.avgIngressRate.filtered());
                break;
            }
        }

        double rate = stats.lineRate * targetUtilization;

        // determine rate required for qmDesired as well as min and max rate
        double qmDesiredRate_sum = 0;
        double rateMax = 0;
        double rateMin = 0;

        for (auto f : l->flows) {
            if (f->flowTargetQM < 0) {
                qmDesiredRate_sum += f->transport->getQMDesiredRate(f->transportHandle);

                EV_DEBUG << "(probably) bottleneck flow: " << f->transport->getFullPath() << endl;
            } else {
                // flows which are constrained by some other link do not desire more rate here
                qmDesiredRate_sum += std::min(f->transport->getQMDesiredRate(f->transportHandle),
                                              f->transport->getRateForQM(f->transportHandle, f->flowTargetQM));

                EV_DEBUG << "non-bottleneck flow: " << f->transport->getFullPath() << "target QM: " << f->flowTargetQM << endl;
            }

            rateMin += f->transport->getRateForQM(f->transportHandle, 0);
            rateMax += f->transport->getRateForQM(f->transportHandle, 1);
        }

        // determine rate available for control
        rate = CoCCUDPTransport::coccComputeCoexistenceRate(coexistenceMode, rate, qmDesiredRate_sum, beRate);

        // handle corner cases where we clearly do not have a bottleneck or have an overutilization situation
        if (rate >= rateMax) {
            l->linkTargetQM = 1;

            return;
        }
        if (rate <= rateMin) {
            l->linkTargetQM = 0;

            return;
        }

        // solve using Newton's method

        int i = 0;
        double qmTarget = 0.5;

        do {
            ICoCCTranslator::CoCCLinearization linSum = { 0, 0 };

            for (auto f : l->flows) {
                ICoCCTranslator::CoCCLinearization lin = { 0, 0 };

                if (f->flowTargetQM < 0) {
                    lin = f->transport->getLinearizationForQM(f->transportHandle, qmTarget);
                } else {
                    // this link is not a bottleneck to this flow
                    // flows which already have their target QM assigned can not increase their sending rate further
                    lin.b = f->transport->getRateForQM(f->transportHandle, f->flowTargetQM);
                }

                linSum.m += lin.m;
                linSum.b += lin.b;
            }

            // detect if m is too small and shift y to move away from plateau
            if (linSum.m < NEWTON_EPSILON) {
                EV_DEBUG << "newton iteration hit plateau at qmTarget=" << qmTarget << endl;

                const double qmRate = linSum.m * qmTarget + linSum.b;

                if (qmRate > rate) {
                    qmTarget *= 0.75;
                } else {
                    qmTarget += (1 - qmTarget) * 0.25;
                }

                EV_DEBUG << "continuing at qmTarget=" << qmTarget << endl;

                continue;
            }

            qmTarget = CoCCUDPTransport::coccComputeLinkTargetQM(rate, linSum.m, linSum.b, true);

            double realRateSum = 0;

            for (auto f : l->flows) {
                realRateSum += f->transport->getRateForQM(f->transportHandle, qmTarget);
            }

            if (std::abs(realRateSum - rate) < newtonPrecision) {
                break;
            }
        } while (i++ < NEWTON_ITER_LIMIT);

        l->linkTargetQM = qmTarget;

        EV_DEBUG << "target QM: " << qmTarget << endl;
    } else {
        // this is the first link on the path. No CC applies here
        l->linkTargetQM = 1;
    }
}

void OracleCCCoordinator::recomputePath(Flow * const f, Link * const l, const bool inbound) {
    Link * currentLink = l;
    Router * r = inbound ? l->from : l->to;

    while (r) {
        currentLink = r->findFlowLink(f->transport, f->transportHandle, inbound);

        computeLink(currentLink);

        r = inbound ? currentLink->from : currentLink->to;
    }
}
