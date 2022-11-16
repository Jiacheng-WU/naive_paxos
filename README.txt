Team Members:

1. Jiacheng Wu:    jcwu22@cs.washington.edu
2. Yuan-Mao Chueh: ymchueh@cs.washington.edu

Project Description:

    * Build Requirements:
        * Configuration:
            - The configuration files is hardcoded in role/config.cpp load_config function
            - It is also easy to support read json config file, which is not the important
            - We could support it before the demo!!
        * Tools and Libs
            - CMake : Ubuntu (snap install cmake), MacOS (brew install cmake)
            - Boost : Ubuntu (apt-get install libboost-all-dev) , MacOS (brew install boost)
            - Clang/GCC : should support C++ 20, LLVM/Clang 14.0 or higher is better!
        * Build Steps
            - mkdir build
            - cd build
            - cmake -DCMAKE_BUILD_TYPE=(Debug|Release) [-DCMAKE_CXX_COMPILER=clang++{>=14} -DCMAKE_CXX_COMPILER=clang{>=14}} ..
            - make
        * Output
            - paxos_server (The Paxos Server runs paxos instance)
            - paxos_client (Example Client to issue requests)

    * Run Specifications:
        * For Server
            - We should specify the server_id for the server
            - ./paxos_server {id} ../config.json
            - Server started with specific id will use the ip and port specified in config.cpp
        * For Client
            - ./paxos_client {port = 0} ../config.json ../test.command
            - If not specified testfile, then client will use interactive mode
            - We support 4 types of command
                - lock (object_id)
                - unlock (object_id)
                - wait (number) {that is pause for number milliseconds}
                - server (server_id) {force next command to send server_id}
            - Just run since it is just an example displayed to issue requests
        * For Recovery
            - ./paxos_server {id} recovery
            - Add "recovery" after command


    * Assumptions:
        * Simplicity Discovery:
            - The identity of the Paxos group members is hardcoded at the clients in the code or through a configuration file.
        * Clients will be almost well-behaved:
            - the client could fail after acquiring a lock.
            - as long as it would restart with same ip and port
            - then the client could unlock the previously locked item
            - The deadlock somehow can be handled by send LOCK_AGAIN
        * Not consider the atmost-once or atleast-once semantics
            - even though we have already embedded at client_once in COMMANDS
            - but due to time limits, just ignore it here

    * Bonus Points
        * Recovery:
            - Use a seperator log for acceptor and learner
            - Use flush rather than fdatasync to sync due to it is hard to obtain fd from fstream
            - Though I knew it is better to use fdatasync to ensure flush to disk instead of os
        * Resending Message:
            - Add timeout for the proposer
            - the proposer will try to resend prepare/accept if it doesn't receive majority message for a long time.
        * Deadlock:
            - We would like to return LOCK_AGAIN state to identify one client try lock again.

Implementation:
    * No Leader/Master
        - No Leader Election Procedure
        - Each server could serve client requests, submit it to paxos group and responds the request.
    * Three roles in a single entity
        - We have a Instance contains Proposer, Acceptor and Learner
        - Thus, Proposer sometimes could directly read the corresponding Learner information
        - Some Optimization:
            * Proposer I don't send PREPARE/ACCEPT if the Learner I already learned the current sequence is chosen
            * Acceptor I inform other Proposer J with INFORM message if the Learner I has learned chosen value
    * Asynchronous Message Event Mechanism
        - Use ASIO framework and callbacks to handle concurrent client requests
        - Each arrived Message would trigger a specific event to propose
        - Sometimes need lock to maintain the critical section
            - Say if the instance would have two messages arrive at same time, and may have conflicts to process
            - But if io_context is single_thread, it may be not necessary to hold a lock.
    * Request resubmit Mechanism
        - Each client requests would be assigned to Instance with Sequence SEQ
        - But Paxos may finally not choose the client requests as value, (Due to other higher proposal number)
        - Then how to tackle especially in Asynchronous manner?
            - We keep the relation between SEQ and submitted value (client requests/commands) in unordered_map
            - After we could execute the value for current SEQ
            - We will first retrieve the original value of this SEQ from the above relation
            - And compare them if they are totally same
                - if no , it means the submitted value is not chosen for current SEQ, we resubmit it
                - if yes, then we could simply dropped the relation (SEQ->value) and respond to the client
    * Command Execution
        - How to Know when a command is chosen and when the command could be executed.
            - Additional INFORM message and timeout mechanism to ensure correctness and efficiency.
            - When the Learner received majority accepted message on the latest proposal number
            - Then this Learner knows this PAXOS instance achieved consensus, it just sends INFORM message
            - When other learner received the INFORM, it also knew we achieved consensus
            - Next, they just put the value (commands) into a min heap of server (less sequence is in the front)
            - Each server also maintain the current executed SEQ, and to find if there is next SEQ to execute
            - One command can be executed (and therefore respond to clients) only if the previous SEQ all executed
            - Say even SEQ 5 is achieved consensus, it still needs to wait SEQ 1,2,3,4 to achieve consensus and execute.
            - We just use a min-heap to maintain accepted but cannot execute values and also deduplicated
            - Then once a new value is put into the heap, we just detect whether the front SEQ is executed SEQ + 1
            - If no, we just ignore. Otherwise, we could execute the front SEQ, remove it, update executed SEQ and detect recursively
    * Concise Structure
        - Only use 10 * sizeof(std::uint32_t) for each message, which is short and faster
            - Use enum MessageType and enum OperationType to distinguish
            - Use same structure for both client and server message
            - But reuse some fields in different situations.
        - Only maintain necessary information on each Proposer, Acceptor, Learner
            - If some information is common, then put these information into the Instance or Server
            - Proposer/Acceptor/Learner just keep a necessary pointer to Instance (and Server)


