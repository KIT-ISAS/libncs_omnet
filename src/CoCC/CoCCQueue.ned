//
// Copyright (C) 2012 Opensim Ltd.
// Author: Tamas Borbely
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

package libncs_omnet.CoCC;

import inet.common.queue.DropTailQueue;
import inet.common.queue.IOutputQueue;
import inet.common.queue.PriorityScheduler;
import inet.common.queue.Sink;
import inet.common.queue.WRRScheduler;
import inet.networklayer.diffserv.BehaviorAggregateClassifier;


//
// CoCC queue to prioritize monitoring feedback
//
module CoCCQueue like IOutputQueue
{
    parameters:
        string interfaceTableModule;
        *.interfaceTableModule = default(absPath(interfaceTableModule));
        @display("i=block/subqueue");
    gates:
        input in;
        output out;
    submodules:
        classifier: BehaviorAggregateClassifier {
            dscps = "EF CS7 CS1"; // EF, Prio, LBE
            @display("p=41,183");
        }
        ctrlEF: DropTailQueue {
            frameCapacity = default(2); // keep low, for low delay and jitter
            @display("p=194,28");
        }
        ctrlPriority: DropTailQueue {
            @display("p=194,105");
        }
        BE: DropTailQueue {
            @display("p=194,183");
        }
        ctrlLBE: DropTailQueue {
            @display("p=194,266");
        }
        // scheduler: PriorityScheduler {
        scheduler: WRRScheduler {
            weights = default("10000 1000 10 1"); // EF, Prio, BE, LBE
            @display("p=359,183");
        }

    connections:
        in --> classifier.in;
        scheduler.out --> out;

        classifier.outs++ --> ctrlEF.in;
        classifier.outs++ --> ctrlPriority.in;
        classifier.defaultOut --> BE.in;
        classifier.outs++ --> ctrlLBE.in;
        
        ctrlEF.out --> scheduler.in++;
        ctrlPriority.out --> scheduler.in++;
        BE.out --> scheduler.in++;
        ctrlLBE.out --> scheduler.in++;
}

