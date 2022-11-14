Team Members:

1. Jiacheng Wu:    jcwu22@cs.washington.edu
2. Yuan-Mao Chueh: ymchueh@cs.washington.edu

Project Description:

    



Some Discussions:

    Suppose we have proposer P1, P2, P3
    Suppoer we heve acceptor A1, A2, A3

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
        - P find it is timeout to receive PROMISE, and repropose PREPARE with n = 6;
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

    * In multi-paxos, the leader could skip the PREPARE in Phase 1, but what proposal number should it propose!
        - It must be the least proposal should have
        - Otherwise may cause problems, Image this case
        - All proposers finish Instance 99 and continue to 100.
        - However, the leader P1 is somehow delayed and cannot contact P3,
        - then P3 try to elect as a leader and start with proposal_number = 1
        - After P3 have finished the whole procedure and learner even learned the value from > 1/2 acceptor
        - P1 is awaken and simply execute the accept phase with its own proposal value with higher number = 3
        - Then all other acceptors simply accepts since it is a newer one which broken the paxos
        - Therefore, we needs the default number for the Leader is the less one
        - and should less than or other possible proposal numbers in PREPARE
        - In such cases, the error will be discovered and then after P1 is rejected,
        - It could arise a new round of paxos or just learned a new leader!!
        - We should also mentioned here that if the leader find itself timeout,
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
        - Though it may somehow sacrifice a little of liveness properties, but is easy to implement
        - In fact, the instance can be stopped if
            - there is a majority of acceptor accepted a proposal number
            - even meantime some proposer may propose new number
        - In our simplified case, the learner may only maintain this new number's accepted infos
        - which may influence liveness somehow.
        - But we do an optimization by delaying the updating infos until received new ACCEPTED
        - rather than the PREPARE!

    *