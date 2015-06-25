<h1>CS60 DartSync Project</h1>
* Binjie Li
* Jamie Yang
* Ben Ribovich
* Frederick Ho

<h2> Dev notes </h2>
  (Make requests to people working on a related component of the project here.)
  Note format: [mm/dd] [name]: [point requiring collaboration] [summary of what's needed]
  
	5/21 fho: (peer to tracker) Need a brief delay between establishing connection between peer and tracker and 
	sending out the REGISTER pkt (See 3.2.1 Protocol, P2T point on design doc) to make sure that the tracker's
	handShakeMonitor() thread doesn't miss it. 

	5/28 binjie: finish peer to tracker communication, still need consider many corner cases.
	=>block monitor
	=>sync method for folders
	=>use addPeerToTable and removeAllPeerFromTable Instad Of ..ToList and ..FromList!!

	5/29 binjie: finish example system!!!!!
	here is the instruction how it works.
	1. upload all the files on server
	2. use gcc command in the compileCommand file in the peer folder to compile peer.
	3. generate two files: peer and peer2. copy them into two different folders.
	4. for example, copy peer into ~/test/ and copy peer2 into ~/test2/
	5. run tracker on another server. and run two peers in differet folders in different servers.
	6. run like: ./peer ./ and ./peer2 ./
	7. then u can see the system works.
	8. right now im using touch and rm command to assume we can download a file instantly!!! but it can test our system without the part of p2p transfer.

	5/30 fho: For the report, there is a Design Summary section for which everyone should write a little bit about the design and control flow of their portion of the project. 