Some Discussions:

    Suppose we have proposer P1, P2, P3
    Suppose we have acceptor A1, A2, A3

    * The Consensus is that once > half of acceptors agreed on a number, then they have consensus.
        - More Specific, > half of acceptors has accepted the same proposal number (not the value)
        - In fact, we should observe the PSM from client sides, once PSM return done
        - the client side could only get the same value for current instance after that time.
            - even from different nodes
            - somehow could block if current instance on this node is not learned


    * Proposer P need to identify which PREPARE proposal number the Acceptor (PROMISE) replied to !!
        Image an extreme case:
        - P first send PREPARE to A1, A2, A3 with proposal number n = 3;
        - A1, A2, A3 recv PREPARE and PROMISE not to respond other proposal less than n = 3;
            - But these three message arrives late until the next propose action.
        - P find it is timeout to receive PROMISE, and re-propose PREPARE with n = 6;
            - But these three message just lost and A1, A2, A3 didn't receive it.
        - P then receive message in the previous round with n = 3;
            - If we do not identify the proposal number,
              then P would think A1, A2, A3 promised not to respond other proposal with n = 6,
              even though now A1, A2, A3 never receive this PREPARE request with n = 6!!!
        - Then, it violates the Paxos algorithm, and may lead the acceptor to accept two different values!!
        Therefore, Propose P should identify which PREPARE numbered n the PROMISE message replied to.
        - If we received the out-of-dated PROMISE, that is the replied n < current proposal number
        - we should omit this PROMISE, and do nothing since new proposal is already start
        - That's also the reason for additional_field_1 in struct Message;

    * Learner L should distinguish the different proposal number (but with same value) of ACCEPTED message
        - Note that consensus is achieved when a majority of Acceptors accept the same identifier number
        - (rather than the same value). [From Wikipedia]
        - I also have a counterexample if the learner just accepted > half acceptors only agreed on same value

    * It is legal to propose no-ops even if some acceptors may already previous accept some values when instance recovery
        - Since the instance will re-execute unlearned commands and the whole procedure of basic paxos
        - Then even if it proposes no-ops in Phase 1 (actually Phase 1 even do not need proposal value)
        - It will still receive other accepted value as the value of current instances
        - Only if the majority of acceptors which received the PREPARE and replied PROMISE with no value
        - Could the new instance decided the current proposal value with NO-OPS!!
        - Thus, it is no wrong to propose NO-OPS when recovery for never learnt instances.

    * It is also legal to even not propose no-ops or just use already existed sequence after recovery
        - Just similar to the above cases since just replacing the no-ops with current values submitted by clients
        - If we have a mechanism to support re-submit client requests when
            - the commands for current instance is not the client submitted!


    * We need to persist what state on paxos nodes
        - The acceptor at least should preserve the three items itself of each instance for future recovery
        - Otherwise the acceptor may reply two different proposer with inconsistent states.

    * The proposer can send Accept Message to all acceptors even if it only received a majority of promise
        - That is also the reason why multi-paxos works
        - In fact, two majority of groups must at least has one same value
        - Even we could image an extreme case, that is
            - Propose P1 first receives promise from A1, A2
            - And the P1 just sends accept to A2, A3. It is still works
            - But for efficiency and reasonable,
            - it is better to update highest_prepare_proposal_number as well

    * In multi-paxos, the leader could skip the PREPARE in Phase 1, but what proposal number should it propose!
        - It must be the least proposal should have
        - Otherwise may cause problems, Image this case
        - All proposers finish Instance 99 and continue to 100.
        - However, the leader P1 is somehow delayed and cannot contact P3,
        - then P3 try to elect as a leader and start with proposal_number = 1
        - After P3 have finished the whole procedure and learner even learned the value from > 1/2 acceptor
        - P1 is awake and simply execute the ACCEPT phase with its own proposal value with higher number = 3
        - Then all other acceptors simply accepts since it is a newer one which broken the paxos
        - Therefore, we need the default number for the Leader is the less one
        - and should less than or other possible proposal numbers in PREPARE
        - In such cases, the error will be discovered and then after P1 is rejected,
        - It could arise a new round of paxos or just learned a new leader!!
        - We should also mention here that if the leader find itself timeout,
        - The leader should also do the whole paxos (including PHASE 1) to ensure consensus!!

    * The Acceptor's PROMISE (on_prepare) and ACCEPTED (on_accept) should be atomic, that is,
        - we cannot separate the two procedures in PROMISE
            - WP writing the highest_prepare_proposal_number and
            - RA reading the highest_accepted_proposal_number/value
        - we also cannot separate the two procedures in ACCEPT
            - RP reading the highest_prepare_proposal_number and
            - WA writing the highest_accepted_proposal_number/value
        - Otherwise, here is the case:
            - P2.proposal_number in PREPARE > P1.proposal_number in ACCEPT
            - P1 send ACCEPT  request to A1 and A1 execute RP in ACCEPTED, which pass the check
            - P2 send PREPARE request to A1 and A1 execute WP in PROMISE
            - A1 in PROMISE execute RA and then A1 in ACCEPT execute WA
            - RP (for P1), WP (for P2), RA (for P2), WA( for P1)
            - The above case is not tolerable since P1 succeed write Value and P2 read Undecided Value
                - Say meanwhile if A2 already accepted P1, then now P1's proposal is accepted by majority
                - and P2 only send PREPARE to A1 and A3, then P2 is also pass the PHASE 1 with no specified value
                - then P2 could specify its own value different A1 and send ACCEPT to A1/A3 which also accepted
                - Thus, in this cases, the majority of acceptors change their opinions
                - even after > half of acceptors has agreed a different value before.
                - which violates the consensus.
            - In fact, any sequential execution won't cause above situations
                - if A1 accepted for P1 is before A1 promise to P2, then P2 will read the accepted value
                - if A1 try to accept P1 is after A1 promise to P2, then P1 will be rejected by A1

    * Learner's Behaviour, it is not necessary the history accepted cases
        - Learner only needs to care about the largest proposal number of accepted messages
        - Though it may somehow sacrifice a little of liveliness properties, but is easy to implement
        - In fact, the instance can be stopped if
            - there is a majority of acceptor accepted a proposal number
            - even meantime some proposer may propose new number
        - In our simplified case, the learner may only maintain this new number's accepted infos
        - which may influence liveness somehow.
        - But we do an optimization by delaying the updating infos until received new ACCEPTED
        - rather than the PREPARE!

    * We only need inform once or even not to inform
        - In fact, if current sequence is already chosen, but the Server didn't knew
        - It would choose a new proposal number to PREPARE, and then it finally will get the value
        - So it is no even necessary to inform others, but we just optimized for it
            - use extra one round to notify all the consensus value.
            - avoid the duplicated PREPARE/ACCEPT by making acceptor reply INFORM.

    * Leader Election in Paxos is really a complex problems
        - Some implementation would like to use redirect message to tell clients to resent to server
        - It would cause the specific leader handle too many requests
        - Even though we could apply optimizations to avoid PREPARE and ACCEPT message
        - However, in this case, we should tackle with complex situations where
            - What if the leader failed?
            - What if two leader is electing?
            - Is it need to log leader election on Paxos Replicated Log?
                - If necessary, then how to decide the specific sequence for such election message
                - We will find at that time, we still need the basic to process.
        - In fact, Raft paper is better to read and just figure out the above problems by its protocol.