Team Members:

1. Jiacheng Wu:    jcwu22@cs.washington.edu
2. Yuan-Mao Chueh: ymchueh@cs.washington.edu

Project Description:

    * Build Requirements:
        * Configuration:
            - The configuration files are hardcoded in role/config.cpp load_config function
            - It is also easy to support reading JSON config files, which is not the important
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
            - paxos_server (The Paxos Server runs Paxos instance)
            - paxos_client (Example Client to issue requests)

    * Run Specifications:
        * For Server
            - We should specify the server_id for the server
            - ./paxos_server {id} ../config.json
            - Server started with specific id will use the ip and port specified in config.cpp
        * For Client
            - ./paxos_client {port = 0} ../config.json ../test.command
            - If not specified test file, then the client will use interactive mode
            - We support 4 types of command
                - lock (object_id)
                - unlock (object_id)
                - wait (number) {that is pause for number milliseconds}
                - server (server_id) {force next command to send server_id}
            - Just run since it is just an example displayed to issue requests
        * For Recovery
            - ./paxos_server {id} recovery
            - Add "recovery" after the command


    * Assumptions:
        * Simplicity Discovery:
            - The identity of the Paxos group members is hard coded at the clients in the code or through a configuration file.
        * Clients will be almost well-behaved:
            - the client could fail after acquiring a lock.
            - as long as it would restart with the same ip and port
            - then the client could unlock the previously locked item
            - The deadlock somehow can be handled by sending LOCK_AGAIN
        * Not consider the atmost-once or atleast-once semantics
            - even though we have already embedded at client_once in COMMANDS
            - but due to time limits, just ignore it here

    * Bonus Points
        * Recovery:
            - Use a separate log for the acceptor and learner
            - Use flush rather than fdatasync to sync due to it is hard to obtain fd from fstream
            - Though I knew it is better to use fdatasync to ensure flush to disk instead of os
        * Resending Message:
            - Add timeout for the proposer
            - the proposer will try to resend prepare/accept if it doesn't receive the majority message for a long time.
        * Deadlock:
            - We would like to return LOCK_AGAIN state to identify one client and try lock again.

Implementation:
    * No Leader/Master
        - No Leader Election Procedure
        - Each server could serve client requests, submit them to Paxos group, and responds to the request.
    * Three roles in a single entity
        - We have an Instance containing Proposer, Acceptor, and Learner
        - Thus, Proposer sometimes could directly read the corresponding Learner information
        - Some Optimization:
            * Proposer I don't send PREPARE/ACCEPT if the Learner I already learned the current sequence is chosen
            * Acceptor I inform other Proposer J with INFORM message if the Learner I has learned the chosen value
    * Asynchronous Message Event Mechanism
        - Use ASIO framework and callbacks to handle concurrent client requests
        - Each arrived Message would trigger a specific event to propose
        - Sometimes need a lock to maintain the critical section
            - Say if the instance would have two messages arrive at the same time, and may have conflicts to process
            - But if io_context is single_thread, it may be not necessary to hold a lock.
    * Request resubmit Mechanism
        - Each client request would be assigned to an Instance with Sequence SEQ
        - But Paxos may finally not choose the client requests as value, (Due to other higher proposal numbers)
        - Then how to tackle especially in an Asynchronous manner?
            - We keep the relation between SEQ and submitted value (client requests/commands) in unordered_map
            - After we could execute the value for the current SEQ
            - We will first retrieve the original value of this SEQ from the above relation
            - And compare them to if they are totally same
                - if no, it means the submitted value is not chosen for the current SEQ, we resubmit it
                - if yes, then we could simply drop the relation (SEQ->value) and respond to the client
    * Command Execution
        - How to Know when a command is chosen and when the command could be executed.
            - Additional INFORM message and timeout mechanism to ensure correctness and efficiency.
            - When the Learner received majority accepted message on the latest proposal number
            - Then this Learner knows this PAXOS instance achieved consensus, it just sends INFORM message
            - When other learners received the INFORM, they also knew we achieved a consensus
            - Next, they just put the value (commands) into a min heap of the server (less sequence is in the front)
            - Each server also maintains the current executed SEQ, and finds if there is a next SEQ to execute
            - One command can be executed (and therefore respond to clients) only if the previous SEQ all executed
            - Say even if SEQ 5 is achieved consensus, it still needs to wait for SEQ 1,2,3,4 to achieve consensus and execute.
            - We just use a min-heap to maintain accepted but cannot execute values and also deduplicated
            - Then once a new value is put into the heap, we just detect whether the front SEQ is executed SEQ + 1
            - If not, we just ignore it. Otherwise, we could execute the front SEQ, remove it, update executed SEQ, and detect recursively
    * Concise Structure
        - Only use 10 * sizeof(std::uint32_t) for each message, which is short and faster
            - Use enum MessageType and enum OperationType to distinguish
            - Use the same structure for both client and server message
            - But reuse some fields in different situations.
        - Only maintain necessary information on each Proposer, Acceptor, Learner
            - If some information is common, then put this information into the Instance or Server
            - Proposer/Acceptor/Learner just keep a necessary pointer to Instance (and Server)


