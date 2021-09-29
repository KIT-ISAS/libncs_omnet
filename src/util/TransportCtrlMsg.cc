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

#include "TransportCtrlMsg.h"

Register_Class (TransportDataInfo);

void TransportDataInfo::copy(const TransportDataInfo& other) {
    if (networkOptions) {
        delete networkOptions;

        networkOptions = nullptr;
    }

    if (other.networkOptions) {
        this->networkOptions = other.networkOptions->dup();
    }
}

TransportDataInfo::TransportDataInfo(const char *name, int kind) :
        TransportDataInfo_Base(name, kind) {
    networkOptions = nullptr;
}

TransportDataInfo::TransportDataInfo(const TransportDataInfo& other) :
        TransportDataInfo_Base(other) {
    copy(other);
}

TransportDataInfo::~TransportDataInfo() {
    if (this->networkOptions) {
        delete this->networkOptions;
    }
}

TransportDataInfo& TransportDataInfo::operator=(const TransportDataInfo& other) {
    if (this == &other)
        return *this;

    TransportDataInfo_Base::operator=(other);
    copy(other);

    return *this;
}

TransportDataInfo * TransportDataInfo::dup() const {
    return new TransportDataInfo(*this);
}

const inet::NetworkOptionsPtr& TransportDataInfo::getNetworkOptions() const {
    return networkOptions;
}

void TransportDataInfo::setNetworkOptions(const inet::NetworkOptionsPtr& networkOptions) {
    if (this->networkOptions) {
        delete this->networkOptions;
    }

    this->networkOptions = networkOptions;
}

inet::NetworkOptionsPtr TransportDataInfo::replaceNetworkOptions(const inet::NetworkOptionsPtr networkOptions) {
    inet::NetworkOptionsPtr result = this->networkOptions;

    this->networkOptions = networkOptions;

    return result;
}

