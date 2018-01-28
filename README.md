# libncs_omnet

OMNeT++ modules which either interface the MATLAB-API and translate the messages into the OMNeT++/INET domain (or the other way  around), or which support you in building your own Cyber-Physical-Networks.

``NcsContext`` provides a mechanism based on subclassing to push configuration parameters to the MATLAB domain, e.g. to run parametric studies.
The subclassed module ``CoCpnNcsContext`` might give you some insights on how to add your own parameters.