Some Discussions:

    Suppose we have proposer P1, P2, P3
    Suppose we have acceptor A1, A2, A3

    * The Consensus is that once > half of acceptors agreed on a number, then they have a consensus.
        - More Specific, > half of the acceptors have accepted the same proposal number (not the value)
        - In fact, we should observe the PSM from the client side, once the PSM return done
        - the client side could only get the same value for the current instance after that time.
            - even from different nodes
            - somehow could block if the current instance on this node is not learned


    * Proposer P needs to identify which PREPARE proposal number the Acceptor (PROMISE) replied to !!
        Image an extreme case:
        - P first send PREPARE to A1, A2, A3 with proposal number n = 3;
        - A1, A2, A3 recv PREPARE and PROMISE not to respond to other proposals less than n = 3;
            - But these three messages arrive late until the next proposed action.
        - P finds it is a timeout to receive PROMISE, and re-propose PREPARE with n = 6;
            - But these three message just lost and A1, A2, A3 didn't receive it.
        - P then receives the message in the previous round with n = 3;
            - If we do not identify the proposal number,
              then P would think A1, A2, A3 promised not to respond other proposal with n = 6,
              even though now A1, A2, A3 never receive this PREPARE request with n = 6!!!
        - Then, it violates the Paxos algorithm, and may lead the acceptor to accept two different values!!
        Therefore, Propose P should identify which PREPARE numbered n the PROMISE message replied to.
        - If we received the out-of-dated PROMISE, that is the replied n < current proposal number
        - we should omit this PROMISE, and do nothing since the new proposal is already started
        - That's also the reason for additional_field_1 in struct Message;

    * Learner L should distinguish the different proposal numbers (but with the same value) of the ACCEPTED message
        - Note that consensus is achieved when a majority of Acceptors accept the same identifier number
        - (rather than the same value). [From Wikipedia]
        - I also have a counterexample if the learner just accepted > half of acceptors only agreed on the same value

    * It is legal to propose no-ops even if some acceptors may already previously accept some values when instance recovery
        - Since the instance will re-execute unlearned commands and the whole procedure of basic Paxos
        - Then even if it proposes no-ops in Phase 1 (actually Phase 1 even do not need proposal value)
        - It will still receive other accepted values as the value of current instances
        - Only if the majority of acceptors who received the PREPARE and replied PROMISE with no value
        - Could the new instance decide the current proposal value with NO-OPS!!
        - Thus, it is not wrong to propose NO-OPS when recovery for never learned instances.

    * It is also legal to even not propose no-ops or just use already existing sequences after recovery
        - Just similar to the above cases since just replacing the no-ops with current values submitted by clients
        - If we have a mechanism to support re-submit client requests when
            - the commands for the current instance is not the client submitted!


    * We need to persist in what state on Paxos nodes
        - The acceptor at least should preserve the three items of each instance for future recovery
        - Otherwise, the acceptor may reply to two different proposers with inconsistent states.

    * The proposer can send Accept Message to all acceptors even if it only received a majority of the promise
        - That is also the reason why multi-Paxos works
        - In fact, two majorities of groups must at least have one same value
        - Even we could imagine an extreme case, that is
            - Propose P1 first receives promise from A1, A2
            - And the P1 just sends accept to A2, A3. It still works
            - But for efficiency and reasonable,
            - it is better to update highest_prepare_proposal_number as well

    * In multi-paxos, the leader could skip the PREPARE in Phase 1, but what proposal number should it propose?
        - It must be the least proposal that should have
        - Otherwise may cause problems, Image this case
        - All proposers finish Instance 99 and continue to 100.
        - However, the leader P1 is somehow delayed and cannot contact P3,
        - then P3 tries to elect a leader and starts with proposal_number = 1
        - After P3 has finished the whole procedure and the learner even learned the value from > 1/2 acceptor
        - P1 is awake and simply executes the ACCEPT phase with its own proposal value with a higher number = 3
        - Then all other acceptors simply accept since it is a newer one which broke the Paxos
        - Therefore, we need the default number for the Leader is the less than one
        - and should be less than other possible proposal numbers in PREPARE
        - In such cases, the error will be discovered, and then after P1 is rejected,
        - It could arise a new round of Paxos or just learn a new leader!!
        - We should also mention here that if the leader finds itself timeout,
        - The leader should also do the whole Paxos (including PHASE 1) to ensure consensus!!

    * The Acceptor's PROMISE (on_prepare) and ACCEPTED (on_accept) should be atomic, that is,
        - we cannot separate the two procedures in PROMISE
            - WP writing the highest_prepare_proposal_number and
            - RA reading the highest_accepted_proposal_number/value
        - we also cannot separate the two procedures in ACCEPT
            - RP reading the highest_prepare_proposal_number and
            - WA writing the highest_accepted_proposal_number/value
        - Otherwise, here is the case:
            - P2.proposal_number in PREPARE > P1.proposal_number in ACCEPT
            - P1 sends ACCEPT  request to A1 and A1 executes RP in ACCEPTED, which passes the check
            - P2 sends PREPARE request to A1 and A1 executes WP in PROMISE
            - A1 in PROMISE execute RA and then A1 in ACCEPT execute WA
            - RP (for P1), WP (for P2), RA (for P2), WA( for P1)
            - The above case is not tolerable since P1 succeed write Value and P2 read Undecided Value
                - Say meanwhile if A2 already accepted P1, then now P1's proposal is accepted by majority
                - and P2 only send PREPARE to A1 and A3, then P2 is also pass the PHASE 1 with no specified value
                - then P2 could specify its own value different A1 and send ACCEPT to A1/A3 which also accepted
                - Thus, in this case, the majority of acceptors change their opinions
                - even after > half of the acceptors have agreed on a different value before.
                - which violates the consensus.
            - In fact, any sequential execution won't cause the above situations
                - if A1 accepted for P1 is before A1 promise to P2, then P2 will read the accepted value
                - if A1 try to accept P1 is after A1 promise to P2, then P1 will be rejected by A1

    * Learner's Behaviour, it is not necessary to the history of accepted cases
        - Learner only needs to care about the largest proposal number of accepted messages
        - Though it may somehow sacrifice a little of liveliness properties, but is easy to implement
        - In fact, the instance can be stopped if
            - there is a majority of acceptors accepted a proposal number
            - In even meantime some proposers may propose a new number
        - In our simplified case, the learner may only maintain this new number's accepted info
        - which may influence liveness somehow.
        - But we do an optimization by delaying the updating info until received new ACCEPTED
        - rather than the PREPARE!

    * We only need to inform once or even not inform
        - In fact, if the current sequence is already chosen, but the Server didn't know
        - It would choose a new proposal number to PREPARE, and then it finally will get the value
        - So it is not even necessary to inform others, but we just optimized for it
            - use an extra round to notify all the consensus values.
            - avoid the duplicated PREPARE/ACCEPT by making the acceptor reply INFORM.

    * Leader Election in Paxos is really a complex problem
        - Some implementations would like to use redirect messages to tell clients to resent to the server
        - It would cause the specific leader to handle too many requests
        - Even though we could apply optimizations to avoid PREPARE and ACCEPT message
        - However, in this case, we should tackle complex situations where
            - What if the leader failed?
            - What if two leaders are elected?
            - Is it needed to log leader election on Paxos Replicated Log?
                - If necessary, then how to decide the specific sequence for such an election message
                - We will find at that time, we still need the basic to process.
        - In fact, Raft paper is better to read and just figure out the above problems by its protocol.
